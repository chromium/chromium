// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_guest_url.h"

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace geic {

namespace {

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

GURL GetGeicSkillsUrlForGuestUrl(const GURL& guest_url) {
  GURL url = guest_url;
  if (!url.is_valid()) {
    return GURL();
  }

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
