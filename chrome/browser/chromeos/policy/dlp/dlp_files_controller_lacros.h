// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_LACROS_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class DlpFilesControllerLacros : public DlpFilesController {
 public:
  explicit DlpFilesControllerLacros(const DlpRulesManager& rules_manager);

  ~DlpFilesControllerLacros() override;

 protected:
  // DlpFilesController:
  absl::optional<data_controls::Component> MapFilePathtoPolicyComponent(
      Profile* profile,
      const base::FilePath& file_path) override;

  FRIEND_TEST_ALL_PREFIXES(DlpFilesControllerLacrosTest,
                           MapFilePathtoPolicyComponentTest);
};
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_LACROS_H_
