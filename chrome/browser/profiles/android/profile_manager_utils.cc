// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/profiles/android/jni_headers/ProfileManagerUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

void CommitPendingWritesForProfile(Profile* profile) {
  // These calls are asynchronous. They may not finish (and may not even
  // start!) before the Android OS kills our process. But we can't wait for them
  // to finish because blocking the UI thread is illegal.
  profile->GetPrefs()->CommitPendingWrite();
  profile->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->FlushCookieStore(
          network::mojom::CookieManager::FlushCookieStoreCallback());
  profile->ForEachLoadedStoragePartition(&content::StoragePartition::Flush);
}

void RemoveSessionCookiesForProfile(Profile* profile) {
  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->session_control =
      network::mojom::CookieDeletionSessionControl::SESSION_COOKIES;
  profile->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->DeleteCookies(std::move(filter),
                      network::mojom::CookieManager::DeleteCookiesCallback());
}

}  // namespace

static void JNI_ProfileManagerUtils_FlushPersistentDataForAllProfiles(
    JNIEnv* env) {
  base::ranges::for_each(
      g_browser_process->profile_manager()->GetLoadedProfiles(),
      CommitPendingWritesForProfile);

  if (g_browser_process->local_state())
    g_browser_process->local_state()->CommitPendingWrite();
}

static void JNI_ProfileManagerUtils_RemoveSessionCookiesForAllProfiles(
    JNIEnv* env) {
  base::ranges::for_each(
      g_browser_process->profile_manager()->GetLoadedProfiles(),
      RemoveSessionCookiesForProfile);
}
