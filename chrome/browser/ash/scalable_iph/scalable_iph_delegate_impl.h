// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class ScalableIphDelegateImpl
    : public scalable_iph::ScalableIphDelegate,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  explicit ScalableIphDelegateImpl(Profile* profile);
  ~ScalableIphDelegateImpl() override;

  // scalable_iph::ScalableIphDelegate:
  void ShowBubble(
      const BubbleParams& params,
      std::unique_ptr<scalable_iph::IphSession> iph_session) override;
  void ShowNotification(
      const NotificationParams& params,
      std::unique_ptr<scalable_iph::IphSession> iph_session) override;
  void AddObserver(
      scalable_iph::ScalableIphDelegate::Observer* observer) override;
  void RemoveObserver(
      scalable_iph::ScalableIphDelegate::Observer* observer) override;
  bool IsOnline() override;

  // chromeos::network_config::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;

 private:
  void SetHasOnlineNetwork(bool has_online_network);
  void QueryOnlineNetworkState();
  void OnNetworkStateList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  raw_ptr<Profile> profile_;
  bool has_online_network_ = false;

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      receiver_cros_network_config_observer_{this};

  base::ObserverList<scalable_iph::ScalableIphDelegate::Observer> observers_;

  base::WeakPtrFactory<ScalableIphDelegateImpl> weak_ptr_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_
