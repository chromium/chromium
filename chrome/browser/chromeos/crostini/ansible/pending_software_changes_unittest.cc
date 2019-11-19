// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/pending_software_changes.h"

#include "base/strings/string_split.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace crostini {

TEST(PendingSoftwareChangesTest, ChangeCalculationWorks) {
  auto old_config = SoftwareConfig::FromJson("").value();
  old_config.SetKeysForTesting({"oldkey1", "oldkey2"});
  old_config.SetSourcesForTesting({"oldsrc"});
  old_config.SetPackagesForTesting({"oldpackage", "relevantpackage"});

  auto new_config = SoftwareConfig::FromJson("").value();
  new_config.SetKeysForTesting({"newkey"});
  new_config.SetSourcesForTesting({"newsrc1", "newsrc2"});
  new_config.SetPackagesForTesting({"relevantpackage", "newpackage"});

  const PendingSoftwareChanges changes(new_config, old_config);

  EXPECT_THAT(changes.key_urls(), testing::ElementsAre("newkey"));
  EXPECT_THAT(changes.source_lines(),
              testing::ElementsAre("newsrc1", "newsrc2"));
  EXPECT_THAT(changes.unmark_package_names(),
              testing::ElementsAre("oldpackage"));
  EXPECT_THAT(changes.install_package_names(),
              testing::ElementsAre("newpackage"));
}

TEST(PendingSoftwareChangesTest, GeneratedOutputIsCorrect) {
  auto old_config = SoftwareConfig::FromJson("").value();
  old_config.SetPackagesForTesting({"qbar", "libbaz-demo", "relevantpackage"});

  auto new_config = SoftwareConfig::FromJson("").value();
  new_config.SetKeysForTesting(
      {"https://example.com/apt/gpgkey", "https://foobar.de/key.asc"});
  new_config.SetSourcesForTesting(
      {"deb https://test.io/BestEditor/software/any/ any main",
       "deb-src https://foo.bar/repo/src/ yummy contrib"});
  new_config.SetPackagesForTesting(
      {"foo", "foo-tools", "libbf5bazserver5", "relevantpackage"});

  const PendingSoftwareChanges changes(new_config, old_config);
  const std::string generated = changes.ToAnsiblePlaybook();

  // clang-format off
  const char expected[] =
R"(---
- hosts: localhost
  become: yes
  tasks:
  # Key configuration
  - name: Remove existing keys
    file:
      path: /etc/apt/trusted.gpg.d/managedcrostini.gpg
      state: absent
  - name: Add GPG key https://example.com/apt/gpgkey
    apt_key:
      url: https://example.com/apt/gpgkey
      keyring: /etc/apt/trusted.gpg.d/managedcrostini.gpg
  - name: Add GPG key https://foobar.de/key.asc
    apt_key:
      url: https://foobar.de/key.asc
      keyring: /etc/apt/trusted.gpg.d/managedcrostini.gpg
  # Source configuration
  - name: Remove existing sources
    file:
      path: /etc/apt/sources.list.d/managedcrostini.list
      state: absent
  - name: Add APT source "deb https://test.io/BestEditor/software/any/ any main"
    apt_repository:
      repo: deb https://test.io/BestEditor/software/any/ any main
      filename: managedcrostini
  - name: Add APT source "deb-src https://foo.bar/repo/src/ yummy contrib"
    apt_repository:
      repo: deb-src https://foo.bar/repo/src/ yummy contrib
      filename: managedcrostini
  # Package configuration
  - name: Unmark unselected packages
    shell: apt-mark auto {{ packages|join(' ') }}
    vars:
      packages:
      - libbaz-demo
      - qbar
  - name: Remove packages that are no longer needed
    apt: autoremove=yes
  - name: Install packages
    apt:
      name: "{{ packages }}"
      update_cache: yes
    vars:
      packages:
      - foo
      - foo-tools
      - libbf5bazserver5
)";
  // clang-format on

  // Compare expected and generated playbooks line by line
  const auto expected_tokens = base::SplitString(
      expected, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  const auto generated_tokens = base::SplitString(
      generated, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  EXPECT_THAT(expected_tokens, testing::ContainerEq(generated_tokens));
}

}  // namespace crostini
