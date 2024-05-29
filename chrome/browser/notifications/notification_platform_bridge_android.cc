// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/native_theme/native_theme.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ActionInfo_jni.h"
#include "chrome/android/chrome_jni_headers/NotificationPlatformBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.notifications
enum NotificationActionType {
  // NB. Making this a one-line enum breaks code generation! crbug.com/657847
  BUTTON,
  TEXT
};

ScopedJavaLocalRef<jobject> JNI_NotificationPlatformBridge_ConvertToJavaBitmap(
    JNIEnv* env,
    const gfx::Image& icon) {
  SkBitmap skbitmap = icon.AsBitmap();
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!skbitmap.drawsNothing())
    j_bitmap = gfx::ConvertToJavaBitmap(skbitmap);
  return j_bitmap;
}

NotificationActionType GetNotificationActionType(
    message_center::ButtonInfo button) {
  return button.placeholder ? NotificationActionType::TEXT
                            : NotificationActionType::BUTTON;
}

ScopedJavaLocalRef<jobjectArray> ConvertToJavaActionInfos(
    const std::vector<message_center::ButtonInfo>& buttons) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jclass> clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/notifications/ActionInfo");
  jobjectArray actions = env->NewObjectArray(buttons.size(), clazz.obj(),
                                             nullptr /* initialElement */);
  base::android::CheckException(env);

  for (size_t i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons[i];
    ScopedJavaLocalRef<jstring> title =
        base::android::ConvertUTF16ToJavaString(env, button.title);
    int type = GetNotificationActionType(button);
    ScopedJavaLocalRef<jstring> placeholder;
    if (button.placeholder) {
      placeholder =
          base::android::ConvertUTF16ToJavaString(env, *button.placeholder);
    }
    ScopedJavaLocalRef<jobject> icon =
        JNI_NotificationPlatformBridge_ConvertToJavaBitmap(env, button.icon);
    ScopedJavaLocalRef<jobject> action_info = Java_ActionInfo_createActionInfo(
        AttachCurrentThread(), title, icon, type, placeholder);
    env->SetObjectArrayElement(actions, i, action_info.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, actions);
}

constexpr jint NotificationTypeToJava(
    NotificationHandler::Type notification_type) {
  return static_cast<jint>(notification_type);
}

constexpr NotificationHandler::Type JavaToNotificationType(
    jint notification_type) {
  constexpr jint kMinValue =
      NotificationTypeToJava(NotificationHandler::Type::WEB_PERSISTENT);
  constexpr jint kMaxValue =
      NotificationTypeToJava(NotificationHandler::Type::MAX);

  if (notification_type >= kMinValue && notification_type <= kMaxValue)
    return static_cast<NotificationHandler::Type>(notification_type);

  NOTREACHED_IN_MIGRATION();
  return NotificationHandler::Type::WEB_PERSISTENT;
}

}  // namespace

// Called by the Java side when a notification event has been received, but the
// NotificationBridge has not been initialized yet. Enforce initialization of
// the class.
static void JNI_NotificationPlatformBridge_InitializeNotificationPlatformBridge(
    JNIEnv* env) {
  g_browser_process->notification_platform_bridge();
}

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  return std::make_unique<NotificationPlatformBridgeAndroid>();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

NotificationPlatformBridgeAndroid::NotificationPlatformBridgeAndroid() {
  java_object_.Reset(Java_NotificationPlatformBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

NotificationPlatformBridgeAndroid::~NotificationPlatformBridgeAndroid() {
  Java_NotificationPlatformBridge_destroy(AttachCurrentThread(), java_object_);
}

void NotificationPlatformBridgeAndroid::OnNotificationClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    std::string& notification_id,
    jint java_notification_type,
    std::string& java_origin_str,
    std::string& scope_url_str,
    std::string& profile_id,
    jboolean incognito,
    std::string& webapk_package,
    jint java_action_index,
    const JavaParamRef<jstring>& java_reply) {
  std::optional<std::u16string> reply;
  if (java_reply)
    reply = ConvertJavaStringToUTF16(env, java_reply);

  GURL origin(java_origin_str);
  GURL scope_url(scope_url_str);
  regenerated_notification_infos_[notification_id] =
      RegeneratedNotificationInfo(scope_url, webapk_package);

  std::optional<int> action_index;
  if (java_action_index != kNotificationInvalidButtonIndex)
    action_index = java_action_index;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  NotificationHandler::Type notification_type =
      JavaToNotificationType(java_notification_type);

  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     NotificationOperation::kClick, notification_type, origin,
                     notification_id, std::move(action_index), std::move(reply),
                     std::nullopt /* by_user */));
}

void NotificationPlatformBridgeAndroid::
    StoreCachedWebApkPackageForNotificationId(
        JNIEnv* env,
        const base::android::JavaParamRef<jobject>& java_object,
        std::string& notification_id,
        std::string& webapk_package) {
  const auto iterator = regenerated_notification_infos_.find(notification_id);
  if (iterator == regenerated_notification_infos_.end())
    return;

  const RegeneratedNotificationInfo& info = iterator->second;
  regenerated_notification_infos_[notification_id] =
      RegeneratedNotificationInfo(info.service_worker_scope, webapk_package);
}

