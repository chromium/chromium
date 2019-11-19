// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_PENDING_SOFTWARE_CHANGES_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_PENDING_SOFTWARE_CHANGES_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/crostini/ansible/software_config.h"

namespace crostini {

// Class that calculates and represents changes which would be applied to the
// current container software configuration to achieve the target state.
//
// Pending changes can be exported as an Ansible playbook through the
// ToAnsiblePlaybook() function.
//
// In the current implementation, we assume ownership of managedcrostini sources
// list and keychain, so other entries from these files can be removed.
class PendingSoftwareChanges {
 public:
  // Calculate changes based on 2 state configurations. Current state
  // (|old_config|) can be empty, which means that no keys, sources and packages
  // were selected previously.
  PendingSoftwareChanges(const SoftwareConfig& new_config,
                         const SoftwareConfig& old_config);
  ~PendingSoftwareChanges();

  const std::vector<std::string>& key_urls() const { return key_urls_; }
  const std::vector<std::string>& source_lines() const { return source_lines_; }
  const std::vector<std::string>& unmark_package_names() const {
    return unmark_package_names_;
  }
  const std::vector<std::string>& install_package_names() const {
    return install_package_names_;
  }

  std::string ToAnsiblePlaybook() const;

 private:
  std::vector<std::string> key_urls_;
  std::vector<std::string> source_lines_;
  std::vector<std::string> unmark_package_names_;
  std::vector<std::string> install_package_names_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_PENDING_SOFTWARE_CHANGES_H_
