// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_LEGACY_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
#define ASH_APP_LIST_VIEWS_LEGACY_REMOVE_QUERY_CONFIRMATION_DIALOG_H_

#include "base/callback.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// LegacyRemoveQueryConfirmationDialog displays the confirmation dialog for
// removing a recent query suggestion. Used for pre-productivity launcher UI.
// When kProductivityLauncher feature is enabled, the UI for confirming
// suggestion results uses `RemoveQueryConfirmationDialog`.
class LegacyRemoveQueryConfirmationDialog : public views::DialogDelegateView {
 public:
  // Callback to notify user's confirmation for removing the zero state
  // suggestion query. Invoked with true if user confirms removing query
  // suggestion; and false for declining the removal. The second parameter is
  // the event flags of user action for invoking the removal action on the
  // associated result.
  using RemovalConfirmationCallback = base::OnceCallback<void(bool)>;

  LegacyRemoveQueryConfirmationDialog(RemovalConfirmationCallback callback,
                                      const std::u16string& result_title);

  LegacyRemoveQueryConfirmationDialog(
      const LegacyRemoveQueryConfirmationDialog&) = delete;
  LegacyRemoveQueryConfirmationDialog& operator=(
      const LegacyRemoveQueryConfirmationDialog&) = delete;

  ~LegacyRemoveQueryConfirmationDialog() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  RemovalConfirmationCallback confirm_callback_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_LEGACY_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
