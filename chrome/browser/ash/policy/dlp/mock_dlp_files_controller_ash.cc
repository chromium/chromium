// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/mock_dlp_files_controller_ash.h"

namespace policy {

MockDlpFilesControllerAsh::MockDlpFilesControllerAsh(
    const DlpRulesManager& rules_manager)
    : DlpFilesControllerAsh(rules_manager) {}

MockDlpFilesControllerAsh::~MockDlpFilesControllerAsh() = default;

}  // namespace policy
