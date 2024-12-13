// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/common/buildflags.h"
#include "third_party/openscreen/src/osp/public/service_info.h"
#include "third_party/openscreen/src/osp/public/service_listener.h"

static_assert(
    BUILDFLAG(ENABLE_SERVICE_DISCOVERY),
    "OpenScreenListener requires enable_service_discovery to be true.");

namespace media_router {

// TODO(issuetracker.google.com/383267932): Don't use implementation
// inheritance!
class OpenScreenListener
    : public openscreen::osp::ServiceListener,
      local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  explicit OpenScreenListener(
      scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
          service_discovery_client);
  OpenScreenListener(const OpenScreenListener&) = delete;
  OpenScreenListener& operator=(const OpenScreenListener&) = delete;
  OpenScreenListener(OpenScreenListener&&) = delete;
  OpenScreenListener& operator=(OpenScreenListener&&) = delete;
  ~OpenScreenListener() override;

  // ServiceListener overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  bool SearchNow() override;
  void AddObserver(
      openscreen::osp::ServiceListener::Observer& observer) override;
  void RemoveObserver(
      openscreen::osp::ServiceListener::Observer& observer) override;
  const std::vector<openscreen::osp::ServiceInfo>& GetReceivers()
      const override;

  // ServiceDiscoveryDeviceLister::Delegate overrides.
  void OnDeviceChanged(
      const std::string& service_type,
      bool added,
      const local_discovery::ServiceDescription& service_description) override;
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;
  void OnPermissionRejected() override;

 protected:
  // Created when start discovering if service discovery is enabled.
  std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister> device_lister_;

 private:
  virtual void CreateDeviceLister();

  std::vector<openscreen::osp::ServiceInfo> receivers_;

  // The client and service type used to create `device_lister_`.
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_H_
