// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_open_prompt.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Dialog class for prompting users about opening a download.
class DownloadOpenConfirmationDialog : public DownloadOpenPrompt,
                                       public TabModalConfirmDialogDelegate {
 public:
  DownloadOpenConfirmationDialog(
      content::WebContents* web_contents,
      const std::string& extension_name,
      const base::FilePath& file_path,
      DownloadOpenPrompt::OpenCallback open_callback);

  DownloadOpenConfirmationDialog(const DownloadOpenConfirmationDialog&) =
      delete;
  DownloadOpenConfirmationDialog& operator=(
      const DownloadOpenConfirmationDialog&) = delete;

  ~DownloadOpenConfirmationDialog() override;

  std::u16string GetTitle() override;
  std::u16string GetDialogMessage() override;
  std::u16string GetAcceptButtonTitle() override;
  std::u16string GetCancelButtonTitle() override;

 private:
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  DownloadOpenPrompt::OpenCallback open_callback_;

  std::string extension_name_;

  base::FilePath file_path_;
};

DownloadOpenConfirmationDialog::DownloadOpenConfirmationDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const base::FilePath& file_path,
    DownloadOpenPrompt::OpenCallback open_callback)
    : TabModalConfirmDialogDelegate(web_contents),
      open_callback_(std::move(open_callback)),
      extension_name_(extension_name),
      file_path_(file_path) {}

DownloadOpenConfirmationDialog::~DownloadOpenConfirmationDialog() = default;

std::u16string DownloadOpenConfirmationDialog::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_DOWNLOAD_OPEN_CONFIRMATION_DIALOG_TITLE);
}

std::u16string DownloadOpenConfirmationDialog::GetDialogMessage() {
  return l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_OPEN_CONFIRMATION_DIALOG_MESSAGE,
      base::UTF8ToUTF16(extension_name_),
      file_path_.BaseName().AsUTF16Unsafe());
}

std::u16string DownloadOpenConfirmationDialog::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
}

std::u16string DownloadOpenConfirmationDialog::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
}

void DownloadOpenConfirmationDialog::OnAccepted() {
  std::move(open_callback_).Run(true);
}

void DownloadOpenConfirmationDialog::OnCanceled() {
  std::move(open_callback_).Run(false);
}

void DownloadOpenConfirmationDialog::OnClosed() {
  std::move(open_callback_).Run(false);
}

}  // namespace

DownloadOpenPrompt::DownloadOpenPrompt() = default;

DownloadOpenPrompt::~DownloadOpenPrompt() = default;

DownloadOpenPrompt* DownloadOpenPrompt::CreateDownloadOpenConfirmationDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const base::FilePath& file_path,
    DownloadOpenPrompt::OpenCallback open_callback) {
  auto prompt = std::make_unique<DownloadOpenConfirmationDialog>(
      web_contents, extension_name, file_path, std::move(open_callback));
  DownloadOpenConfirmationDialog* prompt_observer = prompt.get();
  TabModalConfirmDialog::Create(std::move(prompt), web_contents);
  return prompt_observer;
}

void DownloadOpenPrompt::AcceptConfirmationDialogForTesting(
    DownloadOpenPrompt* download_open_prompt) {
  static_cast<DownloadOpenConfirmationDialog*>(download_open_prompt)->Accept();
}
