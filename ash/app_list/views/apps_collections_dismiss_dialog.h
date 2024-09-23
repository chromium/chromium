// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_COLLECTIONS_DISMISS_DIALOG_H_
#define ASH_APP_LIST_VIEWS_APPS_COLLECTIONS_DISMISS_DIALOG_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Button;
class Label;
class ViewShadow;
}  // namespace views

namespace ash {

// AppsCollectionsDismissDialog displays a confirmation dialog for dismissing
// the apps collections view.
class AppsCollectionsDismissDialog : public views::WidgetDelegateView {
  METADATA_HEADER(AppsCollectionsDismissDialog, views::WidgetDelegateView)

 public:
  explicit AppsCollectionsDismissDialog(base::OnceClosure confirm_callback);

  AppsCollectionsDismissDialog(const AppsCollectionsDismissDialog&) = delete;
  AppsCollectionsDismissDialog& operator=(const AppsCollectionsDismissDialog&) =
      delete;

  ~AppsCollectionsDismissDialog() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  views::Button* cancel_button_for_test() { return cancel_button_; }
  views::Button* accept_button_for_test() { return accept_button_; }

 private:
  base::OnceClosure confirm_callback_;
  std::unique_ptr<views::ViewShadow> view_shadow_;

  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Button> cancel_button_ = nullptr;
  raw_ptr<views::Button> accept_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_COLLECTIONS_DISMISS_DIALOG_H_
