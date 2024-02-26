// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CHROME_ARC_INTENT_HELPER_DELEGATE_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CHROME_ARC_INTENT_HELPER_DELEGATE_H_

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

class Profile;

namespace arc {

// Implementation of ArcIntentHelperBridge::Delegate interface.
class ChromeArcIntentHelperDelegate : public ArcIntentHelperBridge::Delegate {
 public:
  explicit ChromeArcIntentHelperDelegate(Profile* profile);
  ChromeArcIntentHelperDelegate(const ChromeArcIntentHelperDelegate&) = delete;
  ChromeArcIntentHelperDelegate& operator=(
      const ChromeArcIntentHelperDelegate&) = delete;
  ~ChromeArcIntentHelperDelegate() override;

  // ArcIntentHelperBridge::Delegate:
  void ResetArc() override;
  void HandleUpdateAndroidSettings(mojom::AndroidSetting setting,
                                   bool is_enabled) override;

 private:
  void UpdateLocationSettings(bool is_enabled);
  void UpdateLocationAccuracySettings(bool is_enabled);
  bool IsInitialLocationSettingsSyncRequired();

  raw_ptr<Profile> profile_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CHROME_ARC_INTENT_HELPER_DELEGATE_H_
