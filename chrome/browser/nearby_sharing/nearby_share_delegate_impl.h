// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/nearby_share_delegate.h"

class NearbySharingService;

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

// Delegate injected into |ash::Shell| to provide a communication channel
// between the system tray and the |NearbyShareService|. Singleton owned by the
// Shell, lives on the UI thread.
class NearbyShareDelegateImpl : public ash::NearbyShareDelegate {
 public:
  // For testing. Allows overriding |ShowSettingsPage|.
  class SettingsOpener {
   public:
    SettingsOpener() = default;
    SettingsOpener(SettingsOpener&) = delete;
    SettingsOpener& operator=(SettingsOpener&) = delete;
    virtual ~SettingsOpener() = default;

    // Open the chromeos settings page at the given uri, using
    // |chrome::SettingsWindowManager| by default.
    virtual void ShowSettingsPage(const std::string& sub_page);
  };

  NearbyShareDelegateImpl();
  NearbyShareDelegateImpl(NearbyShareDelegateImpl&) = delete;
  NearbyShareDelegateImpl& operator=(NearbyShareDelegateImpl&) = delete;
  ~NearbyShareDelegateImpl() override;

  // ash::NearbyShareDelegate
  bool IsPodButtonVisible() const override;
  bool IsHighVisibilityOn() const override;
  base::Optional<base::TimeDelta> RemainingHighVisibilityTime() const override;
  void EnableHighVisibility() override;
  void DisableHighVisibility() override;
  void ShowNearbyShareSettings() const override;

 private:
  NearbySharingService* GetService() const;

  std::unique_ptr<SettingsOpener> settings_opener_;

  // The time when high visibility is scheduled to be shut off.
  base::TimeTicks shutoff_time_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DELEGATE_IMPL_H_
