// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
#define ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_

#include "base/callback.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// RemoveQueryConfirmationDialog displays the confirmation dialog for removing
// a recent query suggestion.
class RemoveQueryConfirmationDialog : public views::DialogDelegateView {
 public:
  // Callback to notify user's confirmation for removing the zero state
  // suggestion query. Invoked with true if user confirms removing query
  // suggestion; and false for declining the removal. The second parameter is
  // the event flags of user action for invoking the removal action on the
  // associated result.
  using RemovalConfirmationCallback = base::OnceCallback<void(bool)>;

  RemoveQueryConfirmationDialog(const std::u16string& query,
                                RemovalConfirmationCallback callback);
  ~RemoveQueryConfirmationDialog() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  RemovalConfirmationCallback confirm_callback_;

  DISALLOW_COPY_AND_ASSIGN(RemoveQueryConfirmationDialog);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
