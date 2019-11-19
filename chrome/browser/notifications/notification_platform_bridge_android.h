// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_ANDROID_H_

#include <stdint.h>
#include <map>
#include <set>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/optional.h"
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
  ~NotificationPlatformBridgeAndroid() override;

  // Called by the Java implementation when the notification has been clicked.
  void OnNotificationClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      const base::android::JavaParamRef<jstring>& java_notification_id,
      jint java_notification_type,
      const base::android::JavaParamRef<jstring>& java_origin,
      const base::android::JavaParamRef<jstring>& java_scope_url,
      const base::android::JavaParamRef<jstring>& java_profile_id,
      jboolean incognito,
      const base::android::JavaParamRef<jstring>& java_webapk_package,
      jint action_index,
      const base::android::JavaParamRef<jstring>& java_reply);

  // Called by the Java implementation when the query of WebAPK's package name
  // is done.
  void StoreCachedWebApkPackageForNotificationId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      const base::android::JavaParamRef<jstring>& java_notification_id,
      const base::android::JavaParamRef<jstring>& java_webapk_package);

  // Called by the Java implementation when the notification has been closed.
  void OnNotificationClosed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      const base::android::JavaParamRef<jstring>& java_notification_id,
      jint java_notification_type,
      const base::android::JavaParamRef<jstring>& java_origin,
      const base::android::JavaParamRef<jstring>& java_profile_id,
      jboolean incognito,
      jboolean by_user);

  // NotificationPlatformBridge implementation.
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
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
        const base::Optional<std::string>& webapk_package);
    ~RegeneratedNotificationInfo();

    GURL service_worker_scope;
    base::Optional<std::string> webapk_package;
  };

  // Mapping of notification id to renegerated notification info.
  // TODO(peter): Remove this map once notification delegate ids for Web
  // notifications are created by the content/ layer.
  std::map<std::string, RegeneratedNotificationInfo>
      regenerated_notification_infos_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_ANDROID_H_
