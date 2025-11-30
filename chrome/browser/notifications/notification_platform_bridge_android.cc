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
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/android/notification_content_detection_manager_android.h"
#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
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

static ScopedJavaLocalRef<jobject>
JNI_NotificationPlatformBridge_ConvertToJavaBitmap(JNIEnv* env,
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
    std::u16string title = button.title;
    int type = GetNotificationActionType(button);
    std::u16string placeholder;
    if (button.placeholder) {
      placeholder = *button.placeholder;
    }
    ScopedJavaLocalRef<jobject> icon =
        JNI_NotificationPlatformBridge_ConvertToJavaBitmap(env, button.icon);
    ScopedJavaLocalRef<jobject> action_info = Java_ActionInfo_createActionInfo(
        AttachCurrentThread(), title, icon, type, placeholder);
    env->SetObjectArrayElement(actions, i, action_info.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, actions);
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

  NOTREACHED();
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
      base::BindOnce(
          &NotificationDisplayServiceImpl::ProfileLoadedCallback,
          NotificationOperation::kClick, notification_type, origin,
          notification_id, std::move(action_index), std::move(reply),
          std::nullopt /* by_user */, std::nullopt /* is_suspicious */,
          base::BindOnce(
              &NotificationPlatformBridgeAndroid::OnNotificationProcessed,
              weak_factory_.GetWeakPtr(), notification_id)));
}

void NotificationPlatformBridgeAndroid::
    StoreCachedWebApkPackageForNotificationId(
        JNIEnv* env,
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
      base::BindOnce(
          &NotificationDisplayServiceImpl::ProfileLoadedCallback,
          NotificationOperation::kClose, notification_type, GURL(origin),
          notification_id, std::nullopt /* action index */,
          std::nullopt /* reply */, by_user, std::nullopt /* is_suspicious */,
          base::BindOnce(
              &NotificationPlatformBridgeAndroid::OnNotificationProcessed,
              weak_factory_.GetWeakPtr(), notification_id)));
}

void NotificationPlatformBridgeAndroid::OnNotificationDisablePermission(
    JNIEnv* env,
    std::string& notification_id,
    jint java_notification_type,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito,
    jboolean is_suspicious) {
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
                     std::nullopt /* by_user */, is_suspicious,
                     base::DoNothing()));
}

void NotificationPlatformBridgeAndroid::SetIsSuspiciousParameterForTesting(
    JNIEnv* env,
    bool is_suspicious) {
  should_use_test_is_suspicious_value_ = true;
  test_is_suspicious_value_ = is_suspicious;
}

void NotificationPlatformBridgeAndroid::OnReportNotificationAsSafe(
    JNIEnv* env,
    std::string& notification_id,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CHECK(profile_manager);

  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     NotificationOperation::kReportAsSafe,
                     NotificationHandler::Type::WEB_PERSISTENT, GURL(origin),
                     notification_id, std::nullopt /* action index */,
                     std::nullopt /* reply */, std::nullopt /* by_user */,
                     std::nullopt /* is_suspicious */, base::DoNothing()));
}

void NotificationPlatformBridgeAndroid::OnReportWarnedNotificationAsSpam(
    JNIEnv* env,
    std::string& notification_id,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CHECK(profile_manager);

  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     NotificationOperation::kReportWarnedAsSpam,
                     NotificationHandler::Type::WEB_PERSISTENT, GURL(origin),
                     notification_id, std::nullopt /* action index */,
                     std::nullopt /* reply */, std::nullopt /* by_user */,
                     std::nullopt /* is_suspicious */, base::DoNothing()));
}

void NotificationPlatformBridgeAndroid::OnReportUnwarnedNotificationAsSpam(
    JNIEnv* env,
    std::string& notification_id,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CHECK(profile_manager);

  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     NotificationOperation::kReportUnwarnedAsSpam,
                     NotificationHandler::Type::WEB_PERSISTENT, GURL(origin),
                     notification_id, std::nullopt /* action index */,
                     std::nullopt /* reply */, std::nullopt /* by_user */,
                     std::nullopt /* is_suspicious */, base::DoNothing()));
}

void NotificationPlatformBridgeAndroid::OnNotificationShowOriginalNotification(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_object,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CHECK(profile_manager);

  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     NotificationOperation::kShowOriginalNotification,
                     NotificationHandler::Type::WEB_PERSISTENT, GURL(origin),
                     /*notification_id=*/"", std::nullopt /* action index */,
                     std::nullopt /* reply */, std::nullopt /* by_user */,
                     std::nullopt /* is_suspicious */, base::DoNothing()));
}

