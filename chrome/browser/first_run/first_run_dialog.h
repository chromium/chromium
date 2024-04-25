// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

// Hide this function on platforms where the dialog does not exist.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))

namespace first_run {

// Shows the first run dialog. Only called for organic first runs on Mac and
// desktop Linux official builds when metrics reporting is not already enabled.
// Invokes ChangeMetricsReportingState() if consent is given to enable crash
// reporting, and may initiate the flow to set the default browser.
void ShowFirstRunDialog();
void ShowFirstRunDialogViews();
// Maintain Cocoa-based first run dialog until we are confident that views'
// implementation works well on macOS.
#if BUILDFLAG(IS_MAC)
void ShowFirstRunDialogCocoa();
#endif

// Returns a Closure invoked before calling ShowFirstRunDialog(). For testing.
base::OnceClosure& GetBeforeShowFirstRunDialogHookForTesting();

}  // namespace first_run

#endif

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_
