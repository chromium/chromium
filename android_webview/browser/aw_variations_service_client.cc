// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_variations_service_client.h"

#include "android_webview/common/aw_channel.h"
#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using version_info::Channel;

namespace android_webview {
namespace {

// Gets the version number to use for variations seed simulation. Must be called
// on a thread where IO is allowed.
base::Version GetVersionForSimulation() {
  base::AssertBlockingAllowedDeprecated();
  return version_info::GetVersion();
}

}  // namespace

AwVariationsServiceClient::AwVariationsServiceClient() {}

AwVariationsServiceClient::~AwVariationsServiceClient() {}

base::Callback<base::Version(void)>
AwVariationsServiceClient::GetVersionForSimulationCallback() {
  return base::BindRepeating(&GetVersionForSimulation);
}

scoped_refptr<network::SharedURLLoaderFactory>
AwVariationsServiceClient::GetURLLoaderFactory() {
  return nullptr;
}

network_time::NetworkTimeTracker*
AwVariationsServiceClient::GetNetworkTimeTracker() {
  return nullptr;
}

Channel AwVariationsServiceClient::GetChannel() {
  // Pretend stand-alone WebView is always "stable" for the purpose of
  // variations. This simplifies experiment design, since stand-alone WebView
  // need not be considered separately when choosing channels.
  return android_webview::GetChannelOrStable();
}

bool AwVariationsServiceClient::GetSupportsPermanentConsistency() {
  return false;
}

bool AwVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  return false;
}

}  // namespace android_webview
