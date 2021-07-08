// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {

namespace file_manager_private = extensions::api::file_manager_private;

TEST(SystemNotificationManagerTest, TestCopyEvents) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kFilesSWA);
  SystemNotificationManager notification_manager(nullptr);
  file_manager_private::CopyOrMoveProgressStatus status;

  // Check: an uninitialized status.source_url doesn't crash copy event handler.
  notification_manager.HandleCopyEvent(0, status);
}

}  // namespace file_manager
