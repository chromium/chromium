// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/magic_stack_bridge.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/safety_hub/android/jni_headers/MagicStackBridge_jni.h"
#include "chrome/browser/safety_hub/android/jni_headers/MagicStackEntry_jni.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"

namespace {

constexpr char kSafeBrowsing[] = "safe_browsing";
constexpr char kNotificationPermissions[] = "notification_permissions";
constexpr char kRevokedPermissions[] = "revoked_permissions";
constexpr char kPasswords[] = "passwords";

std::string ModuleTypeToString(safety_hub::SafetyHubModuleType module) {
  switch (module) {
    case safety_hub::SafetyHubModuleType::SAFE_BROWSING:
      return kSafeBrowsing;
    case safety_hub::SafetyHubModuleType::NOTIFICATION_PERMISSIONS:
      return kNotificationPermissions;
    case safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS:
      return kRevokedPermissions;
    case safety_hub::SafetyHubModuleType::PASSWORDS:
      return kPasswords;
    default:
      NOTREACHED() << "Module not supported on Android.";
  }
}

}  // namespace

base::android::ScopedJavaLocalRef<jobject> ToJavaMagicStackEntry(
    JNIEnv* env,
    const MenuNotificationEntry& obj) {
  return Java_MagicStackEntry_create(env, obj.label,
                                     ModuleTypeToString(obj.module));
}

std::optional<MenuNotificationEntry> JNI_MagicStackBridge_GetModuleToShow(
    JNIEnv* env,
    Profile* profile) {
  SafetyHubMenuNotificationService* service =
      SafetyHubMenuNotificationServiceFactory::GetForProfile(profile);
  CHECK(service);
  return service->GetNotificationToShow();
}

void JNI_MagicStackBridge_DismissActiveModule(JNIEnv* env, Profile* profile) {
  SafetyHubMenuNotificationService* service =
      SafetyHubMenuNotificationServiceFactory::GetForProfile(profile);
  CHECK(service);
  service->DismissActiveNotification();
}

void JNI_MagicStackBridge_DismissSafeBrowsingModule(JNIEnv* env,
                                                    Profile* profile) {
  SafetyHubMenuNotificationService* service =
      SafetyHubMenuNotificationServiceFactory::GetForProfile(profile);
  CHECK(service);
  service->DismissActiveNotificationOfModule(
      safety_hub::SafetyHubModuleType::SAFE_BROWSING);
}

void JNI_MagicStackBridge_DismissCompromisedPasswordsModule(JNIEnv* env,
                                                            Profile* profile) {
  SafetyHubMenuNotificationService* service =
      SafetyHubMenuNotificationServiceFactory::GetForProfile(profile);
  CHECK(service);
  service->DismissActiveNotificationOfModule(
      safety_hub::SafetyHubModuleType::PASSWORDS);
}
