// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONFIRMATION_DIALOG_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONFIRMATION_DIALOG_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Defines a dialog for accelerators that require confirmation from users prior
// to perform.
class AcceleratorConfirmationDialog : public views::DialogDelegateView {
 public:
  AcceleratorConfirmationDialog(int window_title_text_id,
                                int dialog_text_id,
                                base::OnceClosure on_accept_callback,
                                base::OnceClosure on_cancel_callback);
  ~AcceleratorConfirmationDialog() override;

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  base::WeakPtr<AcceleratorConfirmationDialog> GetWeakPtr();

 private:
  const base::string16 window_title_;
  base::OnceClosure on_accept_callback_;
  base::OnceClosure on_cancel_callback_;

  base::WeakPtrFactory<AcceleratorConfirmationDialog> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AcceleratorConfirmationDialog);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONFIRMATION_DIALOG_H_
