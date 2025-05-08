// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_type_conversion.h"

namespace ash {

BrowserWindowInterface::Type ToInternalBrowserType(ash::BrowserType type) {
  switch (type) {
    case BrowserType::kApp:
      return BrowserWindowInterface::TYPE_APP;
    case BrowserType::kAppPopup:
      return BrowserWindowInterface::TYPE_APP_POPUP;
    case BrowserType::kOther:
      NOTREACHED();
  }
}

BrowserType FromInternalBrowserType(
    BrowserWindowInterface::Type internal_type) {
  switch (internal_type) {
    case BrowserWindowInterface::TYPE_APP:
      return BrowserType::kApp;
    case BrowserWindowInterface::TYPE_APP_POPUP:
      return BrowserType::kAppPopup;
    default:
      return BrowserType::kOther;
  }
}

}  // namespace ash
