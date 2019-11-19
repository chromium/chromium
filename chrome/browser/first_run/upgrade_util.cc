// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

namespace {

#if !defined(OS_MACOSX)
base::CommandLine* command_line = nullptr;
#endif

// A test seam for whole-browser tests to override browser relaunch.
upgrade_util::RelaunchChromeBrowserCallback*
    relaunch_chrome_browser_callback_for_testing = nullptr;

}  // namespace

namespace upgrade_util {

// Forward-declaration of the platform-specific implementation.
bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line);

bool RelaunchChromeBrowser(const base::CommandLine& command_line) {
  if (relaunch_chrome_browser_callback_for_testing)
    return relaunch_chrome_browser_callback_for_testing->Run(command_line);

  return RelaunchChromeBrowserImpl(command_line);
}

#if !defined(OS_MACOSX)

void SetNewCommandLine(std::unique_ptr<base::CommandLine> new_command_line) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete command_line;
  command_line = new_command_line.release();
}

void RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  if (command_line) {
    if (!RelaunchChromeBrowser(*command_line)) {
      DLOG(ERROR) << "Launching a new instance of the browser failed.";
    } else {
      DLOG(WARNING) << "Launched a new instance of the browser.";
    }
    delete command_line;
    command_line = nullptr;
  }
}

#endif  // !defined(OS_MACOSX)

RelaunchChromeBrowserCallback SetRelaunchChromeBrowserCallbackForTesting(
    RelaunchChromeBrowserCallback callback) {
  // Take ownership of the current test callback so it can be returned.
  RelaunchChromeBrowserCallback previous =
      relaunch_chrome_browser_callback_for_testing
          ? std::move(*relaunch_chrome_browser_callback_for_testing)
          : RelaunchChromeBrowserCallback();

  // Move the caller's callback into the global, alloc'ing or freeing as needed.
  auto memory = base::WrapUnique(relaunch_chrome_browser_callback_for_testing);
  if (callback) {
    if (!memory)
      memory = std::make_unique<RelaunchChromeBrowserCallback>();
    *memory = std::move(callback);
  } else if (memory) {
    memory.reset();
  }
  relaunch_chrome_browser_callback_for_testing = memory.release();

  // Return the previous callback.
  return previous;
}

}  // namespace upgrade_util
