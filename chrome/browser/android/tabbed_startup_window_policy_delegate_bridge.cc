// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabbedStartupWindowPolicyDelegate_jni.h"

namespace chrome::android {

static std::vector<std::string>
JNI_TabbedStartupWindowPolicyDelegate_GetSessionStartupUrls(
    JNIEnv* env,
    PrefService* pref_service) {
  std::vector<std::string> url_strings;
  if (!pref_service) {
    return url_strings;
  }
  SessionStartupPref startup_pref =
      SessionStartupPref::GetStartupPref(pref_service);
  for (const GURL& url : startup_pref.urls) {
    url_strings.push_back(url.spec());
  }
  return url_strings;
}

}  // namespace chrome::android

DEFINE_JNI(TabbedStartupWindowPolicyDelegate)
