// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_kiosk_delegate.h"

namespace extensions {

bool ChromeKioskDelegate::IsAutoLaunchedKioskApp(const ExtensionId& id) const {
  return false;
}

}  // namespace extensions
