// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "ash/ash_export.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_private_api_key.h"
#include "ash/user_education/user_education_tutorial_controller.h"

class PrefRegistrySimple;

namespace apps {
enum class LaunchSource;
}  // namespace apps

namespace ui {
class ElementIdentifier;
}  // namespace ui

namespace ash {

class UserEducationDelegate;
class UserEducationFeatureController;

enum class SystemWebAppType;

// The controller, owned by `Shell`, for user education features in Ash.
class ASH_EXPORT UserEducationController {
 public:
  explicit UserEducationController(std::unique_ptr<UserEducationDelegate>);
  UserEducationController(const UserEducationController&) = delete;
  UserEducationController& operator=(const UserEducationController&) = delete;
  ~UserEducationController();

  // Returns the singleton instance owned by `Shell`.
  // NOTE: Exists if and only if user education features are enabled.
  static UserEducationController* Get();

  // Registers user education prefs to the provided `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the identifier for an element associated with the specified
  // `app_id`, or an absent value if no such identifier exists. Note that
  // existence of an identifier does not imply the existence of an associated
  // element.
  std::optional<ui::ElementIdentifier> GetElementIdentifierForAppId(
      const std::string& app_id) const;

  // If present, indicates whether the currently active user is considered new.
  // A user is considered new if the first app list sync in the session was the
  // first sync ever across all ChromeOS devices and sessions for the given
  // user. As such, this value is absent until the first app list sync of the
  // session is completed.
  // NOTE: Currently only the primary user profile is supported.
  std::optional<bool> IsNewUser(UserEducationPrivateApiKey) const;

  // Attempts to launch the system web app associated with the given type on
  // the display associated with the given ID asynchronously.
  // NOTE: Currently only the primary user profile is supported.
  void LaunchSystemWebAppAsync(UserEducationPrivateApiKey,
                               SystemWebAppType system_web_app_type,
                               apps::LaunchSource launch_source,
                               int64_t display_id);

 private:
  // The delegate  which facilitates communication between Ash and user
  // education services in the browser.
  std::unique_ptr<UserEducationDelegate> delegate_;

  // The controller responsible for creation/management of help bubbles.
  UserEducationHelpBubbleController help_bubble_controller_{delegate_.get()};

  // The controller responsible for creation/management of tutorials.
  UserEducationTutorialController tutorial_controller_{delegate_.get()};

  // The set of controllers responsible for specific user education features.
  std::set<std::unique_ptr<UserEducationFeatureController>>
      feature_controllers_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_
