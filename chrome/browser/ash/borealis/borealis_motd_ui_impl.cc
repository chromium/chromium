// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_motd_ui_impl.h"

#include "base/check.h"
#include "chrome/browser/ash/borealis/borealis_motd_page_handler_delegate.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_service_impl.h"
#include "chrome/browser/profiles/profile.h"

namespace borealis {

BorealisMotdUiImpl::BorealisMotdUiImpl(content::WebUI* web_ui)
    : BorealisMOTDUI(web_ui) {}

void BorealisMotdUiImpl::CreatePageHandler(
    mojo::PendingRemote<ash::borealis_motd::mojom::Page> pending_page,
    mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandler>
        pending_page_handler) {
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(profile);
  BorealisService* service = BorealisServiceFactory::GetForProfile(profile);
  CHECK(service);

  page_handler_ = std::make_unique<BorealisMOTDPageHandler>(
      std::make_unique<BorealisMOTDPageHandlerDelegate>(&service->Features(),
                                                        &service->Installer()),
      std::move(pending_page_handler), std::move(pending_page),
      base::BindOnce(&BorealisMotdUiImpl::OnPageClosed,
                     base::Unretained(this)));
}

WEB_UI_CONTROLLER_TYPE_IMPL(BorealisMotdUiImpl)

}  // namespace borealis
