// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/installer_delegate_factory.h"

#include "ash/webui/guest_os_installer/guest_os_installer_ui.h"

namespace guest_os {

std::unique_ptr<ash::guest_os_installer::mojom::PageHandler>
InstallerDelegateFactory(
    ash::GuestOSInstallerUI* webui,
    const GURL& url,
    mojo::PendingRemote<ash::guest_os_installer::mojom::Page> pending_page,
    mojo::PendingReceiver<ash::guest_os_installer::mojom::PageHandler>
        pending_page_handler) {
  return nullptr;
}

}  // namespace guest_os
