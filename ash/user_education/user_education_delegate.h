// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace ui {
class ElementContext;
class ElementIdentifier;
}  // namespace ui

namespace user_education {
class HelpBubble;
struct HelpBubbleParams;
struct TutorialDescription;
}  // namespace user_education

namespace ash {

enum class HelpBubbleId;
enum class TutorialId;

// The delegate of the `UserEducationController` which facilitates communication
// between Ash and user education services in the browser.
class ASH_EXPORT UserEducationDelegate {
 public:
  virtual ~UserEducationDelegate() = default;

  // Attempts to create a help bubble, identified by `help_bubble_id`, with the
  // specified `help_bubble_params` for the tracked element associated with the
  // specified `element_id` in the specified `element_context`. A help bubble
  // may not be created under certain circumstances, e.g. if there is an ongoing
  // tutorial running.
  // NOTE: Currently only the primary user profile is supported.
  virtual std::unique_ptr<user_education::HelpBubble> CreateHelpBubble(
      const AccountId& account_id,
      HelpBubbleId help_bubble_id,
      user_education::HelpBubbleParams help_bubble_params,
      ui::ElementIdentifier element_id,
      ui::ElementContext element_context) = 0;

  // Returns the identifier for an element associated with the specified
  // `app_id`, or an absent value if no such identifier exists. Note that
  // existence of an identifier does not imply the existence of an associated
  // element.
  virtual absl::optional<ui::ElementIdentifier> GetElementIdentifierForAppId(
      const std::string& app_id) const = 0;

  // Registers the tutorial defined by the specified `tutorial_id` and
  // `tutorial_description` for the user associated with the given `account_id`.
  // NOTE: Currently only the primary user profile is supported.
  virtual void RegisterTutorial(
      const AccountId& account_id,
      TutorialId tutorial_id,
      user_education::TutorialDescription tutorial_description) = 0;

  // Starts the tutorial previously registered with the specified `tutorial_id`
  // for the user associated with the given `account_id`. Any running tutorial
  // is cancelled. One of either `completed_callback` or `aborted_callback` will
  // be run on tutorial finish.
  // NOTE: Currently only the primary user profile is supported.
  virtual void StartTutorial(const AccountId& account_id,
                             TutorialId tutorial_id,
                             ui::ElementContext element_context,
                             base::OnceClosure completed_callback,
                             base::OnceClosure aborted_callback) = 0;

  // Aborts the currently running tutorial for the user associated with the
  // given `account_id`, whether it was started by this delegate or not. Any
  // `aborted_callback` passed in at the time of start will be called.
  // NOTE: Currently only the primary user profile is supported.
  virtual void AbortTutorial(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
