// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_
#define CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_

#include "base/scoped_observer.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/frame_service_base.h"
#include "third_party/blink/public/mojom/device/device.mojom.h"

namespace content {
class RenderFrameHost;
}

// A browser-side mojo service, which corresponds to the navigator.device Web
// API. Available only to trusted web applications.
class DeviceServiceImpl final
    : public content::FrameServiceBase<blink::mojom::DeviceAPIService>,
      public ManagedConfigurationAPI::Observer {
 public:
  // Tries to attach this mojo service to |host| for trusted web applications.
  // Will dynamically disconnect if the trustness status is revoked.
  static void Create(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver);

  DeviceServiceImpl(const DeviceServiceImpl&) = delete;
  DeviceServiceImpl& operator=(const DeviceServiceImpl&) = delete;
  ~DeviceServiceImpl() override;

  // blink::mojom::DeviceAPIService:
  void GetManagedConfiguration(
      const std::vector<std::string>& keys,
      GetManagedConfigurationCallback callback) override;
  void SubscribeToManagedConfiguration(
      mojo::PendingRemote<blink::mojom::ManagedConfigurationObserver> observer)
      override;

 private:
  DeviceServiceImpl(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver);

  void OnForceInstallWebAppListChanged();

  ManagedConfigurationAPI* managed_configuration_api();
  // ManagedConfigurationAPI::Observer:
  void OnManagedConfigurationChanged() override;

  content::RenderFrameHost* const host_;
  PrefChangeRegistrar pref_change_registrar_;

  mojo::Remote<blink::mojom::ManagedConfigurationObserver>
      configuration_subscription_;
};

#endif  // CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_
