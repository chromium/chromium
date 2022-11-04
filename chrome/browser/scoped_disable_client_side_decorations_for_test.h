// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCOPED_DISABLE_CLIENT_SIDE_DECORATIONS_FOR_TEST_H_
#define CHROME_BROWSER_SCOPED_DISABLE_CLIENT_SIDE_DECORATIONS_FOR_TEST_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/platform_utils.h"
#endif

namespace ui {

// Disables client-side decorations on Wayland for the lifetime of the object.
// On Wayland, enabling CSD affects the window geometry and makes it a bit
// smaller than it was before enabling CSD.  Some tests fail because of that.
// TODO(crbug.com/1240482): investigate why exactly tests fail, and if possible,
// fix them so they would not need this class.
class ScopedDisableClientSideDecorationsForTest {
 public:
  ScopedDisableClientSideDecorationsForTest();
  ScopedDisableClientSideDecorationsForTest(
      const ScopedDisableClientSideDecorationsForTest&) = delete;
  ScopedDisableClientSideDecorationsForTest& operator=(
      const ScopedDisableClientSideDecorationsForTest&) = delete;
  ~ScopedDisableClientSideDecorationsForTest();

#if BUILDFLAG(IS_OZONE)
 private:
  std::unique_ptr<PlatformUtils::ScopedDisableClientSideDecorationsForTest>
      disabled_csd_;
#endif
};

}  // namespace ui

#endif  // CHROME_BROWSER_SCOPED_DISABLE_CLIENT_SIDE_DECORATIONS_FOR_TEST_H_
