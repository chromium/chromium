// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
#define ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_

#include "ash/app_list/views/contents_view.h"
#include "base/callback.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// RemoveQueryConfirmationDialog displays the confirmation dialog for removing
// a recent query suggestion.
class RemoveQueryConfirmationDialog
    : public views::DialogDelegateView,
      public ContentsView::SearchBoxUpdateObserver {
 public:
  // Callback to notify user's confirmation for removing the zero state
  // suggestion query. Invoked with true if user confirms removing query
  // suggestion; and false for declining the removal. The second parameter is
  // the event flags of user action for invoking the removal action on the
  // associated result.
  using RemovalConfirmationCallback = base::OnceCallback<void(bool, int)>;

  RemoveQueryConfirmationDialog(const base::string16& query,
                                RemovalConfirmationCallback callback,
                                int event_flgas,
                                ContentsView* contents_view);
  ~RemoveQueryConfirmationDialog() override;

  // Shows the dialog with |parent|.
  void Show(gfx::NativeWindow parent);

  // views::View:
  const char* GetClassName() const override;

 private:
  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;

  // views::DialogDelegate:
  bool Accept() override;
  bool Cancel() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // ContentsView::SearchBoxUpdateObserver
  void OnSearchBoxBoundsUpdated() override;
  void OnSearchBoxClearAndDeactivated() override;

  void UpdateBounds();

  RemovalConfirmationCallback confirm_callback_;
  int event_flags_;
  ContentsView* const contents_view_;  // Owned by the views hierarchy

  DISALLOW_COPY_AND_ASSIGN(RemoveQueryConfirmationDialog);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
