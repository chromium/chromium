// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFERENCES_ANDROID_SHARED_PREFERENCES_MANAGER_H_
#define CHROME_BROWSER_PREFERENCES_ANDROID_SHARED_PREFERENCES_MANAGER_H_

#include "base/android/jni_android.h"

namespace android::shared_preferences {

// A SharedPreferencesManager that provides access to Android SharedPreferences
// with uniqueness key checking.
class SharedPreferencesManager {
 public:
  explicit SharedPreferencesManager(const base::android::JavaRef<jobject>& jobj,
                                    JNIEnv* env);
  SharedPreferencesManager(const SharedPreferencesManager&);
  SharedPreferencesManager& operator=(const SharedPreferencesManager&) = delete;
  ~SharedPreferencesManager();

  void RemoveKey(const std::string& shared_preference_key);
  bool ContainsKey(const std::string& shared_preference_key);
  bool ReadBoolean(const std::string& shared_preference_key,
                   bool default_value);
  int ReadInt(const std::string& shared_preference_key, int default_value);
  std::string ReadString(const std::string& shared_preference_key,
                         const std::string& default_value);
  void WriteString(const std::string& shared_preference_key,
                   const std::string& value);

 private:
  base::android::ScopedJavaLocalRef<jobject> java_obj_;
  raw_ptr<JNIEnv> env_;
};

// Get a SharedPreferencesManager to access SharedPreferences registered in
// ChromePreferenceKeys.java.
const SharedPreferencesManager GetChromeSharedPreferences();

}  // namespace android::shared_preferences

#endif  // CHROME_BROWSER_PREFERENCES_ANDROID_SHARED_PREFERENCES_MANAGER_H_
