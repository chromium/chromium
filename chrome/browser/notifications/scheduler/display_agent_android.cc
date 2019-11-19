// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/display_agent_android.h"

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/DisplayAgent_jni.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

notifications::UserActionHandler* GetUserActionHandler(
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  auto* service =
      NotificationScheduleServiceFactory::GetForBrowserContext(profile);
  return service->GetUserActionHandler();
}

}  // namespace

// static
void JNI_DisplayAgent_OnUserAction(JNIEnv* env,
                                   const JavaParamRef<jobject>& j_profile,
                                   jint j_client_type,
                                   jint j_action_type,
                                   const JavaParamRef<jstring>& j_guid,
                                   jint j_button_type,
                                   const JavaParamRef<jstring>& j_button_id) {
  auto user_action_type =
      static_cast<notifications::UserActionType>(j_action_type);
  notifications::UserActionData action_data(
      static_cast<notifications::SchedulerClientType>(j_client_type),
      user_action_type, ConvertJavaStringToUTF8(env, j_guid));

  // Attach button click data.
  if (user_action_type == notifications::UserActionType::kButtonClick) {
    notifications::ButtonClickInfo button_click_info;
    button_click_info.button_id = ConvertJavaStringToUTF8(env, j_button_id);
    button_click_info.type =
        static_cast<notifications::ActionButtonType>(j_button_type);
    action_data.button_click_info =
        base::make_optional(std::move(button_click_info));
  }

  GetUserActionHandler(j_profile)->OnUserAction(action_data);
}

DisplayAgentAndroid::DisplayAgentAndroid() = default;

DisplayAgentAndroid::~DisplayAgentAndroid() = default;

void DisplayAgentAndroid::ShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    std::unique_ptr<SystemData> system_data) {
  // TODO(xingliu): Refactor and hook to NotificationDisplayService.
  DCHECK(notification_data);
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!notification_data->title.empty());
  DCHECK(!notification_data->message.empty());

  // Wrap button info. Retrieving class name in run time must be guarded with
  // test.
  auto java_notification_data = Java_DisplayAgent_buildNotificationData(
      env, ConvertUTF16ToJavaString(env, notification_data->title),
      ConvertUTF16ToJavaString(env, notification_data->message));

  for (const auto& icon : notification_data->icons) {
    Java_DisplayAgent_addIcon(env, java_notification_data,
                              static_cast<int>(icon.first /*IconType*/),
                              gfx::ConvertToJavaBitmap(&icon.second.bitmap),
                              static_cast<jint>(icon.second.resource_id));
  }

  for (size_t i = 0; i < notification_data->buttons.size(); ++i) {
    const auto& button = notification_data->buttons[i];
    Java_DisplayAgent_addButton(
        env, java_notification_data, ConvertUTF16ToJavaString(env, button.text),
        static_cast<int>(button.type), ConvertUTF8ToJavaString(env, button.id));
  }

  auto java_system_data = Java_DisplayAgent_buildSystemData(
      env, static_cast<int>(system_data->type),
      ConvertUTF8ToJavaString(env, system_data->guid));

  Java_DisplayAgent_showNotification(env, java_notification_data,
                                     java_system_data);
}
