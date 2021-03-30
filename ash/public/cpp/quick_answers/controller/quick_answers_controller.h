// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_QUICK_ANSWERS_CONTROLLER_QUICK_ANSWERS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_QUICK_ANSWERS_CONTROLLER_QUICK_ANSWERS_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace quick_answers {
class QuickAnswersClient;
class QuickAnswersDelegate;
struct Context;
}  // namespace quick_answers
}  // namespace chromeos

namespace ash {

enum class QuickAnswersVisibility {
  // Quick Answers UI is hidden and the previous session has finished.
  kClosed = 0,
  // Quick Answers session is initializing and the UI will be shown when the
  // context is ready.
  kPending = 1,
  // Quick Answers UI is visible.
  kVisible = 2,
};

// A controller to manage quick answers UI.
class ASH_PUBLIC_EXPORT QuickAnswersController {
 public:
  QuickAnswersController();
  virtual ~QuickAnswersController();

  // Get the instance of |QuickAnswersController|. It is only available when
  // quick answers rich UI is enabled.
  static QuickAnswersController* Get();

  // Passes in a client instance for the controller to use.
  virtual void SetClient(
      std::unique_ptr<chromeos::quick_answers::QuickAnswersClient> client) = 0;

  // Show the quick-answers view (and/or any accompanying/associated views like
  // user-consent view instead, if consent is not yet granted). |anchor_bounds|
  // is the bounds of the anchor view (which is the context menu for browser).
  // |title| is the text selected by the user. |context| is the context
  // information which will be used as part of the request for getting more
  // relevant result.
  virtual void MaybeShowQuickAnswers(
      const gfx::Rect& anchor_bounds,
      const std::string& title,
      const chromeos::quick_answers::Context& context) = 0;

  // Dismiss the quick-answers view (and/or any associated views like
  // user-consent view) currently shown. |is_active| is true if the quick-answer
  // result considered fulfilling a user's intent. For quick-answer rendered
  // along with browser context menu, if user didn't click on other context menu
  // items, it is considered as active impression.
  virtual void DismissQuickAnswers(bool is_active) = 0;

  // Update the bounds of the anchor view.
  virtual void UpdateQuickAnswersAnchorBounds(
      const gfx::Rect& anchor_bounds) = 0;

  // Called when a quick-answers session has started but the detailed context is
  // still pending.
  virtual void SetPendingShowQuickAnswers() = 0;

  virtual chromeos::quick_answers::QuickAnswersDelegate*
  GetQuickAnswersDelegate() = 0;

  virtual QuickAnswersVisibility GetVisibilityForTesting() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_QUICK_ANSWERS_CONTROLLER_QUICK_ANSWERS_CONTROLLER_H_
