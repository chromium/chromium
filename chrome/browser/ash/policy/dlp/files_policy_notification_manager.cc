// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include "base/check.h"

namespace policy {

FilesPolicyNotificationManager::FilesPolicyNotificationManager(
    content::BrowserContext* context) {
  DCHECK(context);
}

FilesPolicyNotificationManager::~FilesPolicyNotificationManager() = default;

}  // namespace policy
