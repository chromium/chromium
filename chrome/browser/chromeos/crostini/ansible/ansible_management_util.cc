// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/ansible_management_util.h"

#include "chrome/browser/chromeos/crostini/ansible/pending_software_changes.h"

namespace crostini {

base::Optional<std::string> GeneratePlaybookFromConfig(
    const std::string& new_config_json,
    const std::string& old_config_json) {
  auto new_config = SoftwareConfig::FromJson(new_config_json);
  auto old_config = SoftwareConfig::FromJson(old_config_json);

  if (!new_config.has_value()) {
    LOG(ERROR) << "Failed to parse new config.";
    return base::nullopt;
  }
  if (!old_config.has_value()) {
    LOG(ERROR) << "Failed to parse old config.";
    return base::nullopt;
  }

  PendingSoftwareChanges changes(new_config.value(), old_config.value());
  return changes.ToAnsiblePlaybook();
}

}  // namespace crostini
