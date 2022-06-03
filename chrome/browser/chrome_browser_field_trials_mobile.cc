// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_mobile.h"

#include "chrome/browser/browser_process.h"

namespace chrome {

void SetupMobileFieldTrials() {
  DCHECK(!g_browser_process);
}

}  // namespace chrome
