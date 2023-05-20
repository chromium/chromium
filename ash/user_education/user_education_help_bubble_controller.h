// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {
class ElementContext;
class ElementIdentifier;
}  // namespace ui

namespace user_education {
class HelpBubble;
struct HelpBubbleParams;
}  // namespace user_education

namespace ash {

class UserEducationDelegate;
enum class HelpBubbleId;

// The singleton controller, owned by the `UserEducationController`, responsible
// for creation/management of help bubbles.
class ASH_EXPORT UserEducationHelpBubbleController {
 public:
  explicit UserEducationHelpBubbleController(UserEducationDelegate* delegate);
  UserEducationHelpBubbleController(const UserEducationHelpBubbleController&) =
      delete;
  UserEducationHelpBubbleController& operator=(
      const UserEducationHelpBubbleController&) = delete;
  ~UserEducationHelpBubbleController();

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if user education features are enabled.
  static UserEducationHelpBubbleController* Get();

  // Attempts to create a help bubble, identified by `help_bubble_id`, with the
  // specified `help_bubble_params` for the tracked element associated with the
  // specified `element_id` in the specified `element_context`. A help bubble
  // may not be created under certain circumstances, e.g. if there is already a
  // help bubble showing or if there is an ongoing tutorial running. Iff a help
  // bubble was created, `close_callback` is run when the help bubble is closed.
  // NOTE: Currently only the primary user profile is supported.
  bool CreateHelpBubble(HelpBubbleId help_bubble_id,
                        user_education::HelpBubbleParams help_bubble_params,
                        ui::ElementIdentifier element_id,
                        ui::ElementContext element_context,
                        base::OnceClosure close_callback = base::DoNothing());

  // Returns the unique identifier for the help bubble currently being shown for
  // the tracked element associated with the specified `element_id` in the
  // specified `element_context`. If no help bubble is currently being shown for
  // the tracked element or if the tracked element does not exist, an absent
  // value is returned.
  absl::optional<HelpBubbleId> GetHelpBubbleId(
      ui::ElementIdentifier element_id,
      ui::ElementContext element_context) const;

 private:
  // The delegate owned by the `UserEducationController` which facilitates
  // communication between Ash and user education services in the browser.
  const raw_ptr<UserEducationDelegate> delegate_;

  // The currently showing help bubble, if one exists, and a subscription to be
  // notified when it closes. Once closed, help bubble related memory is freed.
  std::unique_ptr<user_education::HelpBubble> help_bubble_;
  base::CallbackListSubscription help_bubble_close_subscription_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_
