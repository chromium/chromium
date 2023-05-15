// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_help_bubble_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_delegate.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "base/check_op.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
UserEducationHelpBubbleController* g_instance = nullptr;

}  // namespace

UserEducationHelpBubbleController::UserEducationHelpBubbleController(
    UserEducationDelegate* delegate)
    : delegate_(delegate) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

UserEducationHelpBubbleController::~UserEducationHelpBubbleController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserEducationHelpBubbleController* UserEducationHelpBubbleController::Get() {
  return g_instance;
}

bool UserEducationHelpBubbleController::CreateHelpBubble(
    HelpBubbleId help_bubble_id,
    user_education::HelpBubbleParams help_bubble_params,
    ui::ElementIdentifier element_id,
    ui::ElementContext element_context,
    base::OnceClosure close_callback) {
  // Prohibit showing multiple help bubbles concurrently.
  if (help_bubble_) {
    return false;
  }

  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  CHECK(user_education_util::IsPrimaryAccountId(account_id));

  // Attempt to create a `help_bubble_`.
  help_bubble_ = delegate_->CreateHelpBubble(account_id, help_bubble_id,
                                             std::move(help_bubble_params),
                                             element_id, element_context);

  // The `delegate_` may opt *not* to create a `help_bubble_` in certain
  // circumstances, e.g. when there is an ongoing tutorial.
  if (!help_bubble_) {
    return false;
  }

  // Subscribe to be notified when the `help_bubble_` closes. Once closed, free
  // `help_bubble_` related memory and run the provided `close_callback`.
  help_bubble_close_subscription_ =
      help_bubble_->AddOnCloseCallback(base::BindOnce(
          [](UserEducationHelpBubbleController* self,
             base::OnceClosure close_callback,
             user_education::HelpBubble* help_bubble) {
            CHECK_EQ(self->help_bubble_.get(), help_bubble);
            self->help_bubble_.reset();
            self->help_bubble_close_subscription_ =
                base::CallbackListSubscription();
            std::move(close_callback).Run();
          },
          base::Unretained(this), std::move(close_callback)));

  // Indicate success.
  return true;
}

}  // namespace ash
