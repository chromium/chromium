// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/android/chrome_startup_flags.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "media/base/media_switches.h"

void SetChromeSpecificCommandLineFlags() {
  // Enable DOM Distiller backend.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableDomDistiller))
    command_line->AppendSwitch(switches::kEnableDomDistiller);
}
