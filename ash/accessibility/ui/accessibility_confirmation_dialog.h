// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_CONFIRMATION_DIALOG_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_CONFIRMATION_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Defines a dialog for accessibility that require confirmation from users prior
// to performing some action.
class AccessibilityConfirmationDialog : public views::DialogDelegateView {
 public:
  AccessibilityConfirmationDialog(const std::u16string& window_title_text,
                                  const std::u16string& dialog_text,
                                  const std::u16string& confirm_text,
                                  const std::u16string& cancel_text,
                                  base::OnceClosure on_accept_callback,
                                  base::OnceClosure on_cancel_callback,
                                  base::OnceClosure on_close_callback);
  ~AccessibilityConfirmationDialog() override;
  AccessibilityConfirmationDialog(const AccessibilityConfirmationDialog&) =
      delete;
  AccessibilityConfirmationDialog& operator=(
      const AccessibilityConfirmationDialog&) = delete;

  // views::DialogDelegate:
  bool ShouldShowCloseButton() const override;

  base::WeakPtr<AccessibilityConfirmationDialog> GetWeakPtr();

 private:
  base::WeakPtrFactory<AccessibilityConfirmationDialog> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_CONFIRMATION_DIALOG_H_
