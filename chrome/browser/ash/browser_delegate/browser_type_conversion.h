// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_TYPE_CONVERSION_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_TYPE_CONVERSION_H_

#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace ash {

BrowserWindowInterface::Type ToInternalBrowserType(ash::BrowserType type);
BrowserType FromInternalBrowserType(BrowserWindowInterface::Type internal_type);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_TYPE_CONVERSION_H_
