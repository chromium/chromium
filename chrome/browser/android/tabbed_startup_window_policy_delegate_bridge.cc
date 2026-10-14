// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tabbed_startup_window_policy_delegate_bridge.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/startup/url_util.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabbedStartupWindowPolicyDelegate_jni.h"

namespace chrome::android {

std::vector<std::string> GetSessionStartupUrls(PrefService* pref_service) {
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

void SetSessionStartupUrls(PrefService* pref_service,
                           const std::vector<std::string>& urls) {
  if (!pref_service) {
    return;
  }
  SessionStartupPref startup_pref =
      SessionStartupPref::GetStartupPref(pref_service);
  startup_pref.urls.clear();
  startup_pref.urls.reserve(urls.size());
  for (const std::string& raw_url : urls) {
    GURL url(raw_url);
    if (startup::ValidateLaunchUrlWebSafe(url)) {
      startup_pref.urls.push_back(std::move(url));
    }
  }
  if (startup_pref.urls.empty()) {
    // SessionStartupPref::DEFAULT maps to SessionStartupPref::kPrefValueNewTab
    // (SessionStartupPref.NEW_TAB on Java) when persisted to prefs via
    // SessionStartupPref::SetStartupPref(), ensuring empty startup URL lists
    // fall back to opening the New Tab page on startup.
    startup_pref.type = SessionStartupPref::DEFAULT;
  }
  SessionStartupPref::SetStartupPref(pref_service, startup_pref);
}

static std::vector<std::string>
JNI_TabbedStartupWindowPolicyDelegate_GetSessionStartupUrls(
    PrefService* pref_service) {
  return GetSessionStartupUrls(pref_service);
}

static void JNI_TabbedStartupWindowPolicyDelegate_SetSessionStartupUrls(
    PrefService* pref_service,
    const std::vector<std::string>& urls) {
  SetSessionStartupUrls(pref_service, urls);
}

}  // namespace chrome::android

DEFINE_JNI(TabbedStartupWindowPolicyDelegate)
