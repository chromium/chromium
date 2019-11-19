// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_H_

#include <string>
#include <vector>

#include "base/optional.h"

namespace crostini {

// Configuration of the desired container software state (GPG keys,
// APT repositories and packages) parsed from JSON.
//
// JSON schema that we require:
// {
//   "version": 1,
//   "keys": [{"url": "https://foo.bar/baz/key"}, ...],
//   "sources": [{"line": "deb https://foo.bar/deb stable main"}, ...],
//   "packages": [{"name": "foo-tools"}, ...]
// }
//
// This schema is versioned 1; version number might be increased in the future
// if incompatible changes are introduced to the format.
class SoftwareConfig {
 public:
  SoftwareConfig(SoftwareConfig&&);
  ~SoftwareConfig();

  static base::Optional<SoftwareConfig> FromJson(
      const std::string& config_json);

  void SetKeysForTesting(std::vector<std::string> key_urls);
  void SetSourcesForTesting(std::vector<std::string> source_lines);
  void SetPackagesForTesting(std::vector<std::string> package_names);

  const std::vector<std::string>& key_urls() const { return key_urls_; }
  const std::vector<std::string>& source_lines() const { return source_lines_; }
  const std::vector<std::string>& package_names() const {
    return package_names_;
  }

 private:
  SoftwareConfig();

  std::vector<std::string> key_urls_;
  std::vector<std::string> source_lines_;
  std::vector<std::string> package_names_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_H_
