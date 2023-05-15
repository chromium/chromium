// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
#define ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Button;
class Label;
}  // namespace views

namespace ash {

class ViewShadow;

// RemoveQueryConfirmationDialog displays the confirmation dialog for removing
// a recent query suggestion.
class RemoveQueryConfirmationDialog : public views::WidgetDelegateView {
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

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  views::Button* cancel_button_for_test() { return cancel_button_; }
  views::Button* accept_button_for_test() { return accept_button_; }

 private:
  RemovalConfirmationCallback confirm_callback_;
  std::unique_ptr<ViewShadow> view_shadow_;

  // Whether Jelly style feature is enabled.
  bool is_jellyroll_enabled_ = false;

  raw_ptr<views::Label, ExperimentalAsh> title_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> body_ = nullptr;
  raw_ptr<views::Button, ExperimentalAsh> cancel_button_ = nullptr;
  raw_ptr<views::Button, ExperimentalAsh> accept_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
