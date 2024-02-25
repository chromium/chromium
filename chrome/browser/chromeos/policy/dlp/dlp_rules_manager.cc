// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#include "chrome/browser/enterprise/data_controls/chrome_dlp_rules_manager.h"

namespace policy {

DlpRulesManager::DlpRulesManager(Profile* profile)
    : data_controls::ChromeDlpRulesManager(profile) {}

}  // namespace policy
