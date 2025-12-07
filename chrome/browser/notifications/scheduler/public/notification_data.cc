// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/notification_data.h"

namespace notifications {

NotificationData::Button::Button() : type(ActionButtonType::kUnknownAction) {}

NotificationData::Button::Button(const Button& other) = default;

NotificationData::Button::Button(Button&& other) = default;

NotificationData::Button& NotificationData::Button::operator=(
    const Button& other) = default;

NotificationData::Button& NotificationData::Button::operator=(Button&& other) =
    default;

NotificationData::Button::~Button() = default;

bool NotificationData::Button::operator==(const Button& other) const {
  return text == other.text && type == other.type && id == other.id;
}

NotificationData::NotificationData() = default;

NotificationData::NotificationData(const NotificationData& other) = default;

NotificationData::NotificationData(NotificationData&& other) = default;

NotificationData& NotificationData::operator=(const NotificationData& other) =
    default;

NotificationData& NotificationData::operator=(NotificationData&& other) =
    default;

NotificationData::~NotificationData() = default;

bool NotificationData::operator==(const NotificationData& other) const {
  return title == other.title && message == other.message &&
         custom_data == other.custom_data && buttons == other.buttons &&
         icons.size() == other.icons.size();
}


}  // namespace notifications
