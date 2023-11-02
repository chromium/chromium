// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_SERVICE_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/device/device.mojom.h"

class ManagedConfigurationServiceImpl
    : public content::DocumentService<
          blink::mojom::ManagedConfigurationService>,
      public ManagedConfigurationAPI::Observer {
 public:
  static void Create(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService>
          receiver);

  ManagedConfigurationServiceImpl(const ManagedConfigurationServiceImpl&) =
      delete;
  ManagedConfigurationServiceImpl& operator=(
      const ManagedConfigurationServiceImpl&) = delete;
  ~ManagedConfigurationServiceImpl() override;

 private:
  ManagedConfigurationServiceImpl(
      content::RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService>
          receiver);
  // blink::mojom::DeviceApiService:
  void GetManagedConfiguration(
      const std::vector<std::string>& keys,
      GetManagedConfigurationCallback callback) override;
  void SubscribeToManagedConfiguration(
      mojo::PendingRemote<blink::mojom::ManagedConfigurationObserver> observer)
      override;

  ManagedConfigurationAPI* managed_configuration_api();

  // ManagedConfigurationAPI::Observer:
  void OnManagedConfigurationChanged() override;
  const url::Origin& GetOrigin() override;

  mojo::Remote<blink::mojom::ManagedConfigurationObserver>
      configuration_subscription_;
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_SERVICE_H_
