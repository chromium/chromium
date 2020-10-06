// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
#define ASH_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace quick_answers {
struct QuickAnswer;
}  // namespace quick_answers
}  // namespace chromeos

namespace ash {
class QuickAnswersView;
class QuickAnswersControllerImpl;

namespace quick_answers {
class UserNoticeView;
}  // namespace quick_answers

// A controller to show/hide and handle interactions for quick
// answers view.
class ASH_EXPORT QuickAnswersUiController {
 public:
  explicit QuickAnswersUiController(QuickAnswersControllerImpl* controller);
  ~QuickAnswersUiController();

  QuickAnswersUiController(const QuickAnswersUiController&) = delete;
  QuickAnswersUiController& operator=(const QuickAnswersUiController&) = delete;

  // Constructs/resets |quick_answers_view_|.
  void CreateQuickAnswersView(const gfx::Rect& anchor_bounds,
                              const std::string& title,
                              const std::string& query);

  // Returns true if there was a QuickAnswersView to close.
  bool CloseQuickAnswersView();

  void OnQuickAnswersViewPressed();

  void OnRetryLabelPressed();

  // |bounds| is the bound of context menu.
  void RenderQuickAnswersViewWithResult(
      const gfx::Rect& bounds,
      const chromeos::quick_answers::QuickAnswer& quick_answer);

  void SetActiveQuery(const std::string& query);

  // Show retry option in the quick answers view.
  void ShowRetry();

  void UpdateQuickAnswersBounds(const gfx::Rect& anchor_bounds);

  // Creates a view for notifying the user about the Quick Answers feature
  // vertically aligned to the anchor.
  void CreateUserNoticeView(const gfx::Rect& anchor_bounds,
                            const base::string16& intent_type,
                            const base::string16& intent_text);

  void CloseUserNoticeView();

  // Invoked when user clicks the 'got it' button to dismiss the notice.
  void OnAcceptButtonPressed();

  // Invoked when user clicks the settings button on the notice view.
  void OnManageSettingsButtonPressed();

  // Used by the controller to check if the user notice view is currently
  // showing instead of QuickAnswers.
  bool is_showing_user_notice_view() const {
    return user_notice_view_ != nullptr;
  }

  // Used by the controller to check if the QuickAnswers view is currently
  // showing.
  bool is_showing_quick_answers_view() const {
    return quick_answers_view_ != nullptr;
  }

  // Invoked when user clicks the Dogfood button on Quick-Answers related views.
  void OnDogfoodButtonPressed();

  const QuickAnswersView* quick_answers_view_for_testing() const {
    return quick_answers_view_;
  }
  const quick_answers::UserNoticeView* notice_view_for_testing() const {
    return user_notice_view_;
  }

 private:
  QuickAnswersControllerImpl* controller_ = nullptr;

  // Owned by view hierarchy.
  QuickAnswersView* quick_answers_view_ = nullptr;
  quick_answers::UserNoticeView* user_notice_view_ = nullptr;
  std::string query_;
};

}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
