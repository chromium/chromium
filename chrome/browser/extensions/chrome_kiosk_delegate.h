// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_KIOSK_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_KIOSK_DELEGATE_H_

#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Delegate in Chrome that provides an extension/app API with Kiosk mode
// functionality.
class ChromeKioskDelegate : public KioskDelegate {
 public:
  ChromeKioskDelegate() = default;
  ~ChromeKioskDelegate() override = default;

  // KioskDelegate overrides:
  bool IsAutoLaunchedKioskApp(const ExtensionId& id) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_KIOSK_DELEGATE_H_
