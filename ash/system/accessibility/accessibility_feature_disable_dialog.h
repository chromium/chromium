// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_DISABLE_DIALOG_H_
#define ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_DISABLE_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Defines a dialog for accessibility features that require confirmation from
// users prior to being disabled. For features like automatic clicks and switch
// access, accidentally disabling the feature could cause users to be unable to
// use their devices.
class AccessibilityFeatureDisableDialog : public views::DialogDelegateView {
  METADATA_HEADER(AccessibilityFeatureDisableDialog, views::DialogDelegateView)
 public:
  AccessibilityFeatureDisableDialog(int window_title_text_id,
                                    base::OnceClosure on_accept_callback,
                                    base::OnceClosure on_cancel_callback);

  AccessibilityFeatureDisableDialog(const AccessibilityFeatureDisableDialog&) =
      delete;
  AccessibilityFeatureDisableDialog& operator=(
      const AccessibilityFeatureDisableDialog&) = delete;

  ~AccessibilityFeatureDisableDialog() override;

  base::WeakPtr<AccessibilityFeatureDisableDialog> GetWeakPtr();

 private:
  base::OnceClosure on_cancel_callback_;

  base::WeakPtrFactory<AccessibilityFeatureDisableDialog> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_DISABLE_DIALOG_H_
