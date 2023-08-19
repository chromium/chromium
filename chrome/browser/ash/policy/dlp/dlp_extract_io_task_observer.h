// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_EXTRACT_IO_TASK_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_EXTRACT_IO_TASK_OBSERVER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"

namespace policy {

// Listens to `ExtractIOTask` events to notify the DLP daemon of extracted
// files.
class DlpExtractIOTaskObserver
    : public file_manager::io_task::IOTaskController::Observer {
 public:
  explicit DlpExtractIOTaskObserver(
      file_manager::io_task::IOTaskController& io_task_controller);
  ~DlpExtractIOTaskObserver() override;

  // file_manager::io_task::IOTaskController::Observer overrides:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override;

 private:
  base::ScopedObservation<file_manager::io_task::IOTaskController,
                          file_manager::io_task::IOTaskController::Observer>
      io_task_controller_observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_EXTRACT_IO_TASK_OBSERVER_H_
