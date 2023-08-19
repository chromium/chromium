// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/test/mock_files_policy_notification_manager.h"

namespace policy {

MockFilesPolicyNotificationManager::MockFilesPolicyNotificationManager(
    content::BrowserContext* context)
    : FilesPolicyNotificationManager(context) {}

MockFilesPolicyNotificationManager::~MockFilesPolicyNotificationManager() =
    default;

}  // namespace policy
