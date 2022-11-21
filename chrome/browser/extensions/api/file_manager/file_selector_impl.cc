// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_manager/file_selector_impl.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace file_manager {

namespace {

// Converts file extensions to a ui::SelectFileDialog::FileTypeInfo.
ui::SelectFileDialog::FileTypeInfo ConvertExtensionsToFileTypeInfo(
    const std::vector<std::string>& extensions) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;

  for (const auto& extension : extensions) {
    base::FilePath::StringType allowed_extension =
        base::FilePath::FromUTF8Unsafe(extension).value();

    // FileTypeInfo takes a nested vector like [["htm", "html"], ["txt"]] to
    // group equivalent extensions, but we don't use this feature here.
    std::vector<base::FilePath::StringType> inner_vector;
    inner_vector.push_back(allowed_extension);
    file_type_info.extensions.push_back(inner_vector);
  }

  return file_type_info;
}

}  // namespace

/******** FileSelectorImpl ********/

FileSelectorImpl::FileSelectorImpl() = default;

FileSelectorImpl::~FileSelectorImpl() {
  if (dialog_.get())
    dialog_->ListenerDestroyed();
  // Send response if needed.
  if (!callback_.is_null())
    SendResponse(false, base::FilePath());
}

void FileSelectorImpl::SelectFile(
    const base::FilePath& suggested_name,
    const std::vector<std::string>& allowed_extensions,
    Browser* browser,
    FileSelector::OnSelectedCallback callback) {
  callback_ = std::move(callback);

  if (!StartSelectFile(suggested_name, allowed_extensions, browser)) {
    // If dialog didn't launch, asynchronously report failure.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSelectorImpl::FileSelectionCanceled,
                       base::Unretained(this), static_cast<void*>(nullptr)));
  }
}

bool FileSelectorImpl::StartSelectFile(
    const base::FilePath& suggested_name,
    const std::vector<std::string>& allowed_extensions,
    Browser* browser) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!dialog_.get());
  DCHECK(browser);

  if (!browser->window())
    return false;

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  // Early return if the select file dialog is already active.
  if (dialog_)
    return false;

  dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  // Convert |allowed_extensions| to ui::SelectFileDialog::FileTypeInfo.
  ui::SelectFileDialog::FileTypeInfo allowed_file_info =
      ConvertExtensionsToFileTypeInfo(allowed_extensions);
  allowed_file_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      std::u16string() /* dialog title*/, suggested_name, &allowed_file_info,
      0 /* file type index */, std::string() /* default file extension */,
      browser->window()->GetNativeWindow(), nullptr /* params */);

  return dialog_->IsRunning(browser->window()->GetNativeWindow());
}

void FileSelectorImpl::FileSelected(const base::FilePath& path,
                                    int index,
                                    void* params) {
  SendResponse(true, path);
  delete this;
}

void FileSelectorImpl::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  // Only single file should be selected in save-as dialog.
  NOTREACHED();
}

void FileSelectorImpl::FileSelectionCanceled(void* params) {
  SendResponse(false, base::FilePath());
  delete this;
}

void FileSelectorImpl::SendResponse(bool success,
                                    const base::FilePath& selected_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Avoid sending multiple responses.
  if (!callback_.is_null())
    std::move(callback_).Run(success, selected_path);
}

/******** FileSelectorFactoryImpl ********/

FileSelectorFactoryImpl::FileSelectorFactoryImpl() = default;

FileSelectorFactoryImpl::~FileSelectorFactoryImpl() = default;

FileSelector* FileSelectorFactoryImpl::CreateFileSelector() const {
  return new FileSelectorImpl();
}

}  // namespace file_manager
