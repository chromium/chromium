// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/user_education/user_education_types.h"
#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"

namespace ui {
class ElementContext;
class ElementIdentifier;
}  // namespace ui

namespace user_education {
class HelpBubble;
}  // namespace user_education

namespace ash {

class HelpBubbleViewAsh;
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

  // Returns the unique identifier for the help bubble currently being shown for
  // the tracked element associated with the specified `element_id` in the
  // specified `element_context`. If no help bubble is currently being shown for
  // the tracked element or if the tracked element does not exist, an absent
  // value is returned.
  std::optional<HelpBubbleId> GetHelpBubbleId(
      ui::ElementIdentifier element_id,
      ui::ElementContext element_context) const;

  // Adds a `callback` to be invoked whenever a help bubble's anchor bounds
  // change until the returned subscription is destroyed.
  [[nodiscard]] base::CallbackListSubscription
  AddHelpBubbleAnchorBoundsChangedCallback(base::RepeatingClosure callback);

  // Adds a `callback` to be invoked whenever a help bubble is closed until the
  // returned subscription is destroyed.
  [[nodiscard]] base::CallbackListSubscription AddHelpBubbleClosedCallback(
      base::RepeatingClosure callback);

  // Adds a `callback` to be invoked whenever a help bubble is shown until the
  // returned subscription is destroyed.
  [[nodiscard]] base::CallbackListSubscription AddHelpBubbleShownCallback(
      base::RepeatingClosure callback);

  // Invoked when the specified `help_bubble_view`'s anchor bounds change.
  void NotifyHelpBubbleAnchorBoundsChanged(
      base::PassKey<HelpBubbleViewAsh>,
      const HelpBubbleViewAsh* help_bubble_view);

  // Invoked when the specified `help_bubble_view` is closed.
  void NotifyHelpBubbleClosed(base::PassKey<HelpBubbleViewAsh>,
                              const HelpBubbleViewAsh* help_bubble_view);

  // Invoked when the specified `help_bubble_view`  is shown.
  void NotifyHelpBubbleShown(base::PassKey<HelpBubbleViewAsh>,
                             const HelpBubbleViewAsh* help_bubble_view);

  // Returns metadata for all currently showing help bubbles. Note that help
  // bubbles are closed asynchronously so it is possible for multiple help
  // bubbles to exist concurrently.
  const std::map<HelpBubbleKey, HelpBubbleMetadata>&
  help_bubble_metadata_by_key() const {
    return help_bubble_metadata_by_key_;
  }

 private:
  // The delegate owned by the `UserEducationController` which facilitates
  // communication between Ash and user education services in the browser.
  const raw_ptr<UserEducationDelegate> delegate_;

  // The currently showing help bubble, if one exists, and a subscription to be
  // notified when it closes. Once closed, help bubble related memory is freed.
  std::unique_ptr<user_education::HelpBubble> help_bubble_;
  base::CallbackListSubscription help_bubble_close_subscription_;

  // Metadata for all currently showing help bubbles. Note that help bubbles
  // are closed asynchronously so it is possible for multiple help bubbles to
  // exist concurrently.
  std::map<HelpBubbleKey, HelpBubbleMetadata> help_bubble_metadata_by_key_;

  // Lists of subscribers to notify for the following events:
  // (a) Help bubble anchor bounds changed
  // (b) Help bubble closed
  // (c) Help bubble shown
  base::RepeatingClosureList help_bubble_anchor_bounds_changed_subscribers_;
  base::RepeatingClosureList help_bubble_closed_subscribers_;
  base::RepeatingClosureList help_bubble_shown_subscribers_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_
