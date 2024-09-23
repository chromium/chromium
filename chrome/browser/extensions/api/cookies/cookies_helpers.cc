// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements common functionality for the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"

#include <stddef.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/cookies.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "url/gurl.h"

using extensions::api::cookies::Cookie;
using extensions::api::cookies::CookieStore;

namespace GetAll = extensions::api::cookies::GetAll;

namespace extensions {

namespace {

constexpr char kIdKey[] = "id";
constexpr char kTabIdsKey[] = "tabIds";

void AppendCookieToVectorIfMatchAndHasHostPermission(
    const net::CanonicalCookie cookie,
    GetAll::Params::Details* details,
    const Extension* extension,
    std::vector<Cookie>* match_vector,
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection) {
  // Ignore any cookie whose domain doesn't match the extension's
  // host permissions.
  GURL cookie_domain_url = cookies_helpers::GetURLFromCanonicalCookie(cookie);
  if (!extension->permissions_data()->HasHostPermission(cookie_domain_url))
    return;
  // Filter the cookie using the match filter.
  cookies_helpers::MatchFilter filter(details);
  // There is an edge case where a getAll call that contains a
  // partition key parameter but no top_level_site parameter results in a
  // return of partitioned and non-partitioned cookies. To ensure this is
  // handled correctly, the CookiePartitionKeyCollection value is set
  filter.SetCookiePartitionKeyCollection(cookie_partition_key_collection);
  if (filter.MatchesCookie(cookie)) {
    match_vector->push_back(
        cookies_helpers::CreateCookie(cookie, *details->store_id));
  }
}

}  // namespace

namespace cookies_helpers {

static const char kOriginalProfileStoreId[] = "0";
static const char kOffTheRecordProfileStoreId[] = "1";

Profile* ChooseProfileFromStoreId(const std::string& store_id,
                                  Profile* profile,
                                  bool include_incognito) {
  DCHECK(profile);
  bool allow_original = !profile->IsOffTheRecord();
  bool allow_incognito = profile->IsOffTheRecord() ||
                         (include_incognito && profile->HasPrimaryOTRProfile());
  if (store_id == kOriginalProfileStoreId && allow_original)
    return profile->GetOriginalProfile();
  if (store_id == kOffTheRecordProfileStoreId && allow_incognito)
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  return nullptr;
}

const char* GetStoreIdFromProfile(Profile* profile) {
  DCHECK(profile);
  return profile->IsOffTheRecord() ?
      kOffTheRecordProfileStoreId : kOriginalProfileStoreId;
}

Cookie CreateCookie(const net::CanonicalCookie& canonical_cookie,
                    const std::string& store_id) {
  Cookie cookie;
  // A cookie is a raw byte sequence. By explicitly parsing it as UTF-8, we
  // apply error correction, so the string can be safely passed to the renderer.
  cookie.name = base::UTF16ToUTF8(base::UTF8ToUTF16(canonical_cookie.Name()));
  cookie.value = base::UTF16ToUTF8(base::UTF8ToUTF16(canonical_cookie.Value()));
  cookie.domain = canonical_cookie.Domain();
  cookie.host_only =
      net::cookie_util::DomainIsHostOnly(canonical_cookie.Domain());
  // A non-UTF8 path is invalid, so we just replace it with an empty string.
  cookie.path = base::IsStringUTF8(canonical_cookie.Path())
                    ? canonical_cookie.Path()
                    : std::string();
  cookie.secure = canonical_cookie.SecureAttribute();
  cookie.http_only = canonical_cookie.IsHttpOnly();

  switch (canonical_cookie.SameSite()) {
    case net::CookieSameSite::NO_RESTRICTION:
      cookie.same_site = api::cookies::SameSiteStatus::kNoRestriction;
      break;
    case net::CookieSameSite::LAX_MODE:
      cookie.same_site = api::cookies::SameSiteStatus::kLax;
      break;
    case net::CookieSameSite::STRICT_MODE:
      cookie.same_site = api::cookies::SameSiteStatus::kStrict;
      break;
    case net::CookieSameSite::UNSPECIFIED:
      cookie.same_site = api::cookies::SameSiteStatus::kUnspecified;
      break;
  }

  cookie.session = !canonical_cookie.IsPersistent();
  if (canonical_cookie.IsPersistent()) {
    double expiration_date =
        canonical_cookie.ExpiryDate().InSecondsFSinceUnixEpoch();
    if (canonical_cookie.ExpiryDate().is_max() ||
        !std::isfinite(expiration_date)) {
      expiration_date = std::numeric_limits<double>::max();
    }
    cookie.expiration_date = expiration_date;
  }
  cookie.store_id = store_id;

  if (canonical_cookie.PartitionKey()) {
    base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            net::CookiePartitionKey::Serialize(canonical_cookie.PartitionKey());
    CHECK(serialized_partition_key.has_value());
    cookie.partition_key = extensions::api::cookies::CookiePartitionKey();
    cookie.partition_key->top_level_site =
        serialized_partition_key->TopLevelSite();
    cookie.partition_key->has_cross_site_ancestor =
        serialized_partition_key->has_cross_site_ancestor();
  }
  return cookie;
}

CookieStore CreateCookieStore(Profile* profile, base::Value::List tab_ids) {
  DCHECK(profile);
  base::Value::Dict dict;
  dict.Set(kIdKey, GetStoreIdFromProfile(profile));
  dict.Set(kTabIdsKey, std::move(tab_ids));

  auto cookie_store = CookieStore::FromValue(dict);
  CHECK(cookie_store);
  return std::move(cookie_store).value();
}

void GetCookieListFromManager(
    network::mojom::CookieManager* manager,
    const GURL& url,
    const net::CookiePartitionKeyCollection& partition_key_collection,
    network::mojom::CookieManager::GetCookieListCallback callback) {
  manager->GetCookieList(url, net::CookieOptions::MakeAllInclusive(),
                         partition_key_collection, std::move(callback));
}

void GetAllCookiesFromManager(
    network::mojom::CookieManager* manager,
    network::mojom::CookieManager::GetAllCookiesCallback callback) {
  manager->GetAllCookies(std::move(callback));
}

GURL GetURLFromCanonicalCookie(const net::CanonicalCookie& cookie) {
  // This is only ever called for CanonicalCookies that have come from a
  // CookieStore, which means they should not have an empty domain. Only file
  // cookies are allowed to have empty domains, and those are only permitted on
  // Android, and hopefully not for much longer (see crbug.com/582985).
  DCHECK(!cookie.Domain().empty());

  return net::cookie_util::CookieOriginToURL(cookie.Domain(),
                                             cookie.SecureAttribute());
}

void AppendMatchingCookiesFromCookieListToVector(
    const net::CookieList& all_cookies,
    GetAll::Params::Details* details,
    const Extension* extension,
    std::vector<Cookie>* match_vector,
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection) {
  for (const net::CanonicalCookie& cookie : all_cookies) {
    AppendCookieToVectorIfMatchAndHasHostPermission(
        cookie, details, extension, match_vector,
        cookie_partition_key_collection);
  }
}

void AppendMatchingCookiesFromCookieAccessResultListToVector(
    const net::CookieAccessResultList& all_cookies_with_access_result,
    GetAll::Params::Details* details,
    const Extension* extension,
    std::vector<Cookie>* match_vector) {
  for (const net::CookieWithAccessResult& cookie_with_access_result :
       all_cookies_with_access_result) {
    const net::CanonicalCookie& cookie = cookie_with_access_result.cookie;
    AppendCookieToVectorIfMatchAndHasHostPermission(
        cookie, details, extension, match_vector,
        CookiePartitionKeyCollectionFromApiPartitionKey(
            details->partition_key));
  }
}

void AppendToTabIdList(WindowController* window, base::Value::List& tab_ids) {
  for (int i = 0; i < window->GetTabCount(); ++i) {
    tab_ids.Append(ExtensionTabUtil::GetTabId(window->GetWebContentsAt(i)));
  }
}

base::expected<bool, std::string> CalculateHasCrossSiteAncestor(
    const std::string& url_string,
    std::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key) {
  // Can not calculate hasCrossSiteAncestor for a key that is not present or
  // does not have a top_level_site.
  CHECK(partition_key.has_value() ||
        !partition_key->top_level_site.has_value());

  if (partition_key->has_cross_site_ancestor.has_value()) {
    return base::ok(partition_key->has_cross_site_ancestor.value());
  }

  // Empty top_level_site indicates an unpartitioned cookie which always has a
  // hasCrossSiteAncestor of false.
  if (partition_key->top_level_site.value().empty()) {
    return base::ok(false);
  }

  GURL url = GURL(url_string);
  if (!url.is_valid()) {
    return base::unexpected("Invalid url_string.");
  }

  GURL top_level_site = GURL(partition_key->top_level_site.value());
  if (!top_level_site.is_valid()) {
    return base::unexpected(
        "Invalid value for CookiePartitionKey.topLevelSite.");
  }

  return !net::SiteForCookies::FromUrl(url).IsFirstParty(top_level_site);
}

bool ValidateCrossSiteAncestor(
    const std::string& url_string,
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key,
    std::string* error_out) {
  // Unpartitioned cookie has no value to validate.
  if (!partition_key.has_value()) {
    return true;
  }

  // A has_cross_site_ancestor value cannot be valid without a top_level_site.
  if (!partition_key->top_level_site.has_value()) {
    if (partition_key->has_cross_site_ancestor.has_value()) {
      *error_out =
          "CookiePartitionKey.topLevelSite is not present when "
          "CookiePartitionKey.hasCrossSiteAncestor is present.";
      return false;
    }
    return true;
  }

  // Empty top_level_site indicates an unpartitioned cookie which must have a
  // value of false.
  if (partition_key->top_level_site.value().empty()) {
    if (partition_key->has_cross_site_ancestor.has_value() &&
        partition_key->has_cross_site_ancestor.value()) {
      *error_out = "CookiePartitionKey.hasCrossSiteAncestor is invalid.";
      return false;
    }
    return true;
  }

  if (!partition_key->has_cross_site_ancestor.has_value()) {
    *error_out =
        "Can not validate an empty value for "
        "hasCrossSiteAncestor.";
    return false;
  }

  GURL url = GURL(url_string);
  if (!url.is_valid()) {
    *error_out = "Invalid url_string.";
    return false;
  }

  GURL top_level_site = GURL(partition_key->top_level_site.value());
  if (!top_level_site.is_valid()) {
    *error_out = "Invalid value for CookiePartitionKey.topLevelSite.";
    return false;
  }

  // has_cross_site_ancestor can not be false if url and top_level_site aren't
  // first party.
  if (!net::SiteForCookies::FromUrl(GURL(url)).IsFirstParty(top_level_site) &&
      !partition_key->has_cross_site_ancestor.value()) {
    *error_out =
        "partitionKey has a first party value for hasCrossSiteAncestor "
        "when the url and the topLevelSite are not first party.";
    return false;
  }
  return true;
}

base::expected<std::optional<net::CookiePartitionKey>, std::string>
ToNetCookiePartitionKey(
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key) {
  if (!partition_key.has_value()) {
    return std::nullopt;
  }

  if (!partition_key->top_level_site.has_value()) {
    if (partition_key->has_cross_site_ancestor.has_value()) {
      return base::unexpected(
          "CookiePartitionKey.topLevelSite unexpectedly not present.");
    }
    return base::ok(std::nullopt);
  }

  if (partition_key->top_level_site->empty()) {
    if (partition_key->has_cross_site_ancestor.has_value() &&
        partition_key->has_cross_site_ancestor.value()) {
      return base::unexpected(
          "partitionKey with empty topLevelSite unexpectedly has a cross-site "
          "ancestor value of true.");
    }
    return base::ok(std::nullopt);
  }

  base::expected<net::CookiePartitionKey, std::string> key =
      net::CookiePartitionKey::FromUntrustedInput(
          partition_key->top_level_site.value(),
          partition_key->has_cross_site_ancestor.value_or(true));
  if (!key.has_value()) {
    return base::unexpected(key.error());
  }

  // Record 'well formatted' uma here so that we count only coercible
  // partition keys.
  base::UmaHistogramBoolean(
      "Extensions.CookieAPIPartitionKeyWellFormatted",
      net::SchemefulSite::Deserialize(partition_key->top_level_site.value())
              .Serialize() == partition_key->top_level_site.value());
  return key;
}

bool CookieMatchesPartitionKeyCollection(
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
    const net::CanonicalCookie& cookie) {
  if (!cookie.IsPartitioned()) {
    return cookie_partition_key_collection.ContainsAllKeys() ||
           cookie_partition_key_collection.IsEmpty();
  }
  return cookie_partition_key_collection.Contains(*cookie.PartitionKey());
}

bool CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        api_partition_key,
    const std::optional<net::CookiePartitionKey>& net_partition_key) {
  if (!net_partition_key.has_value()) {
    // Check to see if the api_partition_key is also unpartitioned.
    return !api_partition_key.has_value() ||
           !api_partition_key->top_level_site.has_value() ||
           api_partition_key.value().top_level_site.value().empty();
  }

  if (api_partition_key->has_cross_site_ancestor.has_value() &&
      net_partition_key->IsThirdParty() !=
          api_partition_key->has_cross_site_ancestor.value()) {
    return false;
  }

  // If both keys are present, they both must be serializable for a match.
  if (!net_partition_key->IsSerializeable() ||
      !api_partition_key->top_level_site.has_value()) {
    return false;
  }

  base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                 std::string>
      net_serialized_result =
          net::CookiePartitionKey::Serialize(net_partition_key);

  if (!net_serialized_result.has_value()) {
    return false;
  }

  return net_serialized_result->TopLevelSite() ==
         api_partition_key->top_level_site.value();
}

net::CookiePartitionKeyCollection
CookiePartitionKeyCollectionFromApiPartitionKey(
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key) {
  if (!partition_key) {
    return net::CookiePartitionKeyCollection();
  }

  if (!partition_key->top_level_site) {
    return net::CookiePartitionKeyCollection::ContainsAll();
  }

  if (partition_key->top_level_site.value().empty()) {
    return net::CookiePartitionKeyCollection();
  }

  if (!partition_key->has_cross_site_ancestor.has_value()) {
    return net::CookiePartitionKeyCollection::MatchesSite(
        net::SchemefulSite(GURL(partition_key->top_level_site.value())));
  }

  base::expected<net::CookiePartitionKey, std::string> net_partition_key =
      net::CookiePartitionKey::FromUntrustedInput(
          partition_key->top_level_site.value(),
          partition_key->has_cross_site_ancestor.value());
  if (!net_partition_key.has_value()) {
    return net::CookiePartitionKeyCollection();
  }

  return net::CookiePartitionKeyCollection::FromOptional(
      net_partition_key.value());
}

MatchFilter::MatchFilter(GetAll::Params::Details* details) : details_(details) {
  DCHECK(details_);
}

bool MatchFilter::MatchesCookie(
    const net::CanonicalCookie& cookie) {
  if (!CookieMatchesPartitionKeyCollection(cookie_partition_key_collection_,
                                           cookie)) {
    return false;
  }
  // Confirm there's at least one parameter to check.
  if (!details_->name && !details_->domain && !details_->path &&
      !details_->secure && !details_->session && !details_->partition_key) {
    return true;
  }

  if (details_->name && *details_->name != cookie.Name())
    return false;

  if (!MatchesDomain(cookie.Domain()))
    return false;

  if (details_->path && *details_->path != cookie.Path())
    return false;

  if (details_->secure && *details_->secure != cookie.SecureAttribute()) {
    return false;
  }

  if (details_->session && *details_->session != !cookie.IsPersistent())
    return false;

  return true;
}

void MatchFilter::SetCookiePartitionKeyCollection(
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection) {
  cookie_partition_key_collection_ = cookie_partition_key_collection;
}

bool MatchFilter::MatchesDomain(const std::string& domain) {
  if (!details_->domain)
    return true;

  // Add a leading '.' character to the filter domain if it doesn't exist.
  if (net::cookie_util::DomainIsHostOnly(*details_->domain))
    details_->domain->insert(0, ".");

  std::string sub_domain(domain);
  // Strip any leading '.' character from the input cookie domain.
  if (!net::cookie_util::DomainIsHostOnly(sub_domain))
    sub_domain = sub_domain.substr(1);

  // Now check whether the domain argument is a subdomain of the filter domain.
  for (sub_domain.insert(0, ".");
       sub_domain.length() >= details_->domain->length();) {
    if (sub_domain == *details_->domain)
      return true;
    const size_t next_dot = sub_domain.find('.', 1);  // Skip over leading dot.
    sub_domain.erase(0, next_dot);
  }
  return false;
}

}  // namespace cookies_helpers
}  // namespace extensions
