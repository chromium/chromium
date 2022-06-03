// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordScriptsFetcherBridge_jni.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"

namespace password_manager {

// static
void JNI_PasswordScriptsFetcherBridge_PrewarmCache(JNIEnv* env) {
  PasswordScriptsFetcherFactory::GetInstance()
      ->GetForBrowserContext(ProfileManager::GetLastUsedProfile())
      ->PrewarmCache();
}

}  // namespace password_manager
