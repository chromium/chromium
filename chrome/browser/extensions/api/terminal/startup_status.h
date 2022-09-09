// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_STARTUP_STATUS_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_STARTUP_STATUS_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace extensions {

class StartupStatusPrinter {
 public:
  explicit StartupStatusPrinter(
      base::RepeatingCallback<void(const std::string& output)> print,
      bool verbose);
  ~StartupStatusPrinter();

  // Starts showing the progress indicator.
  void StartShowingSpinner();

  // Updates the output for a new stage named `stage_name` and number
  // `stage_index`. If `succeeded` is true, indicates that the last stage has
  // completed successfully and we're now in the end-state.
  void PrintStage(int stage_index, const std::string& stage_name);

  // Displays an error message to the user.
  void PrintError(const std::string& output);

  // Displays a successful connection message to the user.
  void PrintSucceeded();

  // Sets the max stage number.
  void set_max_stage(int max_stage) {
    DCHECK(!progress_initialized_);
    max_stage_ = max_stage;
  }

  base::WeakPtr<StartupStatusPrinter> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void Print(const std::string& output);
  void InitializeProgress();
  void PrintProgress();
  void PrintStageWithColor(int stage_index,
                           const char* color,
                           const std::string& stage_name);

  base::RepeatingCallback<void(const std::string& output)> print_;
  const bool verbose_;
  bool progress_initialized_ = false;
  int spinner_index_ = 0;
  int stage_index_ = 1;
  int end_of_line_index_ = 0;
  int max_stage_ = -1;
  std::unique_ptr<base::RepeatingTimer> show_progress_timer_;
  base::WeakPtrFactory<StartupStatusPrinter> weak_factory_{this};
};

class StartupStatus {
 public:
  explicit StartupStatus(std::unique_ptr<StartupStatusPrinter> printer,
                         int max_stage);
  virtual ~StartupStatus();
  void OnConnectingToVsh();
  void OnFinished(bool success, const std::string& failure_reason);
  void StartShowingSpinner();
  StartupStatusPrinter* printer() { return printer_.get(); }

 private:
  std::unique_ptr<StartupStatusPrinter> printer_;
  const int max_stage_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_STARTUP_STATUS_H_
