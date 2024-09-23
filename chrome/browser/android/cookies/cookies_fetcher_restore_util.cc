// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/cookies/cookies_fetcher_restore_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace cookie_fetcher_restore_util {

namespace {

// Let's monitor just the success/failure rate of restoring cookies so we have
// an easy metric to check if there are future issues.
void TriedToRestoreCookieMetric(bool success) {
  base::UmaHistogramBoolean("Cookie.AndroidOTRRestore", success);
}

}  // namespace

// Returns the cookie service at the client end of the mojo pipe.
network::mojom::CookieManager* GetCookieServiceClient(Profile* profile) {
  return profile->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

void CookiesFetcherRestoreCookiesImpl(
    JNIEnv* env,
    Profile* profile,
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
    jint source_port,
    jint source_type) {
  CHECK(profile->IsOffTheRecord());
  std::string domain_str(base::android::ConvertJavaStringToUTF8(env, domain));
  std::string path_str(base::android::ConvertJavaStringToUTF8(env, path));

  std::string top_level_site =
      base::android::ConvertJavaStringToUTF8(env, partition_key);
  // TODO (crbug.com/326605834) Once ancestor chain bit changes are
  // implemented update this method utilize the ancestor bit.
  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      serialized_cookie_partition_key = net::CookiePartitionKey::FromStorage(
          top_level_site, /*has_cross_site_ancestor=*/true);
  if (!serialized_cookie_partition_key.has_value()) {
    TriedToRestoreCookieMetric(/*success=*/false);
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
          static_cast<net::CookieSourceScheme>(source_scheme), source_port,
          static_cast<net::CookieSourceType>(source_type));
  // FromStorage() uses a less strict version of IsCanonical(), we need to check
  // the stricter version as well here. This is safe because this function is
  // only used for incognito cookies which don't survive Chrome updates and
  // therefore should never be the "older" less strict variety.
  if (!cookie || !cookie->IsCanonical()) {
    TriedToRestoreCookieMetric(/*success=*/false);
    return;
  }

  // Fetch cookies all-inclusive as we are doing so for the OTR profile.
  GetCookieServiceClient(profile)->SetCanonicalCookie(
      *cookie,
      net::cookie_util::CookieDomainAndPathToURL(
          domain_str, path_str,
          static_cast<net::CookieSourceScheme>(source_scheme)),
      net::CookieOptions::MakeAllInclusive(),
      network::mojom::CookieManager::SetCanonicalCookieCallback());
  TriedToRestoreCookieMetric(/*success=*/true);
}

}  // namespace cookie_fetcher_restore_util
