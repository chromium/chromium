// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/notification_data.h"

namespace notifications {

NotificationData::Button::Button() : type(ActionButtonType::kUnknownAction) {}
NotificationData::Button::Button(const Button& other) = default;

bool NotificationData::Button::operator==(const Button& other) const {
  return text == other.text && type == other.type && id == other.id;
}

NotificationData::Button::~Button() = default;

NotificationData::NotificationData() = default;

NotificationData::NotificationData(const NotificationData& other) = default;

bool NotificationData::operator==(const NotificationData& other) const {
  return title == other.title && message == other.message &&
         custom_data == other.custom_data && buttons == other.buttons &&
         icons.size() == other.icons.size();
}

NotificationData::~NotificationData() = default;

}  // namespace notifications
