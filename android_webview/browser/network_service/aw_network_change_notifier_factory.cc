// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_network_change_notifier_factory.h"

#include "android_webview/browser/network_service/aw_network_change_notifier.h"
#include "base/memory/ptr_util.h"

namespace android_webview {

AwNetworkChangeNotifierFactory::AwNetworkChangeNotifierFactory() = default;

AwNetworkChangeNotifierFactory::~AwNetworkChangeNotifierFactory() = default;

std::unique_ptr<net::NetworkChangeNotifier>
AwNetworkChangeNotifierFactory::CreateInstanceWithInitialTypes(
    net::NetworkChangeNotifier::ConnectionType /*initial_type*/,
    net::NetworkChangeNotifier::ConnectionSubtype /*initial_subtype*/) {
  return base::WrapUnique(new AwNetworkChangeNotifier(&delegate_));
}

}  // namespace android_webview
