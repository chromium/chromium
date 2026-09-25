// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_FEATURES_GOOGLE_LENS_TIP_H_
#define CHROME_BROWSER_TIPS_FEATURES_GOOGLE_LENS_TIP_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/tips/core/tips_feature.h"
#include "chrome/browser/tips/core/tips_types.h"

class PrefService;

namespace tips {

// Concrete TipsFeature implementation for Google Lens notification.
class GoogleLensTip : public TipsFeature {
 public:
  static constexpr char kNewTabPageSearchBoxLensUserAction[] =
      "NewTabPage.SearchBox.Lens";
  static constexpr char kMobileOmniboxLensUserAction[] = "MobileOmniboxLens";
  static constexpr char kTasksSurfaceFakeBoxLensUserAction[] =
      "TasksSurface.FakeBox.Lens";
  static constexpr char kNotificationsTipsLensUserAction[] =
      "Notifications.Tips.Lens";
  static constexpr int kLookbackDays = 28;

  GoogleLensTip();
  ~GoogleLensTip() override;

  GoogleLensTip(const GoogleLensTip&) = delete;
  GoogleLensTip& operator=(const GoogleLensTip&) = delete;

  // TipsFeature:
  TipFeatureRank GetRank() const override;
  TipsNotificationsFeatureType GetFeatureType() const override;
  std::vector<SignalDefinition> GetRequiredSignals() const override;
  bool IsEligible(const std::map<std::string, float>& signal_values,
                  const PrefService& pref_service) const override;
  notifications::NotificationData GetNotificationData() const override;
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_FEATURES_GOOGLE_LENS_TIP_H_
