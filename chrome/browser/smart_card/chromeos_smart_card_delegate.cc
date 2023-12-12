// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/get_smart_card_context_factory.h"
#include "chrome/browser/smart_card/smart_card_permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

ChromeOsSmartCardDelegate::ChromeOsSmartCardDelegate() = default;
ChromeOsSmartCardDelegate::~ChromeOsSmartCardDelegate() = default;

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
ChromeOsSmartCardDelegate::GetSmartCardContextFactory(
    content::BrowserContext& browser_context) {
  return ::GetSmartCardContextFactory(browser_context);
}

bool ChromeOsSmartCardDelegate::HasReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name) {
  // TODO(crbug.com/1386175): Ask permission context.
  return false;  // Asks every time
}

void ChromeOsSmartCardDelegate::RequestReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name,
    RequestReaderPermissionCallback callback) {
  const url::Origin& origin =
      render_frame_host.GetMainFrame()->GetLastCommittedOrigin();

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    LOG(ERROR) << "Cannot request permission: no PermissionRequestManager";
    std::move(callback).Run(false);
    return;
  }

  // Regarding ownership: The request will delete itself once the request
  // manager notifies that it can do so.
  auto* permission_request = new SmartCardPermissionRequest(
      origin, reader_name,
      base::BindOnce(&ChromeOsSmartCardDelegate::OnPermissionRequestDecided,
                     weak_factory_.GetWeakPtr(), origin, reader_name,
                     std::move(callback)));

  permission_request_manager->AddRequest(&render_frame_host,
                                         permission_request);
}

void ChromeOsSmartCardDelegate::OnPermissionRequestDecided(
    const url::Origin& origin,
    const std::string& reader_name,
    RequestReaderPermissionCallback callback,
    SmartCardPermissionRequest::Result result) {
  switch (result) {
    case SmartCardPermissionRequest::Result::kAllowOnce:
      // TODO(crbug.com/1386175): Set ephemeral grant in permission context.
      std::move(callback).Run(true);
      break;
    case SmartCardPermissionRequest::Result::kAllowAlways:
      // TODO(crbug.com/1386175): Set persistent grant in permission context.
      std::move(callback).Run(true);
      break;
    case SmartCardPermissionRequest::Result::kDontAllow:
      // There's no block list. Origin is free to request again.
      std::move(callback).Run(false);
      break;
  }
}
