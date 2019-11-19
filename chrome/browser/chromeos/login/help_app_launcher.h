// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HELP_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HELP_APP_LAUNCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;

namespace chromeos {

// Provides help content during OOBE / login.
// Based on connectivity state (offline/online) shows help topic dialog
// or launches HelpApp in BWSI mode.
class HelpAppLauncher : public base::RefCountedThreadSafe<HelpAppLauncher> {
 public:
  // IDs of help topics available from HelpApp.
  enum HelpTopic {
    // Showed at EULA screen as "Learn more" about stats/crash reports.
    HELP_STATS_USAGE = 183078,
    // Showed whenever there're troubles signing in.
    HELP_CANT_ACCESS_ACCOUNT = 188036,
    // Showed as "Learn more" about enterprise enrolled devices.
    HELP_ENTERPRISE = 2535613,
    // Shown at reset screen as "Learn more" about powerwash/rollback options.
    HELP_POWERWASH = 183084,
    // Shown as "Learn more" about the device attributes.
    HELP_DEVICE_ATTRIBUTES = 6218780,
    // Shown as "Learn more" for TPM firmware udpate option in reset screen.
    HELP_TPM_FIRMWARE_UPDATE = 7507584,
    // Shown as "Learn more" for Wilco notifications related to battery and
    // charger.
    HELP_WILCO_BATTERY_CHARGER = 9383188,
    // Shown as "Learn more" for Wilco notifications related to dock station.
    HELP_WILCO_DOCK = 9385025,
    // Shown under "Need help?" button on parent access dialog.
    HELP_PARENT_ACCESS_CODE = 7307262,
  };

  // The dialog is shown as a child of |parent_window|. If |parent_window| is
  // null then the dialog is placed in the modal dialog container on the primary
  // display.
  explicit HelpAppLauncher(gfx::NativeWindow parent_window);

  // Shows specified help topic.
  void ShowHelpTopic(HelpTopic help_topic_id);

  // Allows tests to specify a different extension id to connect to.
  static void SetExtensionIdForTest(const char* extension_id);

 protected:
  virtual ~HelpAppLauncher();

 private:
  friend class base::RefCountedThreadSafe<HelpAppLauncher>;

  // Shows help topic dialog for specified GURL.
  void ShowHelpTopicDialog(Profile* profile, const GURL& topic_url);

  // Parent window which is passed to help dialog.
  gfx::NativeWindow parent_window_;

  DISALLOW_COPY_AND_ASSIGN(HelpAppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HELP_APP_LAUNCHER_H_