void NotificationPlatformBridgeAndroid::OnNotificationClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    std::string& notification_id,
    jint java_notification_type,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito,
    jboolean by_user) {
  // The notification was closed by the platform, so clear all local state.
  regenerated_notification_infos_.erase(notification_id);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  NotificationHandler::Type notification_type =
      JavaToNotificationType(java_notification_type);

  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     NotificationOperation::kClose, notification_type,
                     GURL(origin), notification_id,
                     std::nullopt /* action index */, std::nullopt /* reply */,
                     by_user));
}

void NotificationPlatformBridgeAndroid::OnNotificationDisablePermission(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    std::string& notification_id,
    jint java_notification_type,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  NotificationHandler::Type notification_type =
      JavaToNotificationType(java_notification_type);

  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     NotificationOperation::kDisablePermission,
                     notification_type, GURL(origin), notification_id,
                     std::nullopt /* action index */, std::nullopt /* reply */,
                     std::nullopt /* by_user */));
}

void NotificationPlatformBridgeAndroid::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  DCHECK(CanHandleType(notification_type));

  JNIEnv* env = AttachCurrentThread();

  GURL origin_url(notification.origin_url().DeprecatedGetOriginAsURL());

  // TODO(peter): Reconsider the meta-data system to try to remove this branch.
  const PersistentNotificationMetadata* persistent_notification_metadata =
      PersistentNotificationMetadata::From(metadata.get());

  GURL scope_url = persistent_notification_metadata
                       ? persistent_notification_metadata->service_worker_scope
                       : origin_url;
  if (!scope_url.is_valid())
    scope_url = origin_url;

  ScopedJavaLocalRef<jobject> android_profile = profile->GetJavaObject();

  SkBitmap image_bitmap = notification.image().AsBitmap();

  const auto* const color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(
          ui::NativeTheme::GetInstanceForWeb()->GetColorProviderKey(nullptr));
  const SkBitmap* notification_icon_bitmap =
      notification.icon().Rasterize(color_provider).bitmap();

  SkBitmap badge_bitmap = notification.small_image().AsBitmap();

  ScopedJavaLocalRef<jobjectArray> actions =
      ConvertToJavaActionInfos(notification.buttons());

  jint j_notification_type = NotificationTypeToJava(notification_type);

  Java_NotificationPlatformBridge_displayNotification(
      env, java_object_, notification.id(), j_notification_type,
      origin_url.spec(), scope_url.spec(), GetProfileId(profile),
      android_profile, notification.title(), notification.message(),
      image_bitmap, *notification_icon_bitmap, badge_bitmap,
      notification.vibration_pattern(),
      notification.timestamp().InMillisecondsSinceUnixEpoch(),
      notification.renotify(), notification.silent(), actions);

  regenerated_notification_infos_[notification.id()] =
      RegeneratedNotificationInfo(scope_url, std::nullopt);
}

void NotificationPlatformBridgeAndroid::Close(
    Profile* profile,
    const std::string& notification_id) {
  const auto iterator = regenerated_notification_infos_.find(notification_id);
  if (iterator == regenerated_notification_infos_.end())
    return;

  const RegeneratedNotificationInfo& notification_info = iterator->second;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_notification_id =
      ConvertUTF8ToJavaString(env, notification_id);

  GURL scope_url(
      notification_info.service_worker_scope.possibly_invalid_spec());
  ScopedJavaLocalRef<jstring> j_scope_url =
      ConvertUTF8ToJavaString(env, scope_url.spec());

  bool has_queried_webapk_package =
      notification_info.webapk_package.has_value();
  std::string webapk_package =
      has_queried_webapk_package ? *notification_info.webapk_package : "";
  ScopedJavaLocalRef<jstring> j_webapk_package =
      ConvertUTF8ToJavaString(env, webapk_package);

  regenerated_notification_infos_.erase(iterator);

  Java_NotificationPlatformBridge_closeNotification(
      env, java_object_, j_notification_id, j_scope_url,
      has_queried_webapk_package, j_webapk_package);
}

void NotificationPlatformBridgeAndroid::DisplayServiceShutDown(
    Profile* profile) {}

void NotificationPlatformBridgeAndroid::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  std::set<std::string> displayed_notifications;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(displayed_notifications),
                     false /* supports_synchronization */));
}

void NotificationPlatformBridgeAndroid::GetDisplayedForOrigin(
    Profile* profile,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  std::set<std::string> displayed_notifications;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(displayed_notifications),
                     false /* supports_synchronization */));
}

void NotificationPlatformBridgeAndroid::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true);
}

// static
void NotificationPlatformBridgeAndroid::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNotificationsVibrateEnabled, true);
}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    RegeneratedNotificationInfo() {}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    RegeneratedNotificationInfo(
        const GURL& service_worker_scope,
        const std::optional<std::string>& webapk_package)
    : service_worker_scope(service_worker_scope),
      webapk_package(webapk_package) {}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    ~RegeneratedNotificationInfo() {}
