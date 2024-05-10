// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_
#define CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/device_api/device_attribute_api.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/device/device.mojom.h"

namespace content {
class RenderFrameHost;
}

// A browser-side mojo service, which corresponds to the navigator.managed Web
// API. Available only to trusted web applications.
class DeviceServiceImpl final
    : public content::DocumentService<blink::mojom::DeviceAPIService> {
 public:
  using DeviceAttributeCallback =
      base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)>;

  // Tries to attach this mojo service to |host| for trusted web applications.
  // Will dynamically disconnect if the trustness status is revoked.
  static void Create(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver);

  static void CreateForTest(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api);

  // Register the user prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  DeviceServiceImpl(const DeviceServiceImpl&) = delete;
  DeviceServiceImpl& operator=(const DeviceServiceImpl&) = delete;
  ~DeviceServiceImpl() override;

  // blink::mojom::DeviceAPIService:
  void GetDirectoryId(GetDirectoryIdCallback callback) override;
  void GetHostname(GetHostnameCallback callback) override;
  void GetSerialNumber(GetSerialNumberCallback callback) override;
  void GetAnnotatedAssetId(GetAnnotatedAssetIdCallback callback) override;
  void GetAnnotatedLocation(GetAnnotatedLocationCallback callback) override;

 private:
  static void Create(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api);

  DeviceServiceImpl(
      content::RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api);

  void GetDeviceAttribute(
      void (DeviceAttributeApi::*method)(DeviceAttributeCallback callback),
      DeviceAttributeCallback callback);

  void OnDisposingIfNeeded();

  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<DeviceAttributeApi> device_attribute_api_;
};

#endif  // CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_
