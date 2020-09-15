// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/crostini_startup_status.h"

#include <algorithm>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/util/version_loader.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

using crostini::mojom::InstallerState;

namespace extensions {

namespace {

const char kCursorHide[] = "\x1b[?25l";
const char kCursorShow[] = "\x1b[?25h";
const char kColor0Normal[] = "\x1b[0m";  // Default.
const char kColor1RedBright[] = "\x1b[1;31m";
const char kColor2GreenBright[] = "\x1b[1;32m";
const char kColor3Yellow[] = "\x1b[33m";
const char kColor5Purple[] = "\x1b[35m";
const char kEraseInLine[] = "\x1b[K";
const char kSpinner[] = "|/-\\";
const int kMaxStage = 9;

std::string MoveForward(int i) {
  return base::StringPrintf("\x1b[%dC", i);
}

}  // namespace

CrostiniStartupStatus::CrostiniStartupStatus(
    base::RepeatingCallback<void(const std::string&)> print,
    bool verbose)
    : print_(std::move(print)), verbose_(verbose) {
}

CrostiniStartupStatus::~CrostiniStartupStatus() = default;

void CrostiniStartupStatus::OnCrostiniRestarted(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    PrintAfterStage(
        kColor1RedBright,
        base::StringPrintf("Error starting penguin container: %d\r\n", result));
  } else {
    if (verbose_) {
      stage_index_ = kMaxStage + 1;  // done.
      PrintStage(kColor2GreenBright,
                 base::StrCat({l10n_util::GetStringUTF8(
                                   IDS_CROSTINI_TERMINAL_STATUS_READY),
                               "\r\n"}));
    }
  }
  Print(
      base::StringPrintf("\r%s%s%s", kEraseInLine, kColor0Normal, kCursorShow));
}

void CrostiniStartupStatus::ShowProgressAtInterval() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Wait one interval before showing progress.
  if (spinner_index_ > 0) {
    PrintProgress();
  }
  ++spinner_index_;
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CrostiniStartupStatus::ShowProgressAtInterval,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(300));
}

void CrostiniStartupStatus::OnStageStarted(InstallerState stage) {
  stage_ = stage;
  if (stage_index_ < kMaxStage) {
    ++stage_index_;
  }
  if (!verbose_) {
    return;
  }
  static base::NoDestructor<base::flat_map<InstallerState, std::string>>
      kStartStrings({
          {InstallerState::kStart,
           l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_STATUS_START)},
          {InstallerState::kInstallImageLoader,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_INSTALL_IMAGE_LOADER)},
          {InstallerState::kStartConcierge,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_START_CONCIERGE)},
          {InstallerState::kCreateDiskImage,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_CREATE_DISK_IMAGE)},
          {InstallerState::kStartTerminaVm,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_START_TERMINA_VM)},
          {InstallerState::kCreateContainer,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_CREATE_CONTAINER)},
          {InstallerState::kSetupContainer,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_SETUP_CONTAINER)},
          {InstallerState::kStartContainer,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_START_CONTAINER)},
          {InstallerState::kFetchSshKeys,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_FETCH_SSH_KEYS)},
          {InstallerState::kMountContainer,
           l10n_util::GetStringUTF8(
               IDS_CROSTINI_TERMINAL_STATUS_MOUNT_CONTAINER)},
      });
  const std::string& stage_string = (*kStartStrings)[stage];
  PrintStage(kColor3Yellow, stage_string);
}

void CrostiniStartupStatus::OnContainerDownloading(int32_t download_percent) {
  if (download_percent % 8 == 0) {
    PrintAfterStage(kColor3Yellow, ".");
  }
}

void CrostiniStartupStatus::Print(const std::string& output) {
  print_.Run(output);
}

void CrostiniStartupStatus::InitializeProgress() {
  if (progress_initialized_) {
    return;
  }
  progress_initialized_ = true;
  Print(base::StringPrintf("%s%s[%s] ", kCursorHide, kColor5Purple,
                           std::string(kMaxStage, ' ').c_str()));
}

void CrostiniStartupStatus::PrintProgress() {
  InitializeProgress();
  Print(base::StringPrintf("\r%s%s%c", MoveForward(stage_index_).c_str(),
                           kColor5Purple, kSpinner[spinner_index_ & 0x3]));
}

void CrostiniStartupStatus::PrintStage(const char* color,
                                       const std::string& output) {
  DCHECK_GE(stage_index_, 1);
  InitializeProgress();
  std::string progress(stage_index_ - 1, '=');
  Print(base::StringPrintf("\r%s[%s%s%s%s%s ", kColor5Purple, progress.c_str(),
                           MoveForward(3 + (kMaxStage - stage_index_)).c_str(),
                           kEraseInLine, color, output.c_str()));
  end_of_line_index_ = 4 + kMaxStage + output.size();
}

void CrostiniStartupStatus::PrintAfterStage(const char* color,
                                            const std::string& output) {
  InitializeProgress();
  Print(base::StringPrintf("\r%s%s%s", MoveForward(end_of_line_index_).c_str(),
                           color, output.c_str()));
  end_of_line_index_ += output.size();
}

}  // namespace extensions
