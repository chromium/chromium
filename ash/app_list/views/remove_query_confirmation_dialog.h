// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
#define ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_

#include <string>

#include "ash/style/system_dialog_delegate_view.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// RemoveQueryConfirmationDialog displays the confirmation dialog for removing
// a recent query suggestion.
class RemoveQueryConfirmationDialog : public ash::SystemDialogDelegateView {
  METADATA_HEADER(RemoveQueryConfirmationDialog, ash::SystemDialogDelegateView)

 public:
  // Callback to notify user's confirmation for removing the zero state
  // suggestion query. Invoked with true if user confirms removing query
  // suggestion; and false for declining the removal. The second parameter is
  // the event flags of user action for invoking the removal action on the
  // associated result.
  using RemovalConfirmationCallback = base::OnceCallback<void(bool)>;

  RemoveQueryConfirmationDialog(RemovalConfirmationCallback callback,
                                const std::u16string& result_title);

  RemoveQueryConfirmationDialog(const RemoveQueryConfirmationDialog&) = delete;
  RemoveQueryConfirmationDialog& operator=(
      const RemoveQueryConfirmationDialog&) = delete;

  ~RemoveQueryConfirmationDialog() override;

 private:
  RemovalConfirmationCallback confirm_callback_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
