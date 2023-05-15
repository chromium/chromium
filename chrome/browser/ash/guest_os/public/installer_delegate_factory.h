// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_INSTALLER_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_INSTALLER_DELEGATE_FACTORY_H_

#include <memory>

#include "ash/webui/guest_os_installer/mojom/guest_os_installer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {
class GuestOSInstallerUI;
}

namespace guest_os {

std::unique_ptr<ash::guest_os_installer::mojom::PageHandler>
InstallerDelegateFactory(
    ash::GuestOSInstallerUI*,
    mojo::PendingRemote<ash::guest_os_installer::mojom::Page>,
    mojo::PendingReceiver<ash::guest_os_installer::mojom::PageHandler>);
}

#endif
