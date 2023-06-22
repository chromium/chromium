// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"

#include <memory>

#include "ash/public/cpp/network_config_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace ash {

namespace {
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::FilterType;
using ::chromeos::network_config::mojom::kNoLimit;
using ::chromeos::network_config::mojom::NetworkFilter;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using Observer = ::scalable_iph::ScalableIphDelegate::Observer;

bool HasOnlineNetwork(const std::vector<NetworkStatePropertiesPtr>& networks) {
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network->connection_state == ConnectionStateType::kOnline) {
      return true;
    }
  }
  return false;
}

}  // namespace

ScalableIphDelegateImpl::ScalableIphDelegateImpl(Profile* profile)
    : profile_(profile) {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      receiver_cros_network_config_observer_.BindNewPipeAndPassRemote());

  QueryOnlineNetworkState();
}

// Remember NOT to interact with `iph_session` from the destructor. See the
// comment of `ScalableIphDelegate::ShowBubble` for details.
ScalableIphDelegateImpl::~ScalableIphDelegateImpl() = default;

void ScalableIphDelegateImpl::ShowBubble(
    const scalable_iph::ScalableIphDelegate::BubbleParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  // TODO(b/284158855): Add implementation.
}

void ScalableIphDelegateImpl::ShowNotification(
    const scalable_iph::ScalableIphDelegate::NotificationParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  // TODO(b/284158831): Add implementation.
}

void ScalableIphDelegateImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScalableIphDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ScalableIphDelegateImpl::IsOnline() {
  return has_online_network_;
}

void ScalableIphDelegateImpl::OnActiveNetworksChanged(
    std::vector<NetworkStatePropertiesPtr> networks) {
  SetHasOnlineNetwork(HasOnlineNetwork(networks));
}

void ScalableIphDelegateImpl::SetHasOnlineNetwork(bool has_online_network) {
  if (has_online_network_ == has_online_network) {
    return;
  }

  has_online_network_ = has_online_network;

  for (Observer& observer : observers_) {
    observer.OnConnectionChanged(has_online_network_);
  }
}

void ScalableIphDelegateImpl::QueryOnlineNetworkState() {
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll, kNoLimit),
      base::BindOnce(&ScalableIphDelegateImpl::OnNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIphDelegateImpl::OnNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  SetHasOnlineNetwork(HasOnlineNetwork(networks));
}

}  // namespace ash
