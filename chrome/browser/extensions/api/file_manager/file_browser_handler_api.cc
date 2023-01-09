// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file implements the main flow of the extension function
// fileBrowserHandler.selectFile(). Key steps:
// * Verify that the extension function was invoked as a result of user gesture.
// * Display "Save as" dialog using FileSelectorImpl, awaiting user feedback.
// * On dialog closure (from selection or cancellation), call
//   OnFilePathSelected(), which is implemented by derived class.
// * The derived class' OnFilePathSelected() is expected to:
//   * On cancel: Call RespondWithFailure().
//   * Perform platform-dependent processing, e.g., grant permissions.
//   * On failure: Call RespondWithFailure().
//   * On success: Call RespondWithResult(), passing results.

#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/file_manager/file_selector_impl.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/ui/browser.h"

using file_manager::FileSelector;
using file_manager::FileSelectorFactory;
using file_manager::FileSelectorFactoryImpl;

namespace SelectFile =
    extensions::api::file_browser_handler_internal::SelectFile;

namespace {

const char kNoUserGestureError[] =
    "This method can only be called in response to user gesture, such as a "
    "mouse click or key press.";

}  // namespace

FileBrowserHandlerInternalSelectFileFunction::
    FileBrowserHandlerInternalSelectFileFunction()
    : file_selector_factory_(new FileSelectorFactoryImpl()),
      user_gesture_check_enabled_(true) {}

FileBrowserHandlerInternalSelectFileFunction::
    FileBrowserHandlerInternalSelectFileFunction(
        FileSelectorFactory* file_selector_factory,
        bool enable_user_gesture_check)
    : file_selector_factory_(file_selector_factory),
      user_gesture_check_enabled_(enable_user_gesture_check) {
  DCHECK(file_selector_factory);
}

FileBrowserHandlerInternalSelectFileFunction::
    ~FileBrowserHandlerInternalSelectFileFunction() = default;

ExtensionFunction::ResponseAction
FileBrowserHandlerInternalSelectFileFunction::Run() {
  std::unique_ptr<SelectFile::Params> params(
      SelectFile::Params::Create(args()));

  base::FilePath suggested_name(params->selection_params.suggested_name);
  std::vector<std::string> allowed_extensions;
  if (params->selection_params.allowed_file_extensions)
    allowed_extensions = *params->selection_params.allowed_file_extensions;

  if (!user_gesture() && user_gesture_check_enabled_) {
    return RespondNow(Error(kNoUserGestureError));
  }

  FileSelector* file_selector = file_selector_factory_->CreateFileSelector();
  auto callback = base::BindOnce(
      &FileBrowserHandlerInternalSelectFileFunction::OnFilePathSelected, this);
  file_selector->SelectFile(
      suggested_name.BaseName(), allowed_extensions,
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser(),
      std::move(callback));
  return RespondLater();
}

void FileBrowserHandlerInternalSelectFileFunction::RespondWithFailure() {
  SelectFile::Results::Result result;
  result.success = false;
  RespondWithResult(result);
}

void FileBrowserHandlerInternalSelectFileFunction::RespondWithResult(
    const SelectFile::Results::Result& result) {
  Respond(ArgumentList(SelectFile::Results::Create(result)));
}
