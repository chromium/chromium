// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_ANDROID_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// Implementation of the NotificationPlatformBridge for Android, which defers to
// the Android framework for displaying notifications.
//
// Prior to Android Marshmellow, Android did not have the ability to retrieve
// the notifications currently showing for an app without a rather intrusive
// permission. The GetDisplayed() method may return false because of this.
//
// The Android implementation *is* reliable for adding and canceling
// single notifications based on their delegate id. Finally, events for
// persistent Web Notifications will be forwarded directly to the associated
// event handlers, as such notifications may outlive the browser process on
// Android.
class NotificationPlatformBridgeAndroid : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeAndroid();
  NotificationPlatformBridgeAndroid(const NotificationPlatformBridgeAndroid&) =
      delete;
  NotificationPlatformBridgeAndroid& operator=(
      const NotificationPlatformBridgeAndroid&) = delete;
  ~NotificationPlatformBridgeAndroid() override;

  // Called by the Java implementation when the notification has been clicked.
  void OnNotificationClicked(JNIEnv* env,
                             const jni_zero::JavaParamRef<jobject>& java_object,
                             std::string& notification_id,
                             jint java_notification_type,
                             std::string& origin,
                             std::string& scope_url,
                             std::string& profile_id,
                             jboolean incognito,
                             std::string& webapk_package,
                             jint action_index,
                             const jni_zero::JavaParamRef<jstring>& java_reply);

  // Called by the Java implementation when the query of WebAPK's package name
  // is done.
  void StoreCachedWebApkPackageForNotificationId(
      JNIEnv* env,
      const jni_zero::JavaParamRef<jobject>& java_object,
      std::string& notification_id,
      std::string& webapk_package);

  // Called by the Java implementation when the notification has been closed.
  void OnNotificationClosed(JNIEnv* env,
                            const jni_zero::JavaParamRef<jobject>& java_object,
                            std::string& notification_id,
                            jint java_notification_type,
                            std::string& origin,
                            std::string& profile_id,
                            jboolean incognito,
                            jboolean by_user);

  // Called by the Java implementation when the user commits to unsubscribing
  // from notification from this origin.
  void OnNotificationDisablePermission(
      JNIEnv* env,
      const jni_zero::JavaParamRef<jobject>& java_object,
      std::string& otification_id,
      jint java_notification_type,
      std::string& origin,
      std::string& profile_id,
      jboolean incognito);

  // Called by Java tests for testing both suspicious and non-suspicious
  // notification behaviour when showing warnings for suspicious notifications
  // is enabled.
  void SetIsSuspiciousParameterForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      bool is_suspicious);

  // Called by the Java implementation when the user decides they want to report
  // their notification contents as safe to the server.
  void OnReportNotificationAsSafe(
      JNIEnv* env,
      const jni_zero::JavaParamRef<jobject>& java_object,
      std::string& notification_id,
      std::string& origin,
      std::string& profile_id,
      jboolean incognito);

  // Called by the Java implementation when the user decides they want to report
  // their warned notification contents as spam to the server.
  void OnReportWarnedNotificationAsSpam(
      JNIEnv* env,
      const jni_zero::JavaParamRef<jobject>& java_object,
      std::string& notification_id,
      std::string& origin,
      std::string& profile_id,
      jboolean incognito);

  // Called by the Java implementation when the user decides they want to report
  // their unwarned notification contents as spam to the server.
  void OnReportUnwarnedNotificationAsSpam(
      JNIEnv* env,
      const jni_zero::JavaParamRef<jobject>& java_object,
      std::string& notification_id,
      std::string& origin,
      std::string& profile_id,
      jboolean incognito);

  // Called by the Java implementation when the user decides they no longer want
  // to receive warnings for suspicious notifications that come from `origin`.
  void OnNotificationAlwaysAllowFromOrigin(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      std::string& origin,
      std::string& profile_id,
      jboolean incognito);

  // NotificationPlatformBridge implementation.
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void GetDisplayedForOrigin(
      Profile* profile,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  void OnNotificationProcessed(const std::string& notification_id);

  // Change user setting so that suspicious notifications from `url` are always
  // sent to the user. Then, send a new notification from Chrome to the user
  // informing them that their choice to "always allow" has been applied.
  void AlwaysAllowNotifications(const GURL& url, Profile* profile);

  // Contains information necessary in order to enable closing notifications
  // that were not created by this instance of the manager. This list may not
  // contain the notifications that have not been interacted with since the last
  // restart of Chrome.
  // When |Display()| is called, it sets an entry in
  // |regenerated_notification_infos_| synchronously. The |webapk_package| isn't
  // avaiable at that time since the query of WebAPK package name is
  // asynchronous. After the query is done, Java calls the
  // |StoreCachedWebApkPackageForNotificationId| to set |webapk_package|
  // properly. Therefore, before the |webapk_package| is set, additional query
  // is needed on the Java side. For example, we add an additional check when
  // closing a notification on Java in case the |Close()| is called before the
  // query is done.
  struct RegeneratedNotificationInfo {
    RegeneratedNotificationInfo();
    RegeneratedNotificationInfo(
        const GURL& service_worker_scope,
        const std::optional<std::string>& webapk_package);
    ~RegeneratedNotificationInfo();

    GURL service_worker_scope;
    std::optional<std::string> webapk_package;
  };

  // Mapping of notification id to renegerated notification info.
  // TODO(peter): Remove this map once notification delegate ids for Web
  // notifications are created by the content/ layer.
  std::map<std::string, RegeneratedNotificationInfo>
      regenerated_notification_infos_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // When `should_use_test_is_suspicious_value_` is true, use the
  // `test_is_suspicious_value_` value to tell the front end whether to display
  // a warning notification or the original notification.
  bool should_use_test_is_suspicious_value_ = false;
  bool test_is_suspicious_value_ = false;

  base::WeakPtrFactory<NotificationPlatformBridgeAndroid> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_ANDROID_H_
