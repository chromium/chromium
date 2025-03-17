// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_DELEGATE_H_

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
    "OpenScreenListenerDelegate requires enable_service_discovery to be true.");

namespace media_router {

class OpenScreenListenerDelegate
    : public openscreen::osp::ServiceListener::Delegate,
      local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  explicit OpenScreenListenerDelegate(
      scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
          service_discovery_client);
  OpenScreenListenerDelegate(const OpenScreenListenerDelegate&) = delete;
  OpenScreenListenerDelegate& operator=(const OpenScreenListenerDelegate&) =
      delete;
  OpenScreenListenerDelegate(OpenScreenListenerDelegate&&) = delete;
  OpenScreenListenerDelegate& operator=(OpenScreenListenerDelegate&&) = delete;
  ~OpenScreenListenerDelegate() override;

  // ServiceListener::Delegate overrides.
  void StartListener(
      const openscreen::osp::ServiceListener::Config& config) override;
  void StartAndSuspendListener(
      const openscreen::osp::ServiceListener::Config& config) override;
  void StopListener() override;
  void SuspendListener() override;
  void ResumeListener() override;
  void SearchNow(openscreen::osp::ServiceListener::State from) override;

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

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_DISCOVERY_OPEN_SCREEN_LISTENER_DELEGATE_H_
