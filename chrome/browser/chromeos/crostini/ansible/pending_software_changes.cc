// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/pending_software_changes.h"

#include <algorithm>

#include "base/strings/strcat.h"

namespace crostini {

namespace {

const char kAnsiblePackageManagementPlaybookHeader[] =
    "---\n"
    "- hosts: localhost\n"
    "  become: yes\n"
    "  tasks:\n";

const char kAptKeyringPath[] = "/etc/apt/trusted.gpg.d/managedcrostini.gpg";
const char kAptSourcesPath[] = "/etc/apt/sources.list.d/managedcrostini.list";
const char kAptSourcesFilename[] = "managedcrostini";

}  // namespace

namespace {

std::string GenerateKeysSection(const std::vector<std::string>& key_urls) {
  // clang-format off
  std::string result = base::StrCat(
      {"  # Key configuration\n"
       "  - name: Remove existing keys\n"
       "    file:\n"
       "      path: ", kAptKeyringPath, "\n"
       "      state: absent\n"});
  // clang-format on

  for (const auto& url : key_urls) {
    // clang-format off
    base::StrAppend(&result,
        {"  - name: Add GPG key ", url, "\n"
         "    apt_key:\n"
         "      url: ", url, "\n"
         "      keyring: ", kAptKeyringPath, "\n"});
    // clang-format on
  }

  return result;
}

std::string GenerateSourcesSection(
    const std::vector<std::string>& source_lines) {
  // clang-format off
  std::string result = base::StrCat(
      {"  # Source configuration\n"
       "  - name: Remove existing sources\n"
       "    file:\n"
       "      path: ", kAptSourcesPath, "\n"
       "      state: absent\n"});
  // clang-format on

  for (const auto& line : source_lines) {
    // clang-format off
    base::StrAppend(&result,
        {"  - name: Add APT source \"", line, "\"\n"
         "    apt_repository:\n"
         "      repo: ", line, "\n"
         "      filename: ", kAptSourcesFilename, "\n"});
    // clang-format on
  }

  return result;
}

std::string GeneratePackagesSection(
    const std::vector<std::string>& unmark_package_names,
    const std::vector<std::string>& install_package_names) {
  std::string result = "  # Package configuration\n";
  if (!unmark_package_names.empty()) {
    base::StrAppend(&result,
                    {"  - name: Unmark unselected packages\n"
                     "    shell: apt-mark auto {{ packages|join(' ') }}\n"
                     "    vars:\n"
                     "      packages:\n"});
    for (const auto& name : unmark_package_names) {
      base::StrAppend(&result, {"      - ", name, "\n"});
    }

    // Now that we unmarked unneeded packages, trigger autoremove to uninstall
    // them (and their dependencies) if they are not required by other packages.
    base::StrAppend(&result,
                    {"  - name: Remove packages that are no longer needed\n"
                     "    apt: autoremove=yes\n"});
  }

  if (!install_package_names.empty()) {
    base::StrAppend(&result, {"  - name: Install packages\n"
                              "    apt:\n"
                              "      name: \"{{ packages }}\"\n"
                              "      update_cache: yes\n"
                              "    vars:\n"
                              "      packages:\n"});
    for (const auto& name : install_package_names) {
      base::StrAppend(&result, {"      - ", name, "\n"});
    }
  }

  return result;
}

}  // namespace

PendingSoftwareChanges::PendingSoftwareChanges(
    const SoftwareConfig& new_config,
    const SoftwareConfig& old_config) {
  // We only need keys and sources from the target state, as old can be safely
  // removed from the container.
  key_urls_ = new_config.key_urls();
  source_lines_ = new_config.source_lines();

  // For packages, we calculate diff to process changes (unmarking,
  // installation) individually.
  std::vector<std::string> old_package_names = old_config.package_names();
  std::vector<std::string> new_package_names = new_config.package_names();

  std::sort(old_package_names.begin(), old_package_names.end());
  std::sort(new_package_names.begin(), new_package_names.end());

  std::set_difference(old_package_names.begin(), old_package_names.end(),
                      new_package_names.begin(), new_package_names.end(),
                      std::back_inserter(unmark_package_names_));
  std::set_difference(new_package_names.begin(), new_package_names.end(),
                      old_package_names.begin(), old_package_names.end(),
                      std::back_inserter(install_package_names_));
}

PendingSoftwareChanges::~PendingSoftwareChanges() = default;

std::string PendingSoftwareChanges::ToAnsiblePlaybook() const {
  // Generate playbook sections for keys, sources and packages.
  // These sections are not part of the playbook format, but our separation
  // to make generation more modular.
  // Generation is entirely string-based, as a more structured (e.g. node-based)
  // approach to produce YAML is likely to add extra complexity at this point.
  const std::string keys_section = GenerateKeysSection(key_urls());
  const std::string sources_section = GenerateSourcesSection(source_lines());
  const std::string packages_section =
      GeneratePackagesSection(unmark_package_names(), install_package_names());

  return base::StrCat({kAnsiblePackageManagementPlaybookHeader, keys_section,
                       sources_section, packages_section});
}

}  // namespace crostini
