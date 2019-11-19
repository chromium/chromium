// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_UTIL_H_

#include <string>

#include "base/optional.h"

namespace crostini {

// Based on previous and current software configurations in JSON format,
// generate a playbook that applies requested changes.
// Convenience wrapper for PendingSoftwareChanges::ToAnsiblePlaybook().
base::Optional<std::string> GeneratePlaybookFromConfig(
    const std::string& new_config_json,
    const std::string& old_config_json = "");

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_UTIL_H_
