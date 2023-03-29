// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_DESTINATION_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_DESTINATION_H_

#include <string>

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
// DlpFileDestination represents the destination for file transfer. It either
// has a url or a component.
class DlpFileDestination {
 public:
  DlpFileDestination() = delete;
  explicit DlpFileDestination(const std::string& url);
  explicit DlpFileDestination(const DlpRulesManager::Component component);

  DlpFileDestination(const DlpFileDestination&);
  DlpFileDestination& operator=(const DlpFileDestination&);
  DlpFileDestination(DlpFileDestination&&);
  DlpFileDestination& operator=(DlpFileDestination&&);

  bool operator==(const DlpFileDestination&) const;
  bool operator!=(const DlpFileDestination&) const;
  bool operator<(const DlpFileDestination& other) const;
  bool operator<=(const DlpFileDestination& other) const;
  bool operator>(const DlpFileDestination& other) const;
  bool operator>=(const DlpFileDestination& other) const;

  ~DlpFileDestination();

  absl::optional<std::string> url_or_path() const;

  absl::optional<DlpRulesManager::Component> component() const;

 private:
  // Destination url or destination path.
  absl::optional<std::string> url_or_path_;
  // Destination component.
  absl::optional<DlpRulesManager::Component> component_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_DESTINATION_H_
