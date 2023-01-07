// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_SCOPED_RELAUNCH_CHROME_BROWSER_OVERRIDE_H_
#define CHROME_BROWSER_FIRST_RUN_SCOPED_RELAUNCH_CHROME_BROWSER_OVERRIDE_H_

#include "base/functional/callback.h"
#include "chrome/browser/first_run/upgrade_util.h"

namespace upgrade_util {

// A test helper that overrides RelaunchChromeBrowser with a given callback for
// the lifetime of an instance. The previous callback (or none) is restored
// upon deletion.
class ScopedRelaunchChromeBrowserOverride {
 public:
  explicit ScopedRelaunchChromeBrowserOverride(
      RelaunchChromeBrowserCallback callback);

  ScopedRelaunchChromeBrowserOverride(
      const ScopedRelaunchChromeBrowserOverride&) = delete;
  ScopedRelaunchChromeBrowserOverride& operator=(
      const ScopedRelaunchChromeBrowserOverride&) = delete;

  ~ScopedRelaunchChromeBrowserOverride();

 private:
  RelaunchChromeBrowserCallback previous_;
};

}  // namespace upgrade_util

#endif  // CHROME_BROWSER_FIRST_RUN_SCOPED_RELAUNCH_CHROME_BROWSER_OVERRIDE_H_
