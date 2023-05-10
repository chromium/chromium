// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ui {
class ElementContext;
class ElementIdentifier;
}  // namespace ui

namespace user_education {
struct HelpBubbleParams;
}  // namespace user_education

namespace ash {

enum class HelpBubbleId;

// The singleton controller, owned by the `UserEducationController`, responsible
// for creation/management of help bubbles.
class ASH_EXPORT UserEducationHelpBubbleController {
 public:
  UserEducationHelpBubbleController();
  UserEducationHelpBubbleController(const UserEducationHelpBubbleController&) =
      delete;
  UserEducationHelpBubbleController& operator=(
      const UserEducationHelpBubbleController&) = delete;
  ~UserEducationHelpBubbleController();

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if user education features are enabled.
  static UserEducationHelpBubbleController* Get();

  // TODO(http://b/279040829): Implement.
  // Attempts to create a help bubble, identified by `help_bubble_id`, with the
  // specified `help_bubble_params` for the tracked element associated with the
  // specified `element_id` in the specified `element_context`.
  // NOTE: Currently hardcoded to no-op and return `false`.
  bool CreateHelpBubble(HelpBubbleId help_bubble_id,
                        user_education::HelpBubbleParams help_bubble_params,
                        ui::ElementIdentifier element_id,
                        ui::ElementContext element_context);
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_HELP_BUBBLE_CONTROLLER_H_
