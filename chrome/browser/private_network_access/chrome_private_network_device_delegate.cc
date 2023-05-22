// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/chrome_private_network_device_delegate.h"

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser_desktop.h"
#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"
#include "content/public/browser/render_frame_host.h"

ChromePrivateNetworkDeviceDelegate::ChromePrivateNetworkDeviceDelegate() =
    default;
ChromePrivateNetworkDeviceDelegate::~ChromePrivateNetworkDeviceDelegate() =
    default;

std::unique_ptr<ChromePrivateNetworkDeviceChooser>
ChromePrivateNetworkDeviceDelegate::RunChooser(
    content::RenderFrameHost& frame,
    std::unique_ptr<blink::mojom::PrivateNetworkDevice> device,
    const ChromePrivateNetworkDeviceChooser::EventHandler& event_handler) {
  auto controller = std::make_unique<PrivateNetworkDeviceChooserController>(
      &frame, std::move(device), std::move(event_handler));
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  return ChromePrivateNetworkDeviceChooserDesktop::Create(
      &frame, std::move(controller));
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePrivateNetworkDeviceDelegate::AddObserver(
    content::BrowserContext* browser_context,
    Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromePrivateNetworkDeviceDelegate::RemoveObserver(
    content::BrowserContext* browser_context,
    Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
