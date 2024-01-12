// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_network_change_notifier.h"
#include "android_webview/common/aw_features.h"
#include "base/feature_list.h"

namespace android_webview {

AwNetworkChangeNotifier::~AwNetworkChangeNotifier() {
  delegate_->UnregisterObserver(this);
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
  return base::FeatureList::IsEnabled(
      features::kWebViewPropagateNetworkChangeSignals);
}

void AwNetworkChangeNotifier::GetCurrentConnectedNetworks(
    NetworkChangeNotifier::NetworkList* networks) const {
  delegate_->GetCurrentlyConnectedNetworks(networks);
}

net::NetworkChangeNotifier::ConnectionType
AwNetworkChangeNotifier::GetCurrentNetworkConnectionType(
    net::handles::NetworkHandle network) const {
  return delegate_->GetNetworkConnectionType(network);
}

net::handles::NetworkHandle AwNetworkChangeNotifier::GetCurrentDefaultNetwork()
    const {
  return delegate_->GetCurrentDefaultNetwork();
}

void AwNetworkChangeNotifier::OnConnectionTypeChanged() {}

void AwNetworkChangeNotifier::OnConnectionCostChanged() {}

void AwNetworkChangeNotifier::OnMaxBandwidthChanged(
    double max_bandwidth_mbps,
    ConnectionType type) {
  // Note that this callback is sufficient for Network Information API because
  // it also occurs on type changes (see network_change_notifier.h).
  NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChange(max_bandwidth_mbps,
                                                             type);
}

void AwNetworkChangeNotifier::OnNetworkConnected(
    net::handles::NetworkHandle network) {
  if (base::FeatureList::IsEnabled(
          features::kWebViewPropagateNetworkChangeSignals)) {
    NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
        NetworkChangeType::kConnected, network);
  }
}
void AwNetworkChangeNotifier::OnNetworkSoonToDisconnect(
    net::handles::NetworkHandle network) {
  if (base::FeatureList::IsEnabled(
          features::kWebViewPropagateNetworkChangeSignals)) {
    NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
        NetworkChangeType::kSoonToDisconnect, network);
  }
}
void AwNetworkChangeNotifier::OnNetworkDisconnected(
    net::handles::NetworkHandle network) {
  if (base::FeatureList::IsEnabled(
          features::kWebViewPropagateNetworkChangeSignals)) {
    NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
        NetworkChangeType::kDisconnected, network);
  }
}
void AwNetworkChangeNotifier::OnNetworkMadeDefault(
    net::handles::NetworkHandle network) {
  if (base::FeatureList::IsEnabled(
          features::kWebViewPropagateNetworkChangeSignals)) {
    NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
        NetworkChangeType::kMadeDefault, network);
  }
}

void AwNetworkChangeNotifier::OnDefaultNetworkActive() {}

AwNetworkChangeNotifier::AwNetworkChangeNotifier(
    net::NetworkChangeNotifierDelegateAndroid* delegate)
    : net::NetworkChangeNotifier(DefaultNetworkChangeCalculatorParams()),
      delegate_(delegate) {
  delegate_->RegisterObserver(this);
}

// static
net::NetworkChangeNotifier::NetworkChangeCalculatorParams
AwNetworkChangeNotifier::DefaultNetworkChangeCalculatorParams() {
  net::NetworkChangeNotifier::NetworkChangeCalculatorParams params;
  // Use defaults as in network_change_notifier_android.cc
  params.ip_address_offline_delay_ = base::Seconds(1);
  params.ip_address_online_delay_ = base::Seconds(1);
  params.connection_type_offline_delay_ = base::Seconds(0);
  params.connection_type_online_delay_ = base::Seconds(0);
  return params;
}

}  // namespace android_webview
