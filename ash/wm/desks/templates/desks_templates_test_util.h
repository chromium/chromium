// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_TEST_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_TEST_UTIL_H_

#include <vector>

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_icon_view.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "base/callback_helpers.h"
#include "base/guid.h"

namespace views {
class Button;
class Label;
}  // namespace views

namespace ash {

class DesksTemplatesPresenter;
class RoundedImageView;
class PillButton;
class CloseButton;

// Wrapper for `DesksTemplatesPresenter` that exposes internal state to test
// functions.
class DesksTemplatesPresenterTestApi {
 public:
  explicit DesksTemplatesPresenterTestApi(DesksTemplatesPresenter* presenter);
  DesksTemplatesPresenterTestApi(const DesksTemplatesPresenterTestApi&) =
      delete;
  DesksTemplatesPresenterTestApi& operator=(
      const DesksTemplatesPresenterTestApi&) = delete;
  ~DesksTemplatesPresenterTestApi();

  void SetOnUpdateUiClosure(base::OnceClosure closure);

 private:
  DesksTemplatesPresenter* const presenter_;
};

// Wrapper for `DesksTemplatesGridView` that exposes internal state to test
// functions.
class DesksTemplatesGridViewTestApi {
 public:
  explicit DesksTemplatesGridViewTestApi(DesksTemplatesGridView* grid_view);
  DesksTemplatesGridViewTestApi(DesksTemplatesGridViewTestApi&) = delete;
  DesksTemplatesGridViewTestApi& operator=(DesksTemplatesGridViewTestApi&) =
      delete;
  ~DesksTemplatesGridViewTestApi();

  void WaitForItemMoveAnimationDone();

 private:
  DesksTemplatesGridView* grid_view_;
};

// Wrapper for `DesksTemplatesItemView` that exposes internal state to test
// functions.
class DesksTemplatesItemViewTestApi {
 public:
  explicit DesksTemplatesItemViewTestApi(
      const DesksTemplatesItemView* item_view);
  DesksTemplatesItemViewTestApi(const DesksTemplatesItemViewTestApi&) = delete;
  DesksTemplatesItemViewTestApi& operator=(
      const DesksTemplatesItemViewTestApi&) = delete;
  ~DesksTemplatesItemViewTestApi();

  const views::Label* time_view() const { return item_view_->time_view_; }

  const CloseButton* delete_button() const {
    return item_view_->delete_button_;
  }

  const PillButton* launch_button() const { return item_view_->launch_button_; }

  const base::GUID uuid() const { return item_view_->desk_template_->uuid(); }

  const views::View* hover_container() const {
    return item_view_->hover_container_;
  }

  // Icons views are stored in the view hierarchy so this convenience function
  // returns them as a vector of DesksTemplatesIconView*.
  std::vector<DesksTemplatesIconView*> GetIconViews() const;

 private:
  const DesksTemplatesItemView* item_view_;
};

// Wrapper for `DesksTemplatesIconView` that exposes internal state to test
// functions.
class DesksTemplatesIconViewTestApi {
 public:
  explicit DesksTemplatesIconViewTestApi(
      const DesksTemplatesIconView* desks_templates_icon_view);
  DesksTemplatesIconViewTestApi(const DesksTemplatesIconViewTestApi&) = delete;
  DesksTemplatesIconViewTestApi& operator=(
      const DesksTemplatesIconViewTestApi&) = delete;
  ~DesksTemplatesIconViewTestApi();

  const views::Label* count_label() const {
    return desks_templates_icon_view_->count_label_;
  }

  const RoundedImageView* icon_view() const {
    return desks_templates_icon_view_->icon_view_;
  }

  const DesksTemplatesIconView* desks_templates_icon_view() const {
    return desks_templates_icon_view_;
  }

 private:
  const DesksTemplatesIconView* desks_templates_icon_view_;
};

// Return the `grid_item_index`th `DesksTemplatesItemView` from the first
// `OverviewGrid`'s `DesksTemplatesGridView` in `GetOverviewGridList()`.
DesksTemplatesItemView* GetItemViewFromTemplatesGrid(int grid_item_index);

// These buttons are the ones on the primary root window.
views::Button* GetZeroStateDesksTemplatesButton();
views::Button* GetExpandedStateDesksTemplatesButton();
views::Button* GetSaveDeskAsTemplateButton();
views::Button* GetTemplateItemButton(int index);
views::Button* GetTemplateItemDeleteButton(int index);
views::Button* GetDesksTemplatesDialogAcceptButton();

// A lot of the UI relies on calling into the local desk data manager to
// update, which sends callbacks via posting tasks. Call
// `WaitForDesksTemplatesUI()` if testing a piece of the UI which calls into the
// desk model.
void WaitForDesksTemplatesUI();

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_TEST_UTIL_H_
