// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_UI_USER_NOTICE_VIEW_H_
#define ASH_QUICK_ANSWERS_UI_USER_NOTICE_VIEW_H_

#include <memory>

#include "ash/quick_answers/ui/quick_answers_focus_search.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class LabelButton;
}  // namespace views

namespace ash {

class QuickAnswersUiController;
class QuickAnswersPreTargetHandler;

namespace quick_answers {

// TODO(siabhijeet): Investigate BubbleDialogDelegateView as a common view for
// UserNoticeView and QuickAnswersView.
// |intent_type| and |intent_text| are used to generate the notice title
// including predicted intent information. Fallback to title without intent
// information if any of these two strings are empty.
class UserNoticeView : public views::View {
 public:
  UserNoticeView(const gfx::Rect& anchor_view_bounds,
                 const base::string16& intent_type,
                 const base::string16& intent_text,
                 QuickAnswersUiController* ui_controller);

  // Disallow copy and assign.
  UserNoticeView(const UserNoticeView&) = delete;
  UserNoticeView& operator=(const UserNoticeView&) = delete;

  ~UserNoticeView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnFocus() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void InitContent();
  void InitButtonBar();
  void InitWidget();
  void AddDogfoodButton();
  void UpdateWidgetBounds();

  // QuickAnswersFocusSearch::GetFocusableViewsCallback to poll currently
  // focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Cached bounds of the anchor this view is tied to.
  gfx::Rect anchor_view_bounds_;
  // Cached title text.
  base::string16 title_;

  std::unique_ptr<QuickAnswersPreTargetHandler> event_handler_;
  QuickAnswersUiController* const ui_controller_;
  std::unique_ptr<QuickAnswersFocusSearch> focus_search_;

  // Owned by view hierarchy.
  views::View* main_view_ = nullptr;
  views::View* content_ = nullptr;
  views::ImageButton* dogfood_button_ = nullptr;
  views::LabelButton* settings_button_ = nullptr;
  views::LabelButton* accept_button_ = nullptr;
};

}  // namespace quick_answers
}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_UI_USER_NOTICE_VIEW_H_
