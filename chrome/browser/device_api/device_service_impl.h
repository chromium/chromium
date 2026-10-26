// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_
#define CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/self_deleting.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/device_api/device_attribute_api.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/device/device.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/content_settings/core/browser/host_content_settings_map.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {
class RenderFrameHost;
}

// A browser-side mojo service, which corresponds to the navigator.managed Web
// API. Available only to trusted web applications.
class DeviceServiceImpl final
    : public content::DocumentService<blink::mojom::DeviceAPIService>,
      public web_app::WebAppInstallManagerObserver
#if BUILDFLAG(IS_CHROMEOS)
    ,
      public content_settings::Observer
#endif  // BUILDFLAG(IS_CHROMEOS)
{
 public:
  using DeviceAttributeCallback = base::OnceCallback<void(
      base::expected<blink::mojom::DeviceAttributeValuePtr, std::string>)>;

  // Tries to attach this mojo service to |host| for trusted web applications.
  // Will dynamically disconnect if the trustness status is revoked.
  static void Create(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver);

  static void CreateForTest(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api);

  DeviceServiceImpl(
      content::RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api,
      base::SelfDeletingPassKey key);

  DeviceServiceImpl(const DeviceServiceImpl&) = delete;
  DeviceServiceImpl& operator=(const DeviceServiceImpl&) = delete;

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

  ~DeviceServiceImpl() override;

  void GetDeviceAttribute(
      void (DeviceAttributeApi::*method)(DeviceAttributeCallback callback),
      DeviceAttributeCallback callback);

  // WebAppInstallManagerObserver:
  void OnWebAppSourceRemoved(const webapps::AppId& app_id) override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

#if BUILDFLAG(IS_CHROMEOS)
  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  void OnDisposingIfNeeded();

  PrefChangeRegistrar pref_change_registrar_;
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};
#if BUILDFLAG(IS_CHROMEOS)
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};
#endif  // BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<DeviceAttributeApi> device_attribute_api_;
};

#endif  // CHROME_BROWSER_DEVICE_API_DEVICE_SERVICE_IMPL_H_
