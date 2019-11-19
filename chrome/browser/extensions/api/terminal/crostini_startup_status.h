// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_CROSTINI_STARTUP_STATUS_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_CROSTINI_STARTUP_STATUS_H_

#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"

using crostini::mojom::InstallerState;

namespace extensions {

// Displays startup status to the crostini terminal.
class CrostiniStartupStatus
    : public crostini::CrostiniManager::RestartObserver {
 public:
  CrostiniStartupStatus(base::RepeatingCallback<void(const std::string&)> print,
                        bool verbose,
                        base::OnceClosure callback);
  ~CrostiniStartupStatus() override;

  // Updates the status line every 300ms.
  void ShowStatusLineAtInterval();

  // Deletes this object when called.
  void OnCrostiniRestarted(crostini::CrostiniResult result);

 private:
  FRIEND_TEST_ALL_PREFIXES(CrostiniStartupStatusTest, TestNotVerbose);
  FRIEND_TEST_ALL_PREFIXES(CrostiniStartupStatusTest, TestVerbose);

  // crostini::CrostiniManager::RestartObserver
  void OnStageStarted(InstallerState stage) override;
  void OnComponentLoaded(crostini::CrostiniResult result) override;
  void OnConciergeStarted(bool success) override;
  void OnDiskImageCreated(bool success,
                          vm_tools::concierge::DiskImageStatus status,
                          int64_t disk_size_available) override;
  void OnVmStarted(bool success) override;
  void OnContainerDownloading(int32_t download_percent) override;
  void OnContainerCreated(crostini::CrostiniResult result) override;
  void OnContainerSetup(bool success) override;
  void OnContainerStarted(crostini::CrostiniResult result) override;
  void OnSshKeysFetched(bool success) override;
  void OnContainerMounted(bool success) override;

  void PrintStatusLine();
  void Print(const std::string& output);
  void PrintWithTimestamp(const std::string& output);
  // Moves cursor up and to the right to previous line before status line before
  // printing output.
  void PrintResult(const std::string& output);
  void PrintCrostiniResult(crostini::CrostiniResult result);
  void PrintSuccess(bool success);

  base::RepeatingCallback<void(const std::string& output)> print_;
  const bool verbose_;
  base::OnceClosure callback_;
  int spinner_index_ = 0;
  int progress_index_ = 0;
  // Position of cursor on line above status line.
  int cursor_position_ = 0;
  InstallerState stage_ = InstallerState::kStart;

  base::WeakPtrFactory<CrostiniStartupStatus> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_CROSTINI_STARTUP_STATUS_H_
