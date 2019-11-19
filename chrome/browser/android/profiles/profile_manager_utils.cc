// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "chrome/android/public/profiles/jni_headers/ProfileManagerUtils_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

void FlushStoragePartition(content::StoragePartition* partition) {
  partition->Flush();
}

void CommitPendingWritesForProfile(Profile* profile) {
  // These calls are asynchronous. They may not finish (and may not even
  // start!) before the Android OS kills our process. But we can't wait for them
  // to finish because blocking the UI thread is illegal.
  profile->GetPrefs()->CommitPendingWrite();
  content::BrowserContext::GetDefaultStoragePartition(profile)
      ->GetCookieManagerForBrowserProcess()
      ->FlushCookieStore(
          network::mojom::CookieManager::FlushCookieStoreCallback());
  content::BrowserContext::ForEachStoragePartition(
      profile, base::Bind(FlushStoragePartition));
}

void RemoveSessionCookiesForProfile(Profile* profile) {
  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->session_control =
      network::mojom::CookieDeletionSessionControl::SESSION_COOKIES;
  content::BrowserContext::GetDefaultStoragePartition(profile)
      ->GetCookieManagerForBrowserProcess()
      ->DeleteCookies(std::move(filter),
                      network::mojom::CookieManager::DeleteCookiesCallback());
}

}  // namespace

static void JNI_ProfileManagerUtils_FlushPersistentDataForAllProfiles(
    JNIEnv* env) {
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::for_each(loaded_profiles.begin(), loaded_profiles.end(),
                CommitPendingWritesForProfile);

  if (g_browser_process->local_state())
    g_browser_process->local_state()->CommitPendingWrite();
}

static void JNI_ProfileManagerUtils_RemoveSessionCookiesForAllProfiles(
    JNIEnv* env) {
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::for_each(loaded_profiles.begin(), loaded_profiles.end(),
                RemoveSessionCookiesForProfile);
}
