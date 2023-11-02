// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "ui/base/fullscreen_win.h"

bool IsFullScreenMode() {
  return ui::IsFullScreenMode();
}
