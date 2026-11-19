// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_guest_url.h"

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace geic {

namespace {

struct UnwrappedGuestUrl {
  GURL url;
  std::string workforce_provider;
};

// Guest URLs must use HTTPS and must not contain embedded credentials.
bool IsSecureGuestUrl(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme) &&
         !url.has_username() && !url.has_password();
}

// Policy guest URLs must use a production Gemini Enterprise host.
bool IsValidPolicyGuestUrl(const GURL& url) {
  return IsSecureGuestUrl(url) && IsAllowedGeminiEnterpriseHost(url);
}

// Finch guest URLs may also use a pre-production or internal corp Gemini
// Enterprise host.
bool IsValidFinchGuestUrl(const GURL& url) {
  return IsSecureGuestUrl(url) &&
         (IsAllowedGeminiEnterpriseHost(url) ||
          IsAllowedGeminiEnterprisePreProductionHost(url));
}

// Rewrites standalone Gemini Enterprise web app URLs (`/home/cid/<configId>` or
// `/<location>/home/cid/<configId>`) to the side-panel endpoint
// (`/side-panel?configId=<configId>` or
// `/side-panel?configId=<configId>&location=<location>`) at consumption time
// rather than in the policy handler so the stored policy pref retains the raw
// web application URL and CLI/Finch overrides are canonicalized as well.
GURL CanonicalizeGuestUrl(const GURL& input_url) {
  if (!input_url.is_valid()) {
    return GURL();
  }

  static constexpr std::string_view kHomeCidMarker = "/home/cid/";
  std::string_view path = input_url.path();
  std::string_view location;
  std::string_view config_id;

  size_t marker_pos = path.find(kHomeCidMarker);
  if (marker_pos == 0) {
    config_id = path.substr(kHomeCidMarker.size());
  } else if (marker_pos != std::string_view::npos && marker_pos > 1) {
    // Expected path: `/<location>/home/cid/...`.
    std::string_view prefix = path.substr(1, marker_pos - 1);
    if (prefix.find('/') == std::string_view::npos) {
      location = prefix;
      config_id = path.substr(marker_pos + kHomeCidMarker.size());
    }
  }

  if (config_id.ends_with("/")) {
    config_id.remove_suffix(1);
  }
  if (config_id.empty()) {
    return input_url;
  }

  GURL::Replacements replacements;
  replacements.SetPathStr("/side-panel");
  GURL result = net::AppendOrReplaceQueryParameter(
      input_url.ReplaceComponents(replacements), "configId", config_id);
  if (!location.empty()) {
    result = net::AppendOrReplaceQueryParameter(result, "location", location);
  }
  return result;
}

std::optional<UnwrappedGuestUrl> ToUnwrappedGuestUrl(
    base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
        parsed) {
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  return UnwrappedGuestUrl{
      .url = std::move(parsed->inner_url),
      .workforce_provider = std::move(parsed->workforce_provider),
  };
}

// Policy guest URLs must use a production Gemini Enterprise host or a
// production BYOID Auth Portal URL wrapping one.
std::optional<UnwrappedGuestUrl> UnwrapPolicyGuestUrl(const GURL& url) {
  if (IsValidPolicyGuestUrl(url)) {
    return UnwrappedGuestUrl{.url = url};
  }
  return ToUnwrappedGuestUrl(ParseWorkforceAuthPortalUrl(url));
}

// Finch guest URLs may also use a pre-production Gemini Enterprise host or a
// pre-production BYOID Auth Portal URL wrapping one.
std::optional<UnwrappedGuestUrl> UnwrapFinchGuestUrl(const GURL& url) {
  if (std::optional<UnwrappedGuestUrl> policy_url = UnwrapPolicyGuestUrl(url)) {
    return policy_url;
  }
  if (IsValidFinchGuestUrl(url)) {
    return UnwrappedGuestUrl{.url = url};
  }
  return ToUnwrappedGuestUrl(ParsePreProductionWorkforceAuthPortalUrl(url));
}

GURL FinalizeGuestUrl(const UnwrappedGuestUrl& unwrapped) {
  GURL guest_url = CanonicalizeGuestUrl(unwrapped.url);
  if (!unwrapped.workforce_provider.empty()) {
    guest_url = net::AppendOrReplaceQueryParameter(
        guest_url, kWorkforceProviderQueryParam, unwrapped.workforce_provider);
  }
  return guest_url;
}

std::string GetGeicSkillsPath(const GURL& url,
                              std::string_view config_id_query) {
  // Transforms `/side-panel?configId=<configId>` into
  // `/home/cid/<configId>/r/skills`.
  if (!config_id_query.empty()) {
    return base::StrCat({"/home/cid/", config_id_query, "/r/skills"});
  }

  // Transforms `<prefix>/cid/<configId>/side-panel` into
  // `<prefix>/cid/<configId>/customize/skills` (Gemini at Work URLs).
  const std::string path(url.path());
  static re2::LazyRE2 kCidPrefix = {R"(^(.*?/cid/[^/]+))"};
  std::string_view cid_prefix;
  if (re2::RE2::PartialMatch(path, *kCidPrefix, &cid_prefix)) {
    return base::StrCat({cid_prefix, "/customize/skills"});
  }

  // Default mount path prefix for known Gemini at Work hosts.
  constexpr auto kGeminiAtWorkHostDefaultMounts =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          {"gemini.cloud.google", ""},
          {"gemini.google.com", "work"},
      });
  const std::string_view* default_mount =
      base::FindOrNull(kGeminiAtWorkHostDefaultMounts, url.host());
  // Transforms any other path into `/r/skills`.
  if (!default_mount) {
    return "/r/skills";
  }

  // Transforms a Gemini at Work path without a cid, e.g.
  // `/work/u/1/side-panel`, into its mount root, e.g. `/work/u/1`. The server
  // redirects the mount root to the user's instance.
  std::vector<std::string_view> segments = base::SplitStringPiece(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string_view> mount;
  auto it = segments.begin();
  std::string_view mount_prefix =
      it != segments.end() && *it == "work" ? *it++ : *default_mount;
  if (!mount_prefix.empty()) {
    mount.push_back(mount_prefix);
  }
  if (std::distance(it, segments.end()) >= 2 && *it == "u") {
    mount.insert(mount.end(), it, std::next(it, 2));
  }
  return base::StrCat({"/", base::JoinString(mount, "/")});
}

}  // namespace

