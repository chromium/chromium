// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace ash {

// A delegate which exposes browser functionality from //chrome to the help app
// ui page handler.
class HelpAppUIDelegate {
 public:
  virtual ~HelpAppUIDelegate() = default;

  // Opens the native chrome feedback dialog scoped to chrome://help-app.
  // Returns an optional error message if unable to open the dialog or nothing
  // if the dialog was determined to have opened successfully.
  virtual absl::optional<std::string> OpenFeedbackDialog() = 0;

  // Opens OS Settings at the parental controls section.
  virtual void ShowParentalControls() = 0;

  // Gets locally stored users preferences and state.
  virtual PrefService* GetLocalState() = 0;

  // Asks the help app notification controller to show the discover notification
  // if the required heuristics are present and if a notification for the help
  // app has not yet been shown in the current milestone.
  virtual void MaybeShowDiscoverNotification() = 0;

  // Asks the help app notification controller to show the release notes
  // notification if a notification for the help app has not yet been shown in
  // the current milestone.
  virtual void MaybeShowReleaseNotesNotification() = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_
