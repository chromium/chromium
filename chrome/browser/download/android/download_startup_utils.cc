// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_startup_utils.h"

#include <jni.h>
#include <utility>

#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/download/android/jni_headers/DownloadStartupUtils_jni.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/download/public/common/in_progress_download_manager.h"

static void JNI_DownloadStartupUtils_EnsureDownloadSystemInitialized(
    JNIEnv* env,
    jboolean is_full_browser_started,
    jboolean is_incognito) {
  DownloadStartupUtils::EnsureDownloadSystemInitialized(is_full_browser_started,
                                                        is_incognito);
}

// static
void DownloadStartupUtils::EnsureDownloadSystemInitialized(
    bool is_full_browser_started,
    bool is_incognito) {
  DCHECK(is_full_browser_started || !is_incognito)
      << "Incognito mode must load full browser.";
  ProfileKey* profile_key = nullptr;
  if (is_full_browser_started) {
    Profile* profile =
        ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
    if (is_incognito) {
      if (profile->HasOffTheRecordProfile())
        profile = profile->GetOffTheRecordProfile();
      else
        return;
    }
    profile_key = profile->GetProfileKey();
  } else {
    profile_key = ProfileKeyStartupAccessor::GetInstance()->profile_key();
  }
  DownloadManagerUtils::GetInProgressDownloadManager(profile_key);
}
