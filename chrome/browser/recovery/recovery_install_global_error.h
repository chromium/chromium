// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_H_
#define CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

// Shows elevation needed for recovery component install on the wrench menu
// using a bubble view and a menu item.
class RecoveryInstallGlobalError : public GlobalErrorWithStandardBubble,
                                   public KeyedService {
 public:
  explicit RecoveryInstallGlobalError(Profile* profile);
  ~RecoveryInstallGlobalError() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // GlobalErrorWithStandardBubble:
  Severity GetSeverity() override;
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  ui::ImageModel MenuItemIcon() override;
  void ExecuteMenuItem(Browser* browser) override;
  bool HasBubbleView() override;
  bool HasShownBubbleView() override;
  void ShowBubbleView(Browser* browser) override;
  bool ShouldCloseOnDeactivate() const override;
  base::string16 GetBubbleViewTitle() override;
  std::vector<base::string16> GetBubbleViewMessages() override;
  base::string16 GetBubbleViewAcceptButtonLabel() override;
  bool ShouldShowCloseButton() const override;
  bool ShouldAddElevationIconToAcceptButton() override;
  base::string16 GetBubbleViewCancelButtonLabel() override;
  void OnBubbleViewDidClose(Browser* browser) override;
  void BubbleViewAcceptButtonPressed(Browser* browser) override;
  void BubbleViewCancelButtonPressed(Browser* browser) override;

  bool HasElevationNotification() const;
  void OnElevationRequirementChanged();

  bool elevation_needed_;

  // The Profile this service belongs to.
  Profile* profile_;

  // Monitors registry change for recovery component install.
  PrefChangeRegistrar pref_registrar_;

  bool has_shown_bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(RecoveryInstallGlobalError);
};

#endif  // CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_H_
