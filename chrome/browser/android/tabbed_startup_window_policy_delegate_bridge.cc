// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/startup/url_util.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabbedStartupWindowPolicyDelegate_jni.h"

namespace chrome::android {

static std::vector<std::string>
JNI_TabbedStartupWindowPolicyDelegate_GetSessionStartupUrls(
    PrefService* pref_service) {
  std::vector<std::string> url_strings;
  if (!pref_service) {
    return url_strings;
  }
  SessionStartupPref startup_pref =
      SessionStartupPref::GetStartupPref(pref_service);
  for (const GURL& url : startup_pref.urls) {
    if (startup::ValidateLaunchUrlWebSafe(url)) {
      url_strings.push_back(url.spec());
    }
  }
  return url_strings;
}

static void
JNI_TabbedStartupWindowPolicyDelegate_SetSessionStartupUrlsForTesting(
    PrefService* pref_service,
    const std::vector<std::string>& urls) {
  CHECK(pref_service);
  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.reserve(urls.size());
  for (const std::string& url : urls) {
    startup_pref.urls.emplace_back(url);
  }
  SessionStartupPref::SetStartupPref(pref_service, startup_pref);
}

}  // namespace chrome::android

DEFINE_JNI(TabbedStartupWindowPolicyDelegate)
