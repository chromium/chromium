// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/cookies/cookies_fetcher_restore_util.h"

#include "base/time/time.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace cookie_fetcher_restore_util {

// Returns the cookie service at the client end of the mojo pipe.
network::mojom::CookieManager* GetCookieServiceClient() {
  // Since restoring Incognito CCT session from cookies is not supported, it is
  // safe to use the primary OTR profile here.
  return ProfileManager::GetPrimaryUserProfile()
      ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

void CookiesFetcherRestoreCookiesImpl(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jstring>& name,
    const jni_zero::JavaParamRef<jstring>& value,
    const jni_zero::JavaParamRef<jstring>& domain,
    const jni_zero::JavaParamRef<jstring>& path,
    jlong creation,
    jlong expiration,
    jlong last_access,
    jlong last_update,
    jboolean secure,
    jboolean httponly,
    jint same_site,
    jint priority,
    const jni_zero::JavaParamRef<jstring>& partition_key,
    jint source_scheme,
    jint source_port) {
  if (!ProfileManager::GetPrimaryUserProfile()->HasPrimaryOTRProfile()) {
    return;  // Don't create it. There is nothing to do.
  }

  std::string domain_str(base::android::ConvertJavaStringToUTF8(env, domain));
  std::string path_str(base::android::ConvertJavaStringToUTF8(env, path));

  std::string top_level_site =
      base::android::ConvertJavaStringToUTF8(env, partition_key);
  // TODO (crbug.com/326605834) Once ancestor chain bit changes are
  // implemented update this method utilize the ancestor bit.
  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      serialized_cookie_partition_key = net::CookiePartitionKey::FromStorage(
          top_level_site);
  if (!serialized_cookie_partition_key.has_value()) {
    return;
  }

  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::FromStorage(
          base::android::ConvertJavaStringToUTF8(env, name),
          base::android::ConvertJavaStringToUTF8(env, value), domain_str,
          path_str,
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(creation)),
          base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(expiration)),
          base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(last_access)),
          base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(last_update)),
          secure, httponly, static_cast<net::CookieSameSite>(same_site),
          static_cast<net::CookiePriority>(priority),
          serialized_cookie_partition_key.value(),
          static_cast<net::CookieSourceScheme>(source_scheme), source_port);
  // FromStorage() uses a less strict version of IsCanonical(), we need to check
  // the stricter version as well here. This is safe because this function is
  // only used for incognito cookies which don't survive Chrome updates and
  // therefore should never be the "older" less strict variety.
  if (!cookie || !cookie->IsCanonical()) {
    return;
  }

  // Assume HTTPS - since the cookies are being restored from another store,
  // they have already gone through the strict secure check.
  //
  // Similarly, permit samesite cookies to be imported.
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  options.set_do_not_update_access_time();
  GetCookieServiceClient()->SetCanonicalCookie(
      *cookie,
      net::cookie_util::CookieDomainAndPathToURL(
          domain_str, path_str,
          static_cast<net::CookieSourceScheme>(source_scheme)),
      options, network::mojom::CookieManager::SetCanonicalCookieCallback());
}

}  // namespace cookie_fetcher_restore_util
