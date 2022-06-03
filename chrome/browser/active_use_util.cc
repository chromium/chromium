// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/active_use_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/buildflags.h"

bool ShouldRecordActiveUse(const base::CommandLine& command_line) {
#if defined(OS_WIN) && !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return false;
#else
  return command_line.GetSwitchValueNative(switches::kTryChromeAgain).empty();
#endif
}
