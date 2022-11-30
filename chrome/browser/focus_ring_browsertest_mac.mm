// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/focus_ring_browsertest_mac.h"

bool MacOSVersionSupportsDarkMode() {
  if (@available(macOS 10.14, *))
    return true;
  return false;
}
