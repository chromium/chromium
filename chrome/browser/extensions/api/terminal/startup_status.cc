// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/terminal/startup_status.h"
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

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
const char kSpinnerCharacters[] = "|/-\\";

std::string MoveForward(int i) {
  return base::StringPrintf("\x1b[%dC", i);
}

}  // namespace

StartupStatusPrinter::StartupStatusPrinter(
    base::RepeatingCallback<void(const std::string& output)> print,
    bool verbose)
    : print_(std::move(print)), verbose_(verbose) {}

StartupStatusPrinter::~StartupStatusPrinter() = default;

// Starts showing the progress indicator.
void StartupStatusPrinter::StartShowingSpinner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  show_progress_timer_ = std::make_unique<base::RepeatingTimer>();
  show_progress_timer_->Start(
      FROM_HERE, base::Milliseconds(300),
      base::BindRepeating(&StartupStatusPrinter::PrintProgress,
                          // We own the timer, so this'll never get called after
                          // we're destroyed.
                          base::Unretained(this)));
}

void StartupStatusPrinter::PrintStageWithColor(int stage_index,
                                               const char* color,
                                               const std::string& stage_name) {
  DCHECK_GE(stage_index, 0);
  DCHECK_LE(stage_index, max_stage_);
  InitializeProgress();
  stage_index_ = stage_index;
  auto output = verbose_ ? stage_name : "";
  std::string progress(stage_index_, '=');
  std::string padding(max_stage_ - stage_index_, ' ');
  Print(base::StringPrintf("\r%s[%s%s] %s%s%s ", kColor5Purple,
                           progress.c_str(), padding.c_str(), kEraseInLine,
                           color, output.c_str()));
  end_of_line_index_ = 4 + max_stage_ + output.size();
}

void StartupStatusPrinter::PrintStage(int stage_index,
                                      const std::string& stage_name) {
  PrintStageWithColor(stage_index, kColor3Yellow, stage_name);
}

void StartupStatusPrinter::PrintError(const std::string& output) {
  InitializeProgress();
  Print(base::StringPrintf("\r%s%s%s", MoveForward(end_of_line_index_).c_str(),
                           kColor1RedBright, output.c_str()));
  end_of_line_index_ += output.size();
  Print(
      base::StringPrintf("\r%s%s%s", kEraseInLine, kColor0Normal, kCursorShow));
}

void StartupStatusPrinter::PrintSucceeded() {
  InitializeProgress();
  if (verbose_) {
    auto output = base::StrCat(
        {l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_STATUS_READY), "\r\n"});
    PrintStageWithColor(max_stage_, kColor2GreenBright, output);
  }
  Print(
      base::StringPrintf("\r%s%s%s", kEraseInLine, kColor0Normal, kCursorShow));
}

void StartupStatusPrinter::Print(const std::string& output) {
  print_.Run(output);
}

void StartupStatusPrinter::InitializeProgress() {
  if (progress_initialized_) {
    return;
  }
  progress_initialized_ = true;
  Print(base::StringPrintf("%s%s[%s] ", kCursorHide, kColor5Purple,
                           std::string(max_stage_, ' ').c_str()));
}

void StartupStatusPrinter::PrintProgress() {
  InitializeProgress();
  spinner_index_++;
  Print(base::StringPrintf("\r%s%s%c", MoveForward(stage_index_).c_str(),
                           kColor5Purple,
                           kSpinnerCharacters[spinner_index_ & 0x3]));
}

StartupStatus::StartupStatus(std::unique_ptr<StartupStatusPrinter> printer,
                             int max_stage)
    : printer_(std::move(printer)), max_stage_(max_stage) {
  printer_->set_max_stage(max_stage);
}

StartupStatus::~StartupStatus() = default;

void StartupStatus::OnConnectingToVsh() {
  const std::string& stage_string =
      l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_STATUS_CONNECT_CONTAINER);
  printer()->PrintStage(max_stage_, stage_string);
}
void StartupStatus::StartShowingSpinner() {
  printer()->StartShowingSpinner();
}

void StartupStatus::OnFinished(bool success,
                               const std::string& failure_reason) {
  if (success) {
    printer()->PrintSucceeded();
  } else {
    printer()->PrintError(failure_reason);
  }
}

}  // namespace extensions
