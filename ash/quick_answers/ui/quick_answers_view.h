// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
#define ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_answers/ui/quick_answers_focus_search.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"

namespace chromeos {
namespace quick_answers {
struct QuickAnswer;
}  // namespace quick_answers
}  // namespace chromeos

namespace views {
class ImageButton;
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class QuickAnswersUiController;
class QuickAnswersPreTargetHandler;

// A bubble style view to show QuickAnswer.
class ASH_EXPORT QuickAnswersView : public views::Button {
 public:
  QuickAnswersView(const gfx::Rect& anchor_view_bounds,
                   const std::string& title,
                   QuickAnswersUiController* controller);
  ~QuickAnswersView() override;

  QuickAnswersView(const QuickAnswersView&) = delete;
  QuickAnswersView& operator=(const QuickAnswersView&) = delete;

  // views::View:
  const char* GetClassName() const override;
  void OnFocus() override;
  void OnBlur() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::Button:
  void StateChanged(views::Button::ButtonState old_state) override;

  // Called when a click happens to trigger Assistant Query.
  void SendQuickAnswersQuery();

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

  // Update the quick answers view with quick answers result.
  void UpdateView(const gfx::Rect& anchor_view_bounds,
                  const chromeos::quick_answers::QuickAnswer& quick_answer);

  void ShowRetryView();

 private:
  void InitLayout();
  void InitWidget();
  void AddDogfoodButton();
  void AddAssistantIcon();
  void ResetContentView();
  void SetBackgroundState(bool highlight);
  void UpdateBounds();
  void UpdateQuickAnswerResult(
      const chromeos::quick_answers::QuickAnswer& quick_answer);

  // Buttons should fire on mouse-press instead of default behavior (waiting for
  // mouse-release), since events of former type dismiss the accompanying menu.
  void SetButtonNotifyActionToOnPress(views::Button* button);

  // QuickAnswersFocusSearch::GetFocusableViewsCallback to poll currently
  // focusable views.
  std::vector<views::View*> GetFocusableViews();

  gfx::Rect anchor_view_bounds_;
  QuickAnswersUiController* const controller_;
  bool has_second_row_answer_ = false;
  std::string title_;

  views::View* main_view_ = nullptr;
  views::View* content_view_ = nullptr;
  views::Label* first_answer_label_ = nullptr;
  views::LabelButton* retry_label_ = nullptr;
  views::ImageButton* dogfood_button_ = nullptr;

  std::unique_ptr<QuickAnswersPreTargetHandler> quick_answers_view_handler_;
  std::unique_ptr<QuickAnswersFocusSearch> focus_search_;
  base::WeakPtrFactory<QuickAnswersView> weak_factory_{this};
};
}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
