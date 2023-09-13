// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_WARN_SETTINGS_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_WARN_SETTINGS_H_

#include <string>
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {

// Struct holding optional settings that can be applied to obtain a customized
// warning.
struct FilesPolicyWarnSettings {
  FilesPolicyWarnSettings();
  ~FilesPolicyWarnSettings();
  FilesPolicyWarnSettings(const FilesPolicyWarnSettings& other);
  FilesPolicyWarnSettings& operator=(FilesPolicyWarnSettings&& other);

  bool operator==(const FilesPolicyWarnSettings& other) const;
  bool operator!=(const FilesPolicyWarnSettings& other) const;

  bool bypass_requires_justification;
  absl::optional<std::u16string> warning_message;
  absl::optional<GURL> learn_more_url;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_WARN_SETTINGS_H_
