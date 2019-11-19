// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_network_change_notifier.h"

namespace android_webview {

AwNetworkChangeNotifier::~AwNetworkChangeNotifier() {
  delegate_->RemoveObserver(this);
}

net::NetworkChangeNotifier::ConnectionType
AwNetworkChangeNotifier::GetCurrentConnectionType() const {
  return delegate_->GetCurrentConnectionType();
}

void AwNetworkChangeNotifier::GetCurrentMaxBandwidthAndConnectionType(
    double* max_bandwidth_mbps,
    ConnectionType* connection_type) const {
  delegate_->GetCurrentMaxBandwidthAndConnectionType(max_bandwidth_mbps,
                                                     connection_type);
}

bool AwNetworkChangeNotifier::AreNetworkHandlesCurrentlySupported() const {
  return false;
}

void AwNetworkChangeNotifier::GetCurrentConnectedNetworks(
    NetworkChangeNotifier::NetworkList* networks) const {
  delegate_->GetCurrentlyConnectedNetworks(networks);
}

net::NetworkChangeNotifier::ConnectionType
AwNetworkChangeNotifier::GetCurrentNetworkConnectionType(
    NetworkHandle network) const {
  return delegate_->GetNetworkConnectionType(network);
}

net::NetworkChangeNotifier::NetworkHandle
AwNetworkChangeNotifier::GetCurrentDefaultNetwork() const {
  return delegate_->GetCurrentDefaultNetwork();
}

void AwNetworkChangeNotifier::OnConnectionTypeChanged() {}

void AwNetworkChangeNotifier::OnMaxBandwidthChanged(
    double max_bandwidth_mbps,
    ConnectionType type) {
  // Note that this callback is sufficient for Network Information API because
  // it also occurs on type changes (see network_change_notifier.h).
  NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChange(max_bandwidth_mbps,
                                                             type);
}

void AwNetworkChangeNotifier::OnNetworkConnected(NetworkHandle network) {}
void AwNetworkChangeNotifier::OnNetworkSoonToDisconnect(
    NetworkHandle network) {}
void AwNetworkChangeNotifier::OnNetworkDisconnected(
    NetworkHandle network) {}
void AwNetworkChangeNotifier::OnNetworkMadeDefault(NetworkHandle network) {}

AwNetworkChangeNotifier::AwNetworkChangeNotifier(
    net::NetworkChangeNotifierDelegateAndroid* delegate)
    : net::NetworkChangeNotifier(DefaultNetworkChangeCalculatorParams()),
      delegate_(delegate) {
  delegate_->AddObserver(this);
}

// static
net::NetworkChangeNotifier::NetworkChangeCalculatorParams
AwNetworkChangeNotifier::DefaultNetworkChangeCalculatorParams() {
  net::NetworkChangeNotifier::NetworkChangeCalculatorParams params;
  // Use defaults as in network_change_notifier_android.cc
  params.ip_address_offline_delay_ = base::TimeDelta::FromSeconds(1);
  params.ip_address_online_delay_ = base::TimeDelta::FromSeconds(1);
  params.connection_type_offline_delay_ = base::TimeDelta::FromSeconds(0);
  params.connection_type_online_delay_ = base::TimeDelta::FromSeconds(0);
  return params;
}

}  // namespace android_webview
