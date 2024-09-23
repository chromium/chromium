// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_MULTIPROFILES_INTRO_DIALOG_H_
#define ASH_SESSION_MULTIPROFILES_INTRO_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
}

namespace ash {

// Dialog for multi-profiles introduction/confirmation.
class MultiprofilesIntroDialog : public views::DialogDelegateView {
 public:
  // This callback and its parameters match
  // SessionControllerImpl::ShowMultiprofilesIntroDialogCallback.
  typedef base::OnceCallback<void(bool, bool)> OnAcceptCallback;

  static void Show(OnAcceptCallback on_accept);

  MultiprofilesIntroDialog(const MultiprofilesIntroDialog&) = delete;
  MultiprofilesIntroDialog& operator=(const MultiprofilesIntroDialog&) = delete;

  // views::View overrides.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  explicit MultiprofilesIntroDialog(OnAcceptCallback on_accept);
  ~MultiprofilesIntroDialog() override;

  void InitDialog();

  raw_ptr<views::Checkbox> never_show_again_checkbox_;
  OnAcceptCallback on_accept_;
};

}  // namespace ash

#endif  // ASH_SESSION_MULTIPROFILES_INTRO_DIALOG_H_