void NotificationPlatformBridgeAndroid::OnNotificationAlwaysAllowFromOrigin(
    JNIEnv* env,
    std::string& notification_id,
    std::string& origin,
    std::string& profile_id,
    jboolean incognito) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CHECK(profile_manager);

  const GURL& url = GURL(origin);
  profile_manager->LoadProfile(
      GetProfileBaseNameFromProfileId(profile_id), incognito,
      base::BindOnce(
          &NotificationPlatformBridgeAndroid::AlwaysAllowNotifications,
          weak_factory_.GetWeakPtr(), url, notification_id));
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

  bool skip_ua_buttons = persistent_notification_metadata
                             ? persistent_notification_metadata->skip_ua_buttons
                             : false;
  // Extension notifications never show UA buttons like "Unsubscribe" or
  // "Site Settings".
  if (notification_type == NotificationHandler::Type::EXTENSION) {
    skip_ua_buttons = true;
  }

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
      notification.renotify(), notification.silent(), actions,
      should_use_test_is_suspicious_value_
          ? test_is_suspicious_value_
          : (persistent_notification_metadata
                 ? persistent_notification_metadata->is_suspicious
                 : false),
      skip_ua_buttons);

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

  GURL scope_url(
      notification_info.service_worker_scope.possibly_invalid_spec());
  std::string scope_url_spec = scope_url.spec();

  bool has_queried_webapk_package =
      notification_info.webapk_package.has_value();
  std::string webapk_package =
      has_queried_webapk_package ? *notification_info.webapk_package : "";

  regenerated_notification_infos_.erase(iterator);

  Java_NotificationPlatformBridge_closeNotification(
      env, java_object_, notification_id, scope_url_spec,
      has_queried_webapk_package, webapk_package);
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

void NotificationPlatformBridgeAndroid::OnNotificationProcessed(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NotificationPlatformBridge_onNotificationProcessed(env, java_object_,
                                                          notification_id);
}

void NotificationPlatformBridgeAndroid::AlwaysAllowNotifications(
    const GURL& url,
    const std::string& notification_id,
    Profile* profile) {
  // Always allow suspicious notifications from `url`.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  if (!hcsm) {
    return;
  }
  CHECK(url.is_valid());
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
      base::Value(base::Value::Dict().Set(
          safe_browsing::kIsAllowlistedByUserKey, true)));

  safe_browsing::NotificationContentDetectionUkmUtil::
      RecordSuspiciousNotificationInteractionUkm(
          static_cast<int>(
              safe_browsing::SuspiciousNotificationWarningInteractions::
                  kAlwaysAllow),
          url, notification_id, profile);

  // Send a new notification to tell the user that Chrome will no longer hide
  // notifications from `url`.
  std::u16string notification_title =
      base::FeatureList::IsEnabled(
          safe_browsing::kReportNotificationContentDetectionData)
          ? l10n_util::GetStringFUTF16(
                IDS_CHROME_NO_LONGER_SHOW_WARNINGS_NOTIFICATION_TITLE_NEW,
                url_formatter::FormatUrl(
                    url,
                    url_formatter::kFormatUrlOmitDefaults |
                        url_formatter::kFormatUrlOmitHTTPS |
                        url_formatter::kFormatUrlOmitTrivialSubdomains |
                        url_formatter::kFormatUrlTrimAfterHost,
                    base::UnescapeRule::SPACES, nullptr, nullptr, nullptr))
          : l10n_util::GetStringUTF16(
                IDS_CHROME_NO_LONGER_SHOW_WARNINGS_NOTIFICATION_TITLE);
  std::u16string notification_body =
      base::FeatureList::IsEnabled(
          safe_browsing::kReportNotificationContentDetectionData)
          ? l10n_util::GetStringUTF16(
                IDS_CHROME_NO_LONGER_SHOW_WARNINGS_NOTIFICATION_BODY_NEW)
          : l10n_util::GetStringUTF16(
                IDS_CHROME_NO_LONGER_SHOW_WARNINGS_NOTIFICATION_BODY);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      base::NumberToString(
          PlatformNotificationServiceFactory::GetForProfile(profile)
              ->ReadNextPersistentNotificationId()),
      notification_title, notification_body, ui::ImageModel(), std::u16string(),
      url, message_center::NotifierId(), message_center::RichNotificationData(),
      nullptr);
  // Create new `PersistentNotificationMetadata`, where `is_suspicious` is set
  // to false by default. Set `skip_ua_buttons` to true so the confirmation
  // notification does not restore any UA buttons.
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->skip_ua_buttons = true;
  Display(NotificationHandler::Type::WEB_PERSISTENT, profile, notification,
          std::move(metadata));
}

// static
void NotificationPlatformBridgeAndroid::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNotificationsVibrateEnabled, true);
}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    RegeneratedNotificationInfo() = default;

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    RegeneratedNotificationInfo(
        const GURL& service_worker_scope,
        const std::optional<std::string>& webapk_package)
    : service_worker_scope(service_worker_scope),
      webapk_package(webapk_package) {}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    ~RegeneratedNotificationInfo() = default;

DEFINE_JNI(ActionInfo)
DEFINE_JNI(NotificationPlatformBridge)
