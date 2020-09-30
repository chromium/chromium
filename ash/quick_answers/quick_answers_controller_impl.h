// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_
#define ASH_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/quick_answers/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace quick_answers {
class QuickAnswersConsent;
}  // namespace quick_answers
}  // namespace chromeos

namespace ash {
class QuickAnswersUiController;

enum class QuickAnswersVisibility {
  // Quick Answers UI is hidden and the previous session has finished.
  kClosed = 0,
  // Quick Answers session is initializing and the UI will be shown when the
  // context is ready.
  kPending = 1,
  // Quick Answers UI is visible.
  kVisible = 2,
};

// Implementation of QuickAnswerController. It fetches quick answers
// result via QuickAnswersClient and manages quick answers UI.
class ASH_EXPORT QuickAnswersControllerImpl
    : public QuickAnswersController,
      public chromeos::quick_answers::QuickAnswersDelegate {
 public:
  explicit QuickAnswersControllerImpl();
  QuickAnswersControllerImpl(const QuickAnswersControllerImpl&) = delete;
  QuickAnswersControllerImpl& operator=(const QuickAnswersControllerImpl&) =
      delete;
  ~QuickAnswersControllerImpl() override;

  // QuickAnswersController:
  void SetClient(std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
                     client) override;

  // SetClient is required to be called before using these methods.
  // TODO(yanxiao): refactor to delegate to browser.
  void MaybeShowQuickAnswers(
      const gfx::Rect& anchor_bounds,
      const std::string& title,
      const chromeos::quick_answers::Context& context) override;

  void DismissQuickAnswers(bool is_active) override;

  // Update the bounds of the anchor view.
  void UpdateQuickAnswersAnchorBounds(const gfx::Rect& anchor_bounds) override;

  void SetPendingShowQuickAnswers() override;

  chromeos::quick_answers::QuickAnswersDelegate* GetQuickAnswersDelegate()
      override;

  // QuickAnswersDelegate:
  void OnQuickAnswerReceived(
      std::unique_ptr<chromeos::quick_answers::QuickAnswer> answer) override;
  void OnEligibilityChanged(bool eligible) override;
  void OnNetworkError() override;
  void OnRequestPreprocessFinished(
      const chromeos::quick_answers::QuickAnswersRequest& processed_request)
      override;

  // Retry sending quick answers request to backend.
  void OnRetryQuickAnswersRequest();

  // User clicks on the quick answer result.
  void OnQuickAnswerClick();

  // Called by the UI Controller when user grants consent for the Quick Answers
  // feature.
  void OnUserConsentGranted();

  // Called by the UI Controller when user requests detailed settings regarding
  // consent for the Quick Answers feature.
  void OnConsentSettingsRequestedByUser();

  // Open Quick-Answers dogfood URL.
  void OpenQuickAnswersDogfoodLink();

  QuickAnswersUiController* quick_answers_ui_controller() {
    return quick_answers_ui_controller_.get();
  }

  QuickAnswersVisibility visibility() const { return visibility_; }

  chromeos::quick_answers::QuickAnswersConsent*
  GetConsentControllerForTesting() {
    return consent_controller_.get();
  }

  void SetVisibilityForTesting(QuickAnswersVisibility visibility) {
    visibility_ = visibility;
  }

 private:
  void MaybeDismissQuickAnswersConsent();

  void HandleQuickAnswerRequest(
      const chromeos::quick_answers::QuickAnswersRequest& request);

  bool ShouldShowUserConsent() const;
  // Show the user consent view. Does nothing if the view is already visible.
  void ShowUserConsent(const base::string16& intent_type,
                       const base::string16& intent_text);

  chromeos::quick_answers::QuickAnswersRequest BuildRequest();

  // Bounds of the anchor view.
  gfx::Rect anchor_bounds_;

  // Query used to retrieve quick answer.
  std::string query_;

  // Title to be shown on the QuickAnswers view.
  std::string title_;

  // Context information, including surrounding text and device properties.
  chromeos::quick_answers::Context context_;

  std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
      quick_answers_client_;
  std::unique_ptr<chromeos::quick_answers::QuickAnswersConsent>
      consent_controller_;

  // Whether the feature is enabled and all eligibility criteria are met (
  // locale, consents, etc).
  bool is_eligible_ = false;

  std::unique_ptr<QuickAnswersUiController> quick_answers_ui_controller_;

  // The last received QuickAnswer from client.
  std::unique_ptr<chromeos::quick_answers::QuickAnswer> quick_answer_;

  QuickAnswersVisibility visibility_ = QuickAnswersVisibility::kClosed;
};

}  // namespace ash
#endif  // ASH_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_
