// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {
const char kGlobalCookieSetURL[] = "chrome://cookieset";
}  // namespace

BrowsingDataCookieHelper::BrowsingDataCookieHelper(
    content::StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BrowsingDataCookieHelper::~BrowsingDataCookieHelper() {
}

void BrowsingDataCookieHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  storage_partition_->GetCookieManagerForBrowserProcess()->GetAllCookies(
      std::move(callback));
}

void BrowsingDataCookieHelper::DeleteCookie(
    const net::CanonicalCookie& cookie) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_->GetCookieManagerForBrowserProcess()
      ->DeleteCanonicalCookie(cookie, base::DoNothing());
}

CannedBrowsingDataCookieHelper::CannedBrowsingDataCookieHelper(
    content::StoragePartition* storage_partition)
    : BrowsingDataCookieHelper(storage_partition) {}

CannedBrowsingDataCookieHelper::~CannedBrowsingDataCookieHelper() {
  Reset();
}

void CannedBrowsingDataCookieHelper::AddReadCookies(
    const GURL& frame_url,
    const GURL& url,
    const net::CookieList& cookie_list) {
  for (const auto& add_cookie : cookie_list)
    AddCookie(frame_url, add_cookie);
}

void CannedBrowsingDataCookieHelper::AddChangedCookie(
    const GURL& frame_url,
    const GURL& url,
    const net::CanonicalCookie& cookie) {
  AddCookie(frame_url, cookie);
}

void CannedBrowsingDataCookieHelper::Reset() {
  origin_cookie_set_map_.clear();
}

bool CannedBrowsingDataCookieHelper::empty() const {
  for (const auto& pair : origin_cookie_set_map_) {
    if (!pair.second->empty())
      return false;
  }
  return true;
}


size_t CannedBrowsingDataCookieHelper::GetCookieCount() const {
  size_t count = 0;
  for (const auto& pair : origin_cookie_set_map_)
    count += pair.second->size();
  return count;
}

void CannedBrowsingDataCookieHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::CookieList cookie_list;
  for (const auto& pair : origin_cookie_set_map_) {
    cookie_list.insert(cookie_list.begin(), pair.second->begin(),
                       pair.second->end());
  }
  std::move(callback).Run(cookie_list);
}

void CannedBrowsingDataCookieHelper::DeleteCookie(
    const net::CanonicalCookie& cookie) {
  for (const auto& pair : origin_cookie_set_map_)
    DeleteMatchingCookie(cookie, pair.second.get());
  BrowsingDataCookieHelper::DeleteCookie(cookie);
}

bool CannedBrowsingDataCookieHelper::DeleteMatchingCookie(
    const net::CanonicalCookie& add_cookie,
    canonical_cookie::CookieHashSet* cookie_set) {
  return cookie_set->erase(add_cookie) > 0;
}

canonical_cookie::CookieHashSet* CannedBrowsingDataCookieHelper::GetCookiesFor(
    const GURL& first_party_origin) {
  std::unique_ptr<canonical_cookie::CookieHashSet>& entry =
      origin_cookie_set_map_[first_party_origin];
  if (entry)
    return entry.get();

  entry = std::make_unique<canonical_cookie::CookieHashSet>();
  return entry.get();
}

void CannedBrowsingDataCookieHelper::AddCookie(
    const GURL& frame_url,
    const net::CanonicalCookie& cookie) {
  // Storing cookies in separate cookie sets per frame origin makes the
  // GetCookieCount method count a cookie multiple times if it is stored in
  // multiple sets.
  // E.g. let "example.com" be redirected to "www.example.com". A cookie set
  // with the cookie string "A=B; Domain=.example.com" would be sent to both
  // hosts. This means it would be stored in the separate cookie sets for both
  // hosts ("example.com", "www.example.com"). The method GetCookieCount would
  // count this cookie twice. To prevent this, we us a single global cookie
  // set as a work-around to store all added cookies. Per frame URL cookie
  // sets are currently not used. In the future they will be used for
  // collecting cookies per origin in redirect chains.
  // TODO(markusheintz): A) Change the GetCookiesCount method to prevent
  // counting cookies multiple times if they are stored in multiple cookie
  // sets.  B) Replace the GetCookieFor method call below with:
  // "GetCookiesFor(frame_url.GetOrigin());"
  static const base::NoDestructor<GURL> origin_cookie_url(kGlobalCookieSetURL);
  canonical_cookie::CookieHashSet* cookie_set =
      GetCookiesFor(*origin_cookie_url);
  DeleteMatchingCookie(cookie, cookie_set);
  cookie_set->insert(cookie);
}
