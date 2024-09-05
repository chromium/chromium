// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/nearby_share_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"

namespace ash {
class NearbyShareController;
}  // namespace ash

namespace base {
class TimeTicks;
}  // namespace base

// Delegate injected into |ash::Shell| to provide a communication channel
// between the system tray and the |NearbyShareService|. Singleton owned by the
// Shell, lives on the UI thread.
class NearbyShareDelegateImpl
    : public ash::NearbyShareDelegate,
      public ash::SessionObserver,
      public nearby_share::mojom::NearbyShareSettingsObserver,
      public ::NearbySharingService::Observer {
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

  explicit NearbyShareDelegateImpl(
      ash::NearbyShareController* nearby_share_controller);

  NearbyShareDelegateImpl(NearbyShareDelegateImpl&) = delete;
  NearbyShareDelegateImpl& operator=(NearbyShareDelegateImpl&) = delete;

  ~NearbyShareDelegateImpl() override;

  // ash::NearbyShareDelegate
  bool IsEnabled() override;
  bool IsPodButtonVisible() override;
  bool IsHighVisibilityOn() override;
  bool IsEnableHighVisibilityRequestActive() const override;
  base::TimeTicks HighVisibilityShutoffTime() const override;
  void EnableHighVisibility() override;
  void DisableHighVisibility() override;
  void ShowNearbyShareSettings() const override;
  const gfx::VectorIcon& GetIcon(bool on_icon) const override;
  std::u16string GetPlaceholderFeatureName() const override;
  ::nearby_share::mojom::Visibility GetVisibility() const override;
  void SetVisibility(::nearby_share::mojom::Visibility visibility) override;

  // ash::SessionObserver
  void OnLockStateChanged(bool locked) override;
  void OnFirstSessionStarted() override;

  // NearbyShareService::Observer
  void OnHighVisibilityChangeRequested() override;
  void OnHighVisibilityChanged(bool high_visibility_on) override;
  void OnShutdown() override;

  void SetNearbyShareServiceForTest(NearbySharingService* service);
  void SetNearbyShareSettingsForTest(NearbyShareSettings* settings);
  void set_settings_opener_for_test(
      std::unique_ptr<SettingsOpener> settings_opener) {
    settings_opener_ = std::move(settings_opener);
  }

  // nearby_share::mojom::NearbyShareSettingsObserver
  void OnEnabledChanged(bool enabled) override;
  void OnFastInitiationNotificationStateChanged(
      ::nearby_share::mojom::FastInitiationNotificationState state) override;
  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override;
  void OnDeviceNameChanged(const std::string& device_name) override;
  void OnDataUsageChanged(::nearby_share::mojom::DataUsage data_usage) override;
  void OnVisibilityChanged(
      ::nearby_share::mojom::Visibility visibility) override;
  void OnAllowedContactsChanged(
      const std::vector<std::string>& visible_contact_ids) override;
  void OnIsOnboardingCompleteChanged(bool is_complete) override;

 private:
  void AddNearbyShareServiceObservers();
  void RemoveNearbyShareServiceObservers();

  const raw_ptr<ash::NearbyShareController> nearby_share_controller_;
  raw_ptr<NearbySharingService> nearby_share_service_ = nullptr;
  std::unique_ptr<SettingsOpener> settings_opener_;

  // Track if there is an outstanding request to enable high visibility. Reset
  // to false once the request finishes (via OnHighVisibilityChanged());
  bool is_enable_high_visibility_request_active_ = false;

  // This timer is used to automatically turn off high visibility after a
  // timeout.
  base::RetainingOneShotTimer shutoff_timer_;

  // The time when high visibility is scheduled to be shut off.
  base::TimeTicks shutoff_time_;

  raw_ptr<NearbyShareSettings> nearby_share_settings_;
  mojo::Receiver<::nearby_share::mojom::NearbyShareSettingsObserver>
      nearby_share_settings_receiver_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DELEGATE_IMPL_H_
