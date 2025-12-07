// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/load_error_reporter.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

LoadErrorReporter* LoadErrorReporter::instance_ = nullptr;

// static
void LoadErrorReporter::Init(bool enable_noisy_errors) {
  if (!instance_) {
    instance_ = new LoadErrorReporter(enable_noisy_errors);
  }
}

// static
LoadErrorReporter* LoadErrorReporter::GetInstance() {
  CHECK(instance_) << "Init() was never called";
  return instance_;
}

LoadErrorReporter::LoadErrorReporter(bool enable_noisy_errors)
    : enable_noisy_errors_(enable_noisy_errors) {
  if (base::SingleThreadTaskRunner::HasCurrentDefault())
    ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

LoadErrorReporter::~LoadErrorReporter() = default;

void LoadErrorReporter::ReportLoadError(
    const base::FilePath& extension_path,
    const std::u16string& error,
    content::BrowserContext* browser_context,
    bool be_noisy) {
  std::u16string message =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE) + u" " +
      extension_path.LossyDisplayName() + u". " + error;
  ReportError(message, be_noisy);
  for (auto& observer : observers_)
    observer.OnLoadFailure(browser_context, extension_path, error);
}

void LoadErrorReporter::ReportError(const std::u16string& message,
                                    bool be_noisy) {
  // NOTE: There won't be a |ui_task_runner_| in the unit test environment.
  CHECK(!ui_task_runner_ || ui_task_runner_->BelongsToCurrentThread())
      << "ReportError can only be called from the UI thread.";

  errors_.push_back(message);

  // TODO(aa): Print the error message out somewhere better. I think we are
  // going to need some sort of 'extension inspector'.
  LOG(WARNING) << "Extension error: " << message;

  if (enable_noisy_errors_ && be_noisy) {
    // TODO(crbug.com/425390966): Find a way to make this dialog asynchronous
    // so that we don't block the main thread.
    //
    // This dialog is synchronous to prevent a race condition during startup.
    //
    // In the asynchronous case, the sequence of events is:
    // 1. A startup task to load an extension fails, and an asynchronous call
    //    is made to show this parentless dialog.
    // 2. The dialog's widget initializes, registering an accessibility observer
    //    with `AXPlatform`. The async call then returns immediately, marking
    //    the startup task as complete.
    // 3. Because the startup task is finished and no windows are open, the
    //    browser process begins its shutdown sequence.
    // 4. During shutdown, `AXPlatform` is destroyed before the dialog is. Its
    //    destructor's `CHECK` for no remaining observers fails because the
    //    dialog's observer is still registered, causing a crash.
    //
    // By using a synchronous dialog, we block the startup task from completing
    // until the user dismisses the alert, ensuring steps 3 and 4 cannot
    // happen until after the dialog and its observers are gone.
    chrome::ShowWarningMessageBoxSync(
        gfx::NativeWindow(),
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ERROR_ALERT_HEADING),
        message);
  }
}

const std::vector<std::u16string>* LoadErrorReporter::GetErrors() {
  return &errors_;
}

void LoadErrorReporter::ClearErrors() {
  errors_.clear();
}

void LoadErrorReporter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LoadErrorReporter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace extensions
