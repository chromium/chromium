// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_util.h"
#include "chromeos/ui/base/window_pin_type.h"

namespace extensions {
namespace tabs_util {

void SetLockedFullscreenState(Browser* browser, bool pinned) {}

bool IsScreenshotRestricted(content::WebContents* web_contents) {
  return false;
}

}  // namespace tabs_util
}  // namespace extensions
