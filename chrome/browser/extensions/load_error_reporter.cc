// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/load_error_reporter.h"

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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

LoadErrorReporter::~LoadErrorReporter() {}

void LoadErrorReporter::ReportLoadError(
    const base::FilePath& extension_path,
    const std::string& error,
    content::BrowserContext* browser_context,
    bool be_noisy) {
  std::string path_str = base::UTF16ToUTF8(extension_path.LossyDisplayName());
  std::u16string message = base::UTF8ToUTF16(base::StringPrintf(
      "%s %s. %s",
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_LOAD_ERROR_MESSAGE).c_str(),
      path_str.c_str(), error.c_str()));
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
    chrome::ShowWarningMessageBox(
        nullptr,
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
