// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/chrome_private_network_device_chooser_desktop.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/render_frame_host.h"

ChromePrivateNetworkDeviceChooserDesktop::
    ChromePrivateNetworkDeviceChooserDesktop() = default;

ChromePrivateNetworkDeviceChooserDesktop::
    ~ChromePrivateNetworkDeviceChooserDesktop() = default;

std::unique_ptr<ChromePrivateNetworkDeviceChooserDesktop>
ChromePrivateNetworkDeviceChooserDesktop::Create(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<PrivateNetworkDeviceChooserController> controller) {
  std::unique_ptr<ChromePrivateNetworkDeviceChooserDesktop> chooser;
  chooser = std::make_unique<ChromePrivateNetworkDeviceChooserDesktop>();
  chooser->ShowChooser(render_frame_host, std::move(controller));
  return chooser;
}

void ChromePrivateNetworkDeviceChooserDesktop::ShowChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<PrivateNetworkDeviceChooserController> controller) {
  DCHECK(render_frame_host);
  closure_runner_.ReplaceClosure(chrome::ShowDeviceChooserDialog(
      render_frame_host, std::move(controller)));
}
