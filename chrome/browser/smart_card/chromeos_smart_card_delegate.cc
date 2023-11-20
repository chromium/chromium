// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"
#include "chrome/browser/smart_card/get_smart_card_context_factory.h"

ChromeOsSmartCardDelegate::ChromeOsSmartCardDelegate() = default;

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
ChromeOsSmartCardDelegate::GetSmartCardContextFactory(
    content::BrowserContext& browser_context) {
  return ::GetSmartCardContextFactory(browser_context);
}

bool ChromeOsSmartCardDelegate::HasReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name) {
  // TODO(crbug.com/1386175): Ask permission context.
  return true;
}

void ChromeOsSmartCardDelegate::RequestReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name,
    RequestReaderPermissionCallback callback) {
  // TODO(crbug.com/1386175): Show permission prompt. Call permission context
  // according to prompt response.
  std::move(callback).Run(true);
}
