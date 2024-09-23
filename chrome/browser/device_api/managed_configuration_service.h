// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_SERVICE_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/device_api/managed_configuration_api.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/device/device.mojom.h"

class ManagedConfigurationServiceImpl
    : public content::DocumentService<
          blink::mojom::ManagedConfigurationService>,
      public ManagedConfigurationAPI::Observer {
 public:
  // Creates a `ManagedConfigurationServiceImpl` owned by the given `host`, and
  // returns a reference to it. May return `nullptr` when `host` is not allowed
  // to use managed configurations, for example in incognito.
  static ManagedConfigurationServiceImpl* Create(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService>
          receiver);

  ManagedConfigurationServiceImpl(const ManagedConfigurationServiceImpl&) =
      delete;
  ManagedConfigurationServiceImpl& operator=(
      const ManagedConfigurationServiceImpl&) = delete;
  ~ManagedConfigurationServiceImpl() override;

  // blink::mojom::ManagedConfigurationService:
  void GetManagedConfiguration(
      const std::vector<std::string>& keys,
      GetManagedConfigurationCallback callback) override;
  void SubscribeToManagedConfiguration(
      mojo::PendingRemote<blink::mojom::ManagedConfigurationObserver> observer)
      override;

  // ManagedConfigurationAPI::Observer:
  void OnManagedConfigurationChanged() override;

 private:
  ManagedConfigurationServiceImpl(
      content::RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService>
          receiver);

  ManagedConfigurationAPI* managed_configuration_api();

  const url::Origin& GetOrigin() const override;

  mojo::Remote<blink::mojom::ManagedConfigurationObserver>
      configuration_subscription_;
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_SERVICE_H_
