// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace policy {

// DlpFilesController is responsible for deciding whether file transfers are
// allowed according to the files sources saved in the DLP daemon and the rules
// of the Data leak prevention policy set by the admin.
class DlpFilesController {
 public:
  // Types of file actions. These actions are used when warning dialogs are
  // shown because of files restrictions. This is used in UMA histograms, should
  // not change order.
  enum class FileAction {
    kUnknown = 0,
    kDownload = 1,
    kTransfer = 2,
    kUpload = 3,
    kCopy = 4,
    kMove = 5,
    kOpen = 6,
    kShare = 7,
    kMaxValue = kShare
  };

  DlpFilesController(const DlpFilesController& other) = delete;
  DlpFilesController& operator=(const DlpFilesController& other) = delete;

  virtual ~DlpFilesController();

  static constexpr bool kCopyTaskFlowEnabled = false;

 protected:
  explicit DlpFilesController(const DlpRulesManager& rules_manager);

  const raw_ref<const DlpRulesManager, ExperimentalAsh> rules_manager_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_
