// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CHROME_ARC_SETTINGS_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CHROME_ARC_SETTINGS_APP_DELEGATE_H_

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "base/memory/raw_ptr.h"
#include "components/arc/intent_helper/arc_settings_app_delegate.h"

class Profile;

namespace arc {

// Ash-side ArcSettingsAppDelegate handling.
class ChromeArcSettingsAppDelegate : public ArcSettingsAppDelegate {
 public:
  explicit ChromeArcSettingsAppDelegate(Profile* profile);
  ChromeArcSettingsAppDelegate(const ChromeArcSettingsAppDelegate&) = delete;
  ChromeArcSettingsAppDelegate& operator=(const ChromeArcSettingsAppDelegate&) =
      delete;

  ~ChromeArcSettingsAppDelegate() override;

  // ArcSettingsAppDelegate overrides:
  void HandleUpdateAndroidSettings(mojom::AndroidSetting setting,
                                   bool is_enabled) override;

 private:
  void UpdateLocationSettings(bool is_enabled);

  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CHROME_ARC_SETTINGS_APP_DELEGATE_H_
