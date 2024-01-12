// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/browser_prefs_android.h"

#include "chrome/browser/android/preferences/clipboard_android.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/notifications/notification_platform_bridge_android.h"
#include "chrome/browser/readaloud/android/prefs.h"
#include "chrome/browser/webauthn/android/cable_module_android.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace android {

void RegisterPrefs(PrefRegistrySimple* registry) {
  RegisterClipboardAndroidPrefs(registry);
  readaloud::RegisterLocalPrefs(registry);
  webauthn::authenticator::RegisterLocalState(registry);
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  NotificationPlatformBridgeAndroid::RegisterProfilePrefs(registry);
  registry->RegisterBooleanPref(prefs::kDesktopSiteWindowSettingEnabled, false);
}

}  // namespace android
