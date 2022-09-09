// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_H_

#include <string>
#include <vector>

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

#include "third_party/openscreen/src/osp/public/service_info.h"
#include "third_party/openscreen/src/osp/public/service_listener.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace media_router {
class OpenScreenListener
    : public openscreen::osp::ServiceListener,
      local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  explicit OpenScreenListener(std::string service_type);

  OpenScreenListener(const OpenScreenListener&) = delete;
  OpenScreenListener& operator=(const OpenScreenListener&) = delete;

  // ServiceListener overrides
  ~OpenScreenListener() override;

  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  bool SearchNow() override;

  const std::vector<openscreen::osp::ServiceInfo>& GetReceivers()
      const override;
  void AddObserver(ServiceListener::Observer* observer) override;
  void RemoveObserver(ServiceListener::Observer* observer) override;

  // ServiceDiscoveryDeviceLister::Delegate
  void OnDeviceChanged(
      const std::string& service_type,
      bool added,
      const local_discovery::ServiceDescription& service_description) override;

  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;

 private:
  bool is_running_ = false;
  const std::string service_type_;
  std::vector<openscreen::osp::ServiceInfo> receivers_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_H_
