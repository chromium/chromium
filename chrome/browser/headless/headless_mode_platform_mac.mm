// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_platform.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace headless {

namespace {

void PreventDockIconAndMenu() {
  // Transform the process to a background agent (LSUIElement) to hide it from
  // the Dock and menu bar.
  ProcessSerialNumber psn = {0, kCurrentProcess};
  OSStatus status =
      TransformProcessType(&psn, kProcessTransformToUIElementApplication);
  CHECK_EQ(status, noErr);
}

}  // namespace

void InitializePlatform() {
  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());

  if (!command_line.HasSwitch(::switches::kProcessType)) {
    PreventDockIconAndMenu();
  }
}

}  // namespace headless
