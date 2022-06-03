// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_backup_watcher.h"
#include "base/bind.h"
#include "chrome/android/chrome_jni_headers/ChromeBackupWatcher_jni.h"
#include "chrome/browser/android/chrome_backup_agent.h"
#include "chrome/browser/profiles/profile.h"

namespace android {

namespace {

void BackupPrefsChanged(
    const base::android::ScopedJavaGlobalRef<jobject>& java_watcher) {
  Java_ChromeBackupWatcher_onBackupPrefsChanged(
      base::android::AttachCurrentThread(), java_watcher);
}
}

ChromeBackupWatcher::ChromeBackupWatcher(Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Create the Java class, and start watching the Android (Java) preferences
  java_watcher_.Reset(Java_ChromeBackupWatcher_createChromeBackupWatcher(env));
  // Now watch the Chrome C++ preferences
  registrar_.Init(profile->GetPrefs());
  base::RepeatingClosure callback =
      base::BindRepeating(&BackupPrefsChanged, java_watcher_);
  for (const std::string& pref_name : GetBackupPrefNames()) {
    registrar_.Add(pref_name, callback);
  }
}

ChromeBackupWatcher::~ChromeBackupWatcher() {}

}  //  namespace android
