// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"

namespace policy {

DlpFilesController::DlpFilesController(const DlpRulesManager& rules_manager)
    : rules_manager_(rules_manager) {}

DlpFilesController::~DlpFilesController() = default;

}  // namespace policy
