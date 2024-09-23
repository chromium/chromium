// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_SHUTDOWN_CONFIRMATION_DIALOG_H_
#define ASH_SYSTEM_SESSION_SHUTDOWN_CONFIRMATION_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}

namespace ash {

// Defines a dialog for shutdown that require confirmation from users -
// more specifically for the situation where the subsequent boot is slow.
class ShutdownConfirmationDialog : public views::DialogDelegateView {
 public:
  ShutdownConfirmationDialog(int window_title_text_id,
                             int dialog_text_id,
                             base::OnceClosure on_accept_callback,
                             base::OnceClosure on_cancel_callback);

  ShutdownConfirmationDialog(const ShutdownConfirmationDialog&) = delete;
  ShutdownConfirmationDialog& operator=(const ShutdownConfirmationDialog&) =
      delete;

  ~ShutdownConfirmationDialog() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  raw_ptr<views::Label> label_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_SHUTDOWN_CONFIRMATION_DIALOG_H_
