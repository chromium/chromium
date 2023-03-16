// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_VARIATIONS_AW_VARIATIONS_SERVICE_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_VARIATIONS_AW_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/variations/service/variations_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace android_webview {

// AwVariationsServiceClient provides an implementation of
// VariationsServiceClient, all members are currently stubs for WebView.
class AwVariationsServiceClient : public variations::VariationsServiceClient {
 public:
  AwVariationsServiceClient();

  AwVariationsServiceClient(const AwVariationsServiceClient&) = delete;
  AwVariationsServiceClient& operator=(const AwVariationsServiceClient&) =
      delete;

  ~AwVariationsServiceClient() override;

 private:
  base::Version GetVersionForSimulation() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  version_info::Channel GetChannel() override;
  bool OverridesRestrictParameter(std::string* parameter) override;
  bool IsEnterprise() override;
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_VARIATIONS_AW_VARIATIONS_SERVICE_CLIENT_H_
