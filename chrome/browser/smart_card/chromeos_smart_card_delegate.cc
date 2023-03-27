// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"
#include "chrome/browser/chromeos/extensions/smart_card_provider_private/smart_card_provider_private_api.h"

ChromeOsSmartCardDelegate::ChromeOsSmartCardDelegate() = default;

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
ChromeOsSmartCardDelegate::GetSmartCardContextFactory(
    content::BrowserContext& browser_context) {
  return extensions::SmartCardProviderPrivateAPI::Get(browser_context)
      .GetSmartCardContextFactory();
}

bool ChromeOsSmartCardDelegate::SupportsReaderAddedRemovedNotifications()
    const {
  return true;
}
