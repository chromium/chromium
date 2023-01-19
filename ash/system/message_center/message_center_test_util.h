// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_TEST_UTIL_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_TEST_UTIL_H_

#include <memory>
#include <string>

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// Creates a simple notification with the given id.
std::unique_ptr<message_center::Notification> CreateSimpleNotification(
    const std::string& id);

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_TEST_UTIL_H_
