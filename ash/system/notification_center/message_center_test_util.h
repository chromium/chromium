// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_TEST_UTIL_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_TEST_UTIL_H_

#include <memory>
#include <string>

#include "url/gurl.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// Creates a simple notification with the given id. If `has_image` is true, the
// notification will contain a test image.
std::unique_ptr<message_center::Notification> CreateSimpleNotification(
    const std::string& id,
    bool has_image = false,
    const GURL& origin_url = GURL(),
    bool has_inline_reply = false);

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_TEST_UTIL_H_