// static
base::expected<GeicGuestUrl, GuestUrlError> GeicGuestUrl::FromSwitch(
    std::string_view spec,
    bool allow_any_http_url) {
  if (spec.empty()) {
    return base::unexpected(GuestUrlError::kUnset);
  }
  GURL url(spec);
  if (!url.is_valid() || url.has_username() || url.has_password()) {
    return base::unexpected(GuestUrlError::kInvalid);
  }
  if (std::optional<UnwrappedGuestUrl> unwrapped = UnwrapFinchGuestUrl(url)) {
    return GeicGuestUrl(FinalizeGuestUrl(*unwrapped), GuestUrlSource::kSwitch);
  }
  if (allow_any_http_url && url.SchemeIsHTTPOrHTTPS()) {
    return GeicGuestUrl(CanonicalizeGuestUrl(url), GuestUrlSource::kSwitch);
  }
  return base::unexpected(GuestUrlError::kInvalid);
}

// static
base::expected<GeicGuestUrl, GuestUrlError> GeicGuestUrl::FromPolicy(
    std::string_view spec) {
  if (spec.empty()) {
    return base::unexpected(GuestUrlError::kUnset);
  }
  if (std::optional<UnwrappedGuestUrl> unwrapped =
          UnwrapPolicyGuestUrl(GURL(spec))) {
    return GeicGuestUrl(FinalizeGuestUrl(*unwrapped), GuestUrlSource::kPolicy);
  }
  return base::unexpected(GuestUrlError::kInvalid);
}

// static
base::expected<GeicGuestUrl, GuestUrlError> GeicGuestUrl::FromFinch(
    std::string_view spec) {
  if (spec.empty()) {
    return base::unexpected(GuestUrlError::kUnset);
  }
  if (std::optional<UnwrappedGuestUrl> unwrapped =
          UnwrapFinchGuestUrl(GURL(spec))) {
    return GeicGuestUrl(FinalizeGuestUrl(*unwrapped), GuestUrlSource::kFinch);
  }
  return base::unexpected(GuestUrlError::kInvalid);
}

GeicGuestUrl::GeicGuestUrl(GURL url, GuestUrlSource source)
    : url_(std::move(url)), source_(source) {}

GeicGuestUrl::GeicGuestUrl(const GeicGuestUrl&) = default;
GeicGuestUrl& GeicGuestUrl::operator=(const GeicGuestUrl&) = default;
GeicGuestUrl::GeicGuestUrl(GeicGuestUrl&&) noexcept = default;
GeicGuestUrl& GeicGuestUrl::operator=(GeicGuestUrl&&) noexcept = default;
GeicGuestUrl::~GeicGuestUrl() = default;

GURL GeicGuestUrl::SkillsUrl() const {
  GURL url = url_;
  std::string config_id;
  net::GetValueForKeyInQuery(url, "configId", &config_id);
  url = net::AppendOrReplaceQueryParameter(url, "configId", std::nullopt);
  url = net::AppendOrReplaceQueryParameter(url, "location", std::nullopt);
  url = net::AppendOrReplaceQueryParameter(url, "hl", std::nullopt);
  url = net::AppendOrReplaceQueryParameter(url, kWorkforceProviderQueryParam,
                                           std::nullopt);

  std::string path = GetGeicSkillsPath(url, config_id);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  if (url.query().empty()) {
    replacements.ClearQuery();
  }
  return url.ReplaceComponents(replacements);
}

}  // namespace geic
