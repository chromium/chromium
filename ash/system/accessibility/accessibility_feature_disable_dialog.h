// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_DISABLE_DIALOG_H_
#define ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_DISABLE_DIALOG_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Defines a dialog for accessibility features that require confirmation from
// users prior to being disabled. For features like automatic clicks and switch
// access, accidentally disabling the feature could cause users to be unable to
// use their devices.
class AccessibilityFeatureDisableDialog : public views::DialogDelegateView {
 public:
  AccessibilityFeatureDisableDialog(int window_title_text_id,
                                    int dialog_text_id,
                                    base::OnceClosure on_accept_callback,
                                    base::OnceClosure on_cancel_callback);
  ~AccessibilityFeatureDisableDialog() override;

  // views::DialogDelegateView:
  bool Cancel() override;
  bool Accept() override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  base::WeakPtr<AccessibilityFeatureDisableDialog> GetWeakPtr();

  // views::View:
  const char* GetClassName() const override;

 private:
  const base::string16 window_title_;
  base::OnceClosure on_accept_callback_;
  base::OnceClosure on_cancel_callback_;

  base::WeakPtrFactory<AccessibilityFeatureDisableDialog> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AccessibilityFeatureDisableDialog);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_DISABLE_DIALOG_H_
