// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/test/mock_dlp_files_controller_ash.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

MockDlpFilesControllerAsh::MockDlpFilesControllerAsh(
    const DlpRulesManager& rules_manager)
    : DlpFilesControllerAsh(rules_manager) {
  ON_CALL(*this, CheckIfLaunchAllowed)
      .WillByDefault([](const apps::AppUpdate& app_update,
                        apps::IntentPtr intent,
                        CheckIfDlpAllowedCallback result_callback) {
        std::move(result_callback).Run(true);
      });
}

MockDlpFilesControllerAsh::~MockDlpFilesControllerAsh() = default;

}  // namespace policy
