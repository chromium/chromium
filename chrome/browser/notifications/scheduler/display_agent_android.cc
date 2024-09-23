// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/display_agent_android.h"

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DisplayAgent_jni.h"
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

notifications::UserActionHandler* GetUserActionHandler() {
  ProfileKey* profile_key = ::android::GetLastUsedRegularProfileKey();
  DCHECK(profile_key);
  auto* service = NotificationScheduleServiceFactory::GetForKey(profile_key);
  DCHECK(service);
  return service->GetUserActionHandler();
}

}  // namespace

// static
void JNI_DisplayAgent_OnUserAction(JNIEnv* env,
                                   jint j_client_type,
                                   jint j_action_type,
                                   std::string& guid,
                                   jint j_button_type,
                                   std::string& button_id) {
  auto user_action_type =
      static_cast<notifications::UserActionType>(j_action_type);
  notifications::UserActionData action_data(
      static_cast<notifications::SchedulerClientType>(j_client_type),
      user_action_type, guid);

  // Attach button click data.
  if (user_action_type == notifications::UserActionType::kButtonClick) {
    notifications::ButtonClickInfo button_click_info;
    button_click_info.button_id = button_id;
    button_click_info.type =
        static_cast<notifications::ActionButtonType>(j_button_type);
    action_data.button_click_info =
        std::make_optional(std::move(button_click_info));
  }

  GetUserActionHandler()->OnUserAction(action_data);
}

DisplayAgentAndroid::DisplayAgentAndroid() = default;

DisplayAgentAndroid::~DisplayAgentAndroid() = default;

void DisplayAgentAndroid::ShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    std::unique_ptr<SystemData> system_data) {
  // TODO(xingliu): Refactor and hook to NotificationDisplayService.
  DCHECK(notification_data);
  JNIEnv* env = jni_zero::AttachCurrentThread();
  DCHECK(!notification_data->title.empty());
  DCHECK(!notification_data->message.empty());

  // Wrap button info. Retrieving class name in run time must be guarded with
  // test.
  auto java_notification_data = Java_DisplayAgent_buildNotificationData(
      env, notification_data->title, notification_data->message);

  for (const auto& icon : notification_data->icons) {
    Java_DisplayAgent_addIcon(
        env, java_notification_data, static_cast<int>(icon.first /*IconType*/),
        icon.second.bitmap, static_cast<jint>(icon.second.resource_id));
  }

  for (size_t i = 0; i < notification_data->buttons.size(); ++i) {
    const auto& button = notification_data->buttons[i];
    Java_DisplayAgent_addButton(env, java_notification_data, button.text,
                                static_cast<int>(button.type), button.id);
  }

  auto java_system_data = Java_DisplayAgent_buildSystemData(
      env, static_cast<int>(system_data->type), system_data->guid);

  Java_DisplayAgent_showNotification(env, java_notification_data,
                                     java_system_data);
}
