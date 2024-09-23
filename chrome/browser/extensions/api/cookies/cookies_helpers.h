// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines common functionality used by the implementation of the Chrome
// Extensions Cookies API implemented in
// chrome/browser/extensions/api/cookies/cookies_api.cc. This separate interface
// exposes pieces of the API implementation mainly for unit testing purposes.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_HELPERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/common/extensions/api/cookies.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class Profile;

namespace net {
class CanonicalCookie;
}

namespace extensions {

class Extension;
class WindowController;

namespace cookies_helpers {

// Returns either the original profile or the incognito profile, based on the
// given store ID.  Returns NULL if the profile doesn't exist or is not allowed
// (e.g. if incognito mode is not enabled for the extension).
Profile* ChooseProfileFromStoreId(const std::string& store_id,
                                  Profile* profile,
                                  bool include_incognito);

// Returns the store ID for a particular user profile.
const char* GetStoreIdFromProfile(Profile* profile);

// Constructs a new Cookie object representing a cookie as defined by the
// cookies API.
api::cookies::Cookie CreateCookie(const net::CanonicalCookie& cookie,
                                  const std::string& store_id);

// Constructs a new CookieStore object as defined by the cookies API.
api::cookies::CookieStore CreateCookieStore(Profile* profile,
                                            base::Value::List tab_ids);

// Dispatch a request to the CookieManager for cookies associated with
// |url| and |partition_key_collection|.
void GetCookieListFromManager(
    network::mojom::CookieManager* manager,
    const GURL& url,
    const net::CookiePartitionKeyCollection& partition_key_collection,
    network::mojom::CookieManager::GetCookieListCallback callback);

// Dispatch a request to the CookieManager for all cookies.
void GetAllCookiesFromManager(
    network::mojom::CookieManager* manager,
    network::mojom::CookieManager::GetAllCookiesCallback callback);

// Constructs a URL from a cookie's information for use in checking
// a cookie against the extension's host permissions. The Secure
// property of the cookie defines the URL scheme, and the cookie's
// domain becomes the URL host.
GURL GetURLFromCanonicalCookie(
    const net::CanonicalCookie& cookie);

// Looks through all cookies in the given cookie store, and appends to the
// match vector all the cookies that both match the given URL and cookie details
// and are allowed by extension host permissions.
void AppendMatchingCookiesFromCookieListToVector(
    const net::CookieList& all_cookies,
    api::cookies::GetAll::Params::Details* details,
    const Extension* extension,
    std::vector<api::cookies::Cookie>* match_vector,
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection);

// Same as above except takes a CookieAccessResultList (and ignores the access
// results).
void AppendMatchingCookiesFromCookieAccessResultListToVector(
    const net::CookieAccessResultList& all_cookies_with_access_result,
    api::cookies::GetAll::Params::Details* details,
    const Extension* extension,
    std::vector<api::cookies::Cookie>* match_vector);

// Appends the IDs of all tabs belonging to the given browser to the
// given list.
void AppendToTabIdList(WindowController* window, base::Value::List& tab_ids);

// The extensions API allows the caller to provide an incomplete
// partitionKey that does not contain a hasCrossSiteAncestor value. If the key
// is incomplete, this method calculates the value for the hasCrossSiteAncestor
// otherwise the existing value for hasCrossSiteAncestor is returned.
base::expected<bool, std::string> CalculateHasCrossSiteAncestor(
    const std::string& url_string,
    std::optional<extensions::api::cookies::CookiePartitionKey>& partition_key);

// Checks to make sure the has_cross_site_ancestor value is valid.
// Returns false and populates error_out string on failure.
bool ValidateCrossSiteAncestor(
    const std::string& url_string,
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key,
    std::string* error_out);

// Checks to make sure that the partition_key provided is valid and creates a
// net::CookiePartitionKey from it.
base::expected<std::optional<net::CookiePartitionKey>, std::string>
ToNetCookiePartitionKey(
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key);

// Returns empty collection if no partition_key.
// Returns CookiePartitionKeyCollection::ContainsAll() if top_level_site has no
// value. Returns CookiePartitionKeyCollection::FromOptional() if partition_key
// and top_level_site are both present.
//
// If no value for partition_key->has_cross_site_ancestor is provided, keys with
// both values will be used to create a collection.
net::CookiePartitionKeyCollection
CookiePartitionKeyCollectionFromApiPartitionKey(
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        partition_key);

// Returns true for unpartitioned cookies if the collection is empty.
// Otherwise returns true if the collection contains the cookie's partition key.
bool CookieMatchesPartitionKeyCollection(
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
    const net::CanonicalCookie& cookie);

// Returns true if the top_level_site values match or both optionals do not
// contain a value. For match to occur both partition keys must be serializable
// if they are present.
bool CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
    const std::optional<extensions::api::cookies::CookiePartitionKey>&
        api_partition_key,
    const std::optional<net::CookiePartitionKey>& net_partition_key);

// A class representing the cookie filter parameters passed into
// cookies.getAll().
// This class is essentially a convenience wrapper for the details dictionary
// passed into the cookies.getAll() API by the user. If the dictionary contains
// no filter parameters, the MatchFilter will always trivially
// match all cookies.
class MatchFilter {
 public:
  // Takes the details dictionary argument given by the user as input.
  // This class does not take ownership of the lifetime of the Details
  // object.
  explicit MatchFilter(api::cookies::GetAll::Params::Details* details);

  // Returns true if the given cookie matches the properties in the match
  // filter.
  bool MatchesCookie(const net::CanonicalCookie& cookie);

  // Sets the value for cookie_partition_key_collection_
  void SetCookiePartitionKeyCollection(
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection);

 private:
  // Returns true if the given cookie domain string matches the filter's
  // domain. Any cookie domain which is equal to or is a subdomain of the
  // filter's domain will be matched; leading '.' characters indicating
  // host-only domains have no meaning in the match filter domain (for
  // instance, a match filter domain of 'foo.bar.com' will be treated the same
  // as '.foo.bar.com', and both will match cookies with domain values of
  // 'foo.bar.com', '.foo.bar.com', and 'baz.foo.bar.com'.
  bool MatchesDomain(const std::string& domain);

  raw_ptr<api::cookies::GetAll::Params::Details> details_;
  net::CookiePartitionKeyCollection cookie_partition_key_collection_;
};

}  // namespace cookies_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_HELPERS_H_
