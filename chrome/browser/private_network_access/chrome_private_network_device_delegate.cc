// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/chrome_private_network_device_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser_desktop.h"
#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"
#include "chrome/browser/private_network_access/private_network_device_permission_context.h"
#include "chrome/browser/private_network_access/private_network_device_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/re2/src/re2/re2.h"

ChromePrivateNetworkDeviceDelegate::ChromePrivateNetworkDeviceDelegate() =
    default;
ChromePrivateNetworkDeviceDelegate::~ChromePrivateNetworkDeviceDelegate() =
    default;

void ChromePrivateNetworkDeviceDelegate::RequestPermission(
    content::RenderFrameHost& frame,
    blink::mojom::PrivateNetworkDevicePtr device,
    network::mojom::URLLoaderNetworkServiceObserver::
        OnPrivateNetworkAccessPermissionRequiredCallback callback) {
  bool is_device_valid = CheckDevice(*device, frame);
  if (HasDevicePermission(frame, *device, is_device_valid)) {
    std::move(callback).Run(true);
    return;
  }
  chooser_ = RunChooser(frame, std::move(device), std::move(callback),
                        is_device_valid);
}

bool ChromePrivateNetworkDeviceDelegate::CheckDevice(
    const blink::mojom::PrivateNetworkDevice& device,
    content::RenderFrameHost& frame) {
  // Check if `Private-Network-Access-ID` is missing.
  if (!device.id.has_value()) {
    base::UmaHistogramEnumeration(
        kPrivateNetworkDeviceValidityHistogramName,
        PrivateNetworkDeviceValidity::kDeviceIDMissing);
    frame.AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                              "`Private-Network-Access-ID` is missing.");
    return false;
  }

  // Check if `Private-Network-Access-Name` is valid.
  const re2::RE2 pattern("^[a-zA-Z0-9_\\-.]+$");
  if (!device.name.has_value() || device.name.value().length() > 248 ||
      !re2::RE2::FullMatch(device.name.value(), pattern)) {
    base::UmaHistogramEnumeration(
        kPrivateNetworkDeviceValidityHistogramName,
        device.name.has_value()
            ? PrivateNetworkDeviceValidity::kDeviceNameInvalid
            : PrivateNetworkDeviceValidity::kDeviceNameMissing);
    frame.AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                              "`Private-Network-Access-Name` is invalid. "
                              "A valid name is a string that matches the "
                              "[ECMAScript] regexp /^[a-z0-9_-.]+$/. "
                              "The maximum number of UTF-8 code units in a "
                              "Private Network Device name is 248.");
    return false;
  }

  return true;
}

bool ChromePrivateNetworkDeviceDelegate::HasDevicePermission(
    content::RenderFrameHost& frame,
    const blink::mojom::PrivateNetworkDevice& device,
    bool is_device_valid) {
  PrivateNetworkDevicePermissionContext* context =
      GetPermissionContext(frame.GetBrowserContext());
  return context && context->HasDevicePermission(frame.GetLastCommittedOrigin(),
                                                 device, is_device_valid);
}

std::unique_ptr<ChromePrivateNetworkDeviceChooser>
ChromePrivateNetworkDeviceDelegate::RunChooser(
    content::RenderFrameHost& frame,
    blink::mojom::PrivateNetworkDevicePtr device,
    network::mojom::URLLoaderNetworkServiceObserver::
        OnPrivateNetworkAccessPermissionRequiredCallback callback,
    bool is_device_valid) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(false);
  return nullptr;
#else
  auto controller = std::make_unique<PrivateNetworkDeviceChooserController>(
      &frame, std::move(device),
      base::BindOnce(&ChromePrivateNetworkDeviceDelegate::
                         HandlePrivateNetworkDeviceChooserResult,
                     base::Unretained(this), is_device_valid,
                     std::move(callback)));
  return ChromePrivateNetworkDeviceChooserDesktop::Create(
      &frame, std::move(controller));
#endif  // BUILDFLAG(IS_ANDROID)
}

PrivateNetworkDevicePermissionContext*
ChromePrivateNetworkDeviceDelegate::GetPermissionContext(
    content::BrowserContext* browser_context) {
  if (!browser_context) {
    return nullptr;
  }
  auto* profile = Profile::FromBrowserContext(browser_context);
  return profile ? PrivateNetworkDevicePermissionContextFactory::GetForProfile(
                       profile)
                 : nullptr;
}

void ChromePrivateNetworkDeviceDelegate::
    HandlePrivateNetworkDeviceChooserResult(
        bool is_device_valid,
        network::mojom::URLLoaderNetworkServiceObserver::
            OnPrivateNetworkAccessPermissionRequiredCallback callback,
        PrivateNetworkDevicePermissionContext* permission_context,
        const url::Origin& origin,
        const blink::mojom::PrivateNetworkDevice& device,
        bool permission_granted) {
  chooser_ = nullptr;
  if (permission_granted && permission_context) {
    permission_context->GrantDevicePermission(origin, device, is_device_valid);
  }
  std::move(callback).Run(permission_granted);
}
