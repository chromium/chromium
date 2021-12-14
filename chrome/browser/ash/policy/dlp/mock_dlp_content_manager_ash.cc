// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/mock_dlp_content_manager_ash.h"

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

namespace policy {

MockDlpContentManagerAsh::MockDlpContentManagerAsh() = default;

MockDlpContentManagerAsh::~MockDlpContentManagerAsh() = default;

void MockDlpContentManagerAsh::Init() {
  SetReportingManagerForTesting(new DlpReportingManager());

  ON_CALL(*this, CheckScreenShareRestriction)
      .WillByDefault([](const content::DesktopMediaID& media_id,
                        const std::u16string& application_title,
                        OnDlpRestrictionCheckedCallback callback) {
        // Allow by default.
        std::move(callback).Run(true);
      });
}

}  // namespace policy
