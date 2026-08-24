// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"

namespace {

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
