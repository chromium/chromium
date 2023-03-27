// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_
#define CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_

#include "content/public/browser/smart_card_delegate.h"

class ChromeOsSmartCardDelegate : public content::SmartCardDelegate {
 public:
  ChromeOsSmartCardDelegate();

  // `content::SmartCardDelegate` overrides:
  mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory(content::BrowserContext& browser_context) override;
  bool SupportsReaderAddedRemovedNotifications() const override;
};

#endif  // CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_
