// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_CONFIRMATION_MODAL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_CONFIRMATION_MODAL_H_

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/views/window/dialog_delegate.h"

namespace enterprise_connectors {
class SigninExperienceTestObserver;

class FileSystemConfirmationModal : public views::DialogDelegate {
 public:
  using Callback = base::OnceCallback<void(bool)>;
  ~FileSystemConfirmationModal() override;

  static void Show(gfx::NativeWindow context,
                   const std::u16string& title,
                   const std::u16string& message,
                   const std::u16string& cancel_button,
                   const std::u16string& accept_button,
                   Callback callback,
                   SigninExperienceTestObserver* observer = nullptr);

  // WidgetDelegate:
  ui::ModalType GetModalType() const override;
  // Title and icon.
  std::u16string GetWindowTitle() const override;
  ui::ImageModel GetWindowIcon() override;

 private:
  FileSystemConfirmationModal(const std::u16string& title,
                              const std::u16string& message,
                              const std::u16string& cancel_button,
                              const std::u16string& accept_button,
                              Callback callback);

  void OnCancellation() { std::move(callback_).Run(false); }
  void OnConfirmation() { std::move(callback_).Run(true); }

  const std::u16string title_, message_;
  Callback callback_;
  base::WeakPtrFactory<FileSystemConfirmationModal> weak_factory_{this};

  friend class FileSystemConfirmationModalTest;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_CONFIRMATION_MODAL_H_
