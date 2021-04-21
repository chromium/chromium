// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_CROSTINI_STARTUP_STATUS_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_CROSTINI_STARTUP_STATUS_H_

#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"

namespace extensions {

// Displays startup status to the crostini terminal.
class CrostiniStartupStatus
    : public crostini::CrostiniManager::RestartObserver {
 public:
  CrostiniStartupStatus(base::RepeatingCallback<void(const std::string&)> print,
                        bool verbose);
  ~CrostiniStartupStatus() override;

  // Updates the progress spinner every 300ms.
  void ShowProgressAtInterval();

  // Called when startup is complete.
  void OnCrostiniRestarted(crostini::CrostiniResult result);

 private:
  FRIEND_TEST_ALL_PREFIXES(CrostiniStartupStatusTest, TestNotVerbose);
  FRIEND_TEST_ALL_PREFIXES(CrostiniStartupStatusTest, TestVerbose);

  // crostini::CrostiniManager::RestartObserver
  void OnStageStarted(crostini::mojom::InstallerState stage) override;
  void OnContainerDownloading(int32_t download_percent) override;

  void Print(const std::string& output);
  void InitializeProgress();
  void PrintProgress();
  void PrintStage(const char* color, const std::string& output);
  void PrintAfterStage(const char* color, const std::string& output);

  base::RepeatingCallback<void(const std::string& output)> print_;
  const bool verbose_;
  bool progress_initialized_ = false;
  int spinner_index_ = 0;
  int stage_index_ = 0;
  int end_of_line_index_ = 0;

  base::WeakPtrFactory<CrostiniStartupStatus> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_CROSTINI_STARTUP_STATUS_H_
