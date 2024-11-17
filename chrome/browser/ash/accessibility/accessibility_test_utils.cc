// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

CaretBoundsChangedWaiter::CaretBoundsChangedWaiter(
    ui::InputMethod* input_method)
    : input_method_(input_method) {
  input_method_->AddObserver(this);
}
CaretBoundsChangedWaiter::~CaretBoundsChangedWaiter() {
  input_method_->RemoveObserver(this);
}

void CaretBoundsChangedWaiter::Wait() {
  run_loop_.Run();
}

void CaretBoundsChangedWaiter::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  run_loop_.Quit();
}

ExtensionConsoleErrorObserver::ExtensionConsoleErrorObserver(
    Profile* profile,
    const char* extension_id) {
  error_console_ = extensions::ErrorConsole::Get(profile);
  profile->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  error_console_->SetReportingForExtension(
      extension_id, extensions::ExtensionError::Type::kRuntimeError,
      true /* enabled */);
  error_console_->SetReportingForExtension(
      extension_id, extensions::ExtensionError::Type::kRuntimeError,
      true /* enabled */);
  error_console_->AddObserver(this);
}

ExtensionConsoleErrorObserver::~ExtensionConsoleErrorObserver() {
  if (error_console_)
    error_console_->RemoveObserver(this);
}

void ExtensionConsoleErrorObserver::OnErrorAdded(
    const extensions::ExtensionError* error) {
  // Add a non-fatal failure to the test. Thus the test can continue
  // executing in case the warning/error is helpful in debugging.
  ADD_FAILURE() << "Found extension console warning or error with message: "
                << error->message();
  errors_.push_back(error->message());
}

void ExtensionConsoleErrorObserver::OnErrorConsoleDestroyed() {
  error_console_ = nullptr;
}

bool ExtensionConsoleErrorObserver::HasErrorsOrWarnings() {
  return !errors_.empty();
}

std::string ExtensionConsoleErrorObserver::GetErrorOrWarningAt(
    size_t index) const {
  return errors_.size() > index ? base::UTF16ToUTF8(errors_[index])
                                : std::string();
}

size_t ExtensionConsoleErrorObserver::GetErrorsAndWarningsCount() const {
  return errors_.size();
}

HistogramWaiter::HistogramWaiter(const char* metric_name) {
  histogram_observer_ =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          metric_name,
          base::BindRepeating(&HistogramWaiter::OnHistogramCallback,
                              base::Unretained(this)));
}

HistogramWaiter::~HistogramWaiter() {
  histogram_observer_.reset();
}

void HistogramWaiter::Wait() {
  run_loop_.Run();
}

void HistogramWaiter::OnHistogramCallback(const char* metric_name,
                                          uint64_t name_hash,
                                          base::HistogramBase::Sample sample) {
  run_loop_.Quit();
  histogram_observer_.reset();
}

MagnifierAnimationWaiter::MagnifierAnimationWaiter(
    FullscreenMagnifierController* controller)
    : controller_(controller) {}

MagnifierAnimationWaiter::~MagnifierAnimationWaiter() = default;

void MagnifierAnimationWaiter::Wait() {
  base::RepeatingTimer check_timer;
  check_timer.Start(FROM_HERE, base::Milliseconds(10), this,
                    &MagnifierAnimationWaiter::OnTimer);
  runner_ = new content::MessageLoopRunner;
  runner_->Run();
}

void MagnifierAnimationWaiter::OnTimer() {
  DCHECK(runner_.get());
  if (!controller_->IsOnAnimationForTesting()) {
    runner_->Quit();
  }
}

}  // namespace ash
