// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/guest_os_installer/guest_os_installer_ui.h"

#include "ash/webui/grit/ash_guest_os_installer_resources.h"
#include "ash/webui/grit/ash_guest_os_installer_resources_map.h"
#include "ash/webui/guest_os_installer/url_constants.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

GuestOSInstallerUI::GuestOSInstallerUI(content::WebUI* web_ui,
                                       DelegateFactory delegate_factory)
    : ui::MojoWebDialogUI(web_ui), delegate_factory_(delegate_factory) {
  auto* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      ash::kChromeUIGuestOSInstallerHost);

  source->DisableTrustedTypesCSP();

  source->AddResourcePaths(base::make_span(kAshGuestOsInstallerResources,
                                           kAshGuestOsInstallerResourcesSize));
  source->SetDefaultResource(IDR_ASH_GUEST_OS_INSTALLER_INDEX_HTML);
}

GuestOSInstallerUI::~GuestOSInstallerUI() = default;

void GuestOSInstallerUI::BindInterface(
    mojo::PendingReceiver<ash::guest_os_installer::mojom::PageHandlerFactory>
        pending_receiver) {
  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void GuestOSInstallerUI::CreatePageHandler(
    mojo::PendingRemote<ash::guest_os_installer::mojom::Page> pending_page,
    mojo::PendingReceiver<ash::guest_os_installer::mojom::PageHandler>
        pending_page_handler) {
  // Code under //ash/webui isn't allowed to know about types under //chrome,
  // which is where all the actual GuestOS implementations live. To get around
  // this we delegate actually picking a backend to this delegate factory
  // callback, which lives in //chrome and is passed to us by our constructor
  // (which also lives in //chrome).
  handler_ = delegate_factory_.Run(this, std::move(pending_page),
                                   std::move(pending_page_handler));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GuestOSInstallerUI)

}  // namespace ash
