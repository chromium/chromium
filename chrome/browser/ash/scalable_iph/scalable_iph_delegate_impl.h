// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class ScalableIphDelegateImpl
    : public scalable_iph::ScalableIphDelegate,
      public chromeos::network_config::CrosNetworkConfigObserver,
      public ShellObserver,
      public SessionObserver,
      public chromeos::PowerManagerClient::Observer,
      public AppListControllerObserver,
      public SyncedPrintersManager::Observer,
      public phonehub::FeatureStatusProvider::Observer {
 public:
  explicit ScalableIphDelegateImpl(Profile* profile,
                                   scalable_iph::Logger* logger);
  ~ScalableIphDelegateImpl() override;

  // scalable_iph::ScalableIphDelegate:
  bool ShowBubble(
      const BubbleParams& params,
      std::unique_ptr<scalable_iph::IphSession> iph_session) override;
  bool ShowNotification(
      const NotificationParams& params,
      std::unique_ptr<scalable_iph::IphSession> iph_session) override;
  void AddObserver(
      scalable_iph::ScalableIphDelegate::Observer* observer) override;
  void RemoveObserver(
      scalable_iph::ScalableIphDelegate::Observer* observer) override;
  bool IsOnline() override;
  int ClientAgeInDays() override;
  void PerformActionForScalableIph(
      scalable_iph::ActionType action_type) override;

  // chromeos::network_config::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;

  // ShellObserver:
  void OnShellDestroying() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // SyncedPrintersManager::Observer
  void OnSavedPrintersChanged() override;

  // phonehub::FeatureStatusProvider::Observer
  void OnFeatureStatusChanged() override;

  void SetFakeFeatureStatusProviderForTesting(
      phonehub::FeatureStatusProvider* feature_status_provider);

 private:
  bool IsEligibleAction(scalable_iph::ActionType action_type);
  void SetHasOnlineNetwork(bool has_online_network);
  void QueryOnlineNetworkState();
  void OnNetworkStateList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void NotifySessionStateChanged(
      ::scalable_iph::ScalableIphDelegate::SessionState session_state);
  void NotifySuspendDoneWithoutLockScreen();
  void MaybeNotifyHasSavedPrinters();
  void MaybeNotifyPhoneHubOnboardingEligibility();
  void OnNudgeButtonClicked(const std::string& bubble_id,
                            scalable_iph::ScalableIphDelegate::Action action);
  void OnNudgeDismissed(const std::string& bubble_id);

  scalable_iph::Logger* GetLogger() { return logger_; }

  raw_ptr<Profile> profile_;

  // Owned by `ScalableIph`
  raw_ptr<scalable_iph::Logger> logger_;

  raw_ptr<SyncedPrintersManager> synced_printers_manager_;
  raw_ptr<phonehub::FeatureStatusProvider> feature_status_provider_;
  bool has_online_network_ = false;
  bool has_saved_printers_ = false;
  bool phonehub_onboarding_eligible_ = false;

  std::unique_ptr<scalable_iph::IphSession> bubble_iph_session_;
  std::string bubble_id_;

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      receiver_cros_network_config_observer_{this};

  base::ObserverList<scalable_iph::ScalableIphDelegate::Observer> observers_;
  base::ScopedObservation<Shell, ShellObserver> shell_observer_{this};
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      session_observer_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_{this};
  base::ScopedObservation<AppListController, AppListControllerObserver>
      app_list_controller_observer_{this};
  base::ScopedObservation<SyncedPrintersManager,
                          SyncedPrintersManager::Observer>
      synced_printers_manager_observer_{this};
  base::ScopedObservation<phonehub::FeatureStatusProvider,
                          phonehub::FeatureStatusProvider::Observer>
      feature_status_provider_observer_{this};

  base::WeakPtrFactory<ScalableIphDelegateImpl> weak_ptr_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_
