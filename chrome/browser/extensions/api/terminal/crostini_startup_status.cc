// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/crostini_startup_status.h"

#include <algorithm>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chromeos/dbus/util/version_loader.h"
#include "components/version_info/version_info.h"

namespace extensions {

namespace {

const char kCursorHide[] = "\x1b[?25l";
const char kCursorShow[] = "\x1b[?25h";
const char kColor0Normal[] = "\x1b[0m";  // Default.
const char kColor1Red[] = "\x1b[31m";
const char kColor2Green[] = "\x1b[32m";
const char kColor4Blue[] = "\x1b[34m";
const char kColor5Purple[] = "\x1b[35m";
const char kProgressStart[] = "\x1b[7m";  // Invert color.
const char kProgressEnd[] = "\x1b[27m";   // Revert color.
const char kSpinner[] = "|/-\\";
const int kTimestampLength = 25;
const int kMaxProgress = 9;
const base::NoDestructor<std::vector<std::string>> kSuccessEmoji(
    {"😀", "😉", "🤩", "🤪", "😎", "🥳", "👍"});
const base::NoDestructor<std::vector<std::string>> kErrorEmoji({"🤕", "😠",
                                                                "😧", "😢", "😞"});
}  // namespace

CrostiniStartupStatus::CrostiniStartupStatus(
    base::RepeatingCallback<void(const std::string&)> print,
    bool verbose,
    base::OnceClosure callback)
    : print_(std::move(print)),
      verbose_(verbose),
      callback_(std::move(callback)) {
  Print(kCursorHide);
  if (verbose_) {
    PrintWithTimestamp("Chrome OS " + version_info::GetVersionNumber() + " " +
                       chromeos::version_loader::GetVersion(
                           chromeos::version_loader::VERSION_FULL) +
                       "\r\n");
  }
}

CrostiniStartupStatus::~CrostiniStartupStatus() = default;

void CrostiniStartupStatus::OnCrostiniRestarted(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Error starting crostini for terminal: "
               << static_cast<int>(result);
    PrintWithTimestamp(base::StringPrintf(
        "Error starting penguin container: %d %s\r\n", result,
        (*kErrorEmoji)[rand() % kErrorEmoji->size()].c_str()));
  } else {
    if (verbose_) {
      PrintWithTimestamp(base::StringPrintf(
          "Ready %s\r\n",
          (*kSuccessEmoji)[rand() % kSuccessEmoji->size()].c_str()));
    }
    Print(kCursorShow);
  }
  std::move(callback_).Run();
  delete this;
}

void CrostiniStartupStatus::ShowStatusLineAtInterval() {
  ++spinner_index_;
  PrintStatusLine();
  base::PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CrostiniStartupStatus::ShowStatusLineAtInterval,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(300));
}

void CrostiniStartupStatus::OnStageStarted(InstallerState stage) {
  stage_ = stage;
  progress_index_++;
  if (!verbose_) {
    return;
  }
  static base::NoDestructor<base::flat_map<InstallerState, std::string>>
      kStartStrings({
          {InstallerState::kStart, "Starting... 🤔"},
          {InstallerState::kInstallImageLoader,
           "Checking cros-termina component..."},
          {InstallerState::kStartConcierge, "Starting VM controller..."},
          {InstallerState::kCreateDiskImage, "Creating termina VM image..."},
          {InstallerState::kStartTerminaVm, "Starting termina VM..."},
          {InstallerState::kCreateContainer, "Creating penguin container..."},
          {InstallerState::kSetupContainer,
           "Checking penguin container setup..."},
          {InstallerState::kStartContainer, "Starting penguin container..."},
          {InstallerState::kFetchSshKeys,
           "Fetching penguin container ssh keys..."},
          {InstallerState::kMountContainer,
           "Mounting penguin container sshfs..."},
      });
  const std::string& start_string = (*kStartStrings)[stage];
  cursor_position_ = kTimestampLength + start_string.length();
  PrintWithTimestamp(start_string + "\r\n");
  PrintStatusLine();
}

void CrostiniStartupStatus::OnComponentLoaded(crostini::CrostiniResult result) {
  PrintCrostiniResult(result);
}

void CrostiniStartupStatus::OnConciergeStarted(bool success) {
  PrintSuccess(success);
}

void CrostiniStartupStatus::OnDiskImageCreated(
    bool success,
    vm_tools::concierge::DiskImageStatus status,
    int64_t disk_size_available) {
  PrintSuccess(success);
}

void CrostiniStartupStatus::OnVmStarted(bool success) {
  PrintSuccess(success);
}

void CrostiniStartupStatus::OnContainerDownloading(int32_t download_percent) {
  if (download_percent % 8 == 0) {
    PrintResult(".");
  }
}

void CrostiniStartupStatus::OnContainerCreated(
    crostini::CrostiniResult result) {
  PrintCrostiniResult(result);
}

void CrostiniStartupStatus::OnContainerSetup(bool success) {
  PrintSuccess(success);
}

void CrostiniStartupStatus::OnContainerStarted(
    crostini::CrostiniResult result) {
  PrintCrostiniResult(result);
}

void CrostiniStartupStatus::OnSshKeysFetched(bool success) {
  PrintSuccess(success);
}

void CrostiniStartupStatus::OnContainerMounted(bool success) {
  PrintSuccess(success);
}

void CrostiniStartupStatus::PrintStatusLine() {
  std::string progress(progress_index_, ' ');
  std::string dots(std::max(kMaxProgress - progress_index_, 0), '.');
  Print(base::StringPrintf("[%s%s%s%s%s%s] %s%c%s\r", kProgressStart,
                           progress.c_str(), kProgressEnd, kColor5Purple,
                           dots.c_str(), kColor0Normal, kColor4Blue,
                           kSpinner[spinner_index_ & 0x3], kColor0Normal));
}

void CrostiniStartupStatus::Print(const std::string& output) {
  print_.Run(output);
}

void CrostiniStartupStatus::PrintWithTimestamp(const std::string& output) {
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  Print(base::StringPrintf("%04d-%02d-%02d %02d:%02d:%02d.%03d %s",
                           exploded.year, exploded.month, exploded.day_of_month,
                           exploded.hour, exploded.minute, exploded.second,
                           exploded.millisecond, output.c_str()));
}

void CrostiniStartupStatus::PrintResult(const std::string& output) {
  if (!verbose_) {
    return;
  }

  std::string cursor_move = "\x1b[A";  // cursor up.
  for (int i = 0; i < cursor_position_; ++i) {
    cursor_move += "\x1b[C";  // cursor forward.
  }
  Print(cursor_move + output + "\r\n");
  cursor_position_ += output.length();
  PrintStatusLine();
}

void CrostiniStartupStatus::PrintCrostiniResult(
    crostini::CrostiniResult result) {
  if (result == crostini::CrostiniResult::SUCCESS) {
    PrintSuccess(true);
  } else {
    PrintResult(base::StringPrintf("%serror=%d%s ❌", kColor1Red, result,
                                   kColor0Normal));
  }
}

void CrostiniStartupStatus::PrintSuccess(bool success) {
  if (success) {
    PrintResult(
        base::StringPrintf("%sdone%s ✔️", kColor2Green, kColor0Normal));
  } else {
    PrintResult(base::StringPrintf("%serror%s ❌", kColor1Red, kColor0Normal));
  }
}

}  // namespace extensions
