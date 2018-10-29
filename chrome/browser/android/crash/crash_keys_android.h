// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CRASH_CRASH_KEYS_ANDROID_H_
#define CHROME_BROWSER_ANDROID_CRASH_CRASH_KEYS_ANDROID_H_

#include <string>

// See CrashKeys.java for how to add a new crash key.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.crash
enum class CrashKeyIndex {
  LOADED_DYNAMIC_MODULE = 0,
  ACTIVE_DYNAMIC_MODULE,
  APPLICATION_STATUS,
  NUM_KEYS
};

// These methods are only exposed for testing -- normal usage should be from
// Java.
void SetAndroidCrashKey(CrashKeyIndex index, const std::string& value);
void ClearAndroidCrashKey(CrashKeyIndex index);
void FlushAndroidCrashKeys();

#endif  // CHROME_BROWSER_ANDROID_CRASH_CRASH_KEYS_ANDROID_H_
