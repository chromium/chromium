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
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/cookies/cookies_api_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/cookies.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "cookies_helpers.h"
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
  cookie.secure = canonical_cookie.IsSecure();
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
    CHECK(canonical_cookie.PartitionKey()->IsSerializeable());
    std::string top_level_site;
    CHECK_EQ(base::FeatureList::IsEnabled(net::features::kPartitionedCookies),
             net::CookiePartitionKey::Serialize(canonical_cookie.PartitionKey(),
                                                top_level_site));

    cookie.partition_key = extensions::api::cookies::CookiePartitionKey();
    cookie.partition_key->top_level_site = top_level_site;
  }
  return cookie;
}

CookieStore CreateCookieStore(Profile* profile, base::Value::List tab_ids) {
  DCHECK(profile);
  base::Value::Dict dict;
  dict.Set(cookies_api_constants::kIdKey, GetStoreIdFromProfile(profile));
  dict.Set(cookies_api_constants::kTabIdsKey, std::move(tab_ids));

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
                                             cookie.IsSecure());
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
        net::CookiePartitionKeyCollection());
  }
}

void AppendToTabIdList(Browser* browser, base::Value::List& tab_ids) {
  DCHECK(browser);
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_ids.Append(ExtensionTabUtil::GetTabId(tab_strip->GetWebContentsAt(i)));
  }
}

bool ValidateCookieApiPartitionKey(
    const absl::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key,
    absl::optional<net::CookiePartitionKey>& net_partition_key,
    std::string& error_message) {
  if (partition_key &&
      base::FeatureList::IsEnabled(net::features::kPartitionedCookies)) {
    if (partition_key->top_level_site &&
        !net::CookiePartitionKey::Deserialize(
            partition_key->top_level_site.value(), net_partition_key)) {
      error_message = "Invalid format for partitionKey.topLevelSite.";
      return false;
    }
  }
  return true;
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

bool CookieMatchesPartitionKeyInDetails(
    const absl::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key,
    const net::CanonicalCookie& cookie) {
  if (!partition_key) {
    return !cookie.IsPartitioned();
  }

  if (cookie.IsPartitioned() && !cookie.PartitionKey()->IsSerializeable()) {
    return false;
  }

  std::string serialized_partition_key;
  return net::CookiePartitionKey::Serialize(cookie.PartitionKey(),
                                            serialized_partition_key) &&
         serialized_partition_key == partition_key->top_level_site.value();
}

net::CookiePartitionKeyCollection
CookiePartitionKeyCollectionFromApiPartitionKey(
    const absl::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key) {
  if (!partition_key) {
    return net::CookiePartitionKeyCollection();
  }

  if (!partition_key->top_level_site) {
    return net::CookiePartitionKeyCollection::ContainsAll();
  }

  absl::optional<net::CookiePartitionKey> net_partition_key;
  if (!net::CookiePartitionKey::Deserialize(
          partition_key->top_level_site.value(), net_partition_key)) {
    return net::CookiePartitionKeyCollection();
  }

  return net::CookiePartitionKeyCollection::FromOptional(net_partition_key);
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

  if (details_->secure && *details_->secure != cookie.IsSecure())
    return false;

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
