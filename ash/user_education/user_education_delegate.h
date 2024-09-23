// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

class AccountId;

namespace apps {
enum class LaunchSource;
}  // namespace apps

namespace ui {
class ElementContext;
class ElementIdentifier;
}  // namespace ui

namespace user_education {
struct TutorialDescription;
}  // namespace user_education

namespace ash {

enum class SystemWebAppType;
enum class TutorialId;

// The delegate of the `UserEducationController` which facilitates communication
// between Ash and user education services in the browser.
class ASH_EXPORT UserEducationDelegate {
 public:
  virtual ~UserEducationDelegate() = default;

  // Returns the identifier for an element associated with the specified
  // `app_id`, or an absent value if no such identifier exists. Note that
  // existence of an identifier does not imply the existence of an associated
  // element.
  virtual std::optional<ui::ElementIdentifier> GetElementIdentifierForAppId(
      const std::string& app_id) const = 0;

  // If present, indicates whether the user associated with the given
  // `account_id` is considered new. A user is considered new if the first app
  // list sync in the session was the first sync ever across all ChromeOS
  // devices and sessions for the given user. As such, this value is absent
  // until the first app list sync of the session is completed.
  // NOTE: Currently only the primary user profile is supported.
  virtual const std::optional<bool>& IsNewUser(
      const AccountId& account_id) const = 0;

  // Returns whether the tutorial specified by `tutorial_id` is registered for
  // the user associated with the given `account_id`.
  // NOTE: Currently only the primary user profile is supported.
  virtual bool IsTutorialRegistered(const AccountId& account_id,
                                    TutorialId tutorial_id) const = 0;

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

  // Aborts the currently running tutorial. If `tutorial_id` is given, will only
  // abort the tutorial if it matches the id. If no `tutorial_id` is given, it
  // aborts any running tutorial whether it was started by this controller or
  // not. Any `aborted_callback` passed in at the time of start will be called.
  // NOTE: Currently only the primary user profile is supported.
  virtual void AbortTutorial(
      const AccountId& account_id,
      std::optional<TutorialId> tutorial_id = std::nullopt) = 0;

  // Attempts to launch the system web app associated with the given type on
  // the display associated with the given ID asynchronously.
  // NOTE: Currently only the primary user profile is supported.
  virtual void LaunchSystemWebAppAsync(const AccountId& account_id,
                                       SystemWebAppType system_web_app_type,
                                       apps::LaunchSource launch_source,
                                       int64_t display_id) = 0;

  // Returns true if there is a currently running tutorial for the user
  // associated with `account_id`. If `tutorial_id` is specified, specifically
  // returns whether *that* tutorial is running.
  virtual bool IsRunningTutorial(
      const AccountId& account_id,
      std::optional<TutorialId> tutorial_id = std::nullopt) const = 0;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
