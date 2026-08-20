// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_BACKGROUND_MODE_MANAGER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_member.h"

class Profile;
class ScopedProfileKeepAlive;

namespace omnibox_everywhere {

// Manages OmniboxEverywhere's background mode and status icon based on pref
// state.
class OmniboxEverywhereBackgroundModeManager
    : public StatusIconObserver,
      public StatusIconMenuModel::Delegate {
 public:
  using ShowUICallback = base::RepeatingClosure;

  explicit OmniboxEverywhereBackgroundModeManager(
      ShowUICallback show_ui_callback);
  OmniboxEverywhereBackgroundModeManager(
      const OmniboxEverywhereBackgroundModeManager&) = delete;
  OmniboxEverywhereBackgroundModeManager& operator=(
      const OmniboxEverywhereBackgroundModeManager&) = delete;
  ~OmniboxEverywhereBackgroundModeManager() override;

  // Sets the profile for which to hold a profile keep alive.
  void SetProfile(Profile* profile);

  // Stops the background mode by releasing keep-alives and status icon.
  void ExitBackgroundMode();

  StatusIcon* status_icon_for_testing() { return status_icon_; }

 private:
  void OnPrefChanged();

  void ShowStatusIcon();
  void HideStatusIcon();
  void UpdateStatusIconContextMenu();
  void UpdateProfileKeepAlive();

  // StatusIconObserver:
  void OnStatusIconClicked() override;

  // StatusIconMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  void Reset();

  BooleanPrefMember background_mode_pref_member_;
  StringPrefMember hotkey_string_pref_member_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  raw_ptr<StatusIcon> status_icon_;
  ShowUICallback show_ui_callback_;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_BACKGROUND_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_BACKGROUND_MODE_MANAGER_H_
