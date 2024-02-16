// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class EmbeddedA11yExtensionLoaderTest : public InProcessBrowserTest {
 public:
  EmbeddedA11yExtensionLoaderTest() = default;
  ~EmbeddedA11yExtensionLoaderTest() override = default;
  EmbeddedA11yExtensionLoaderTest(const EmbeddedA11yExtensionLoaderTest&) =
      delete;
  EmbeddedA11yExtensionLoaderTest& operator=(
      const EmbeddedA11yExtensionLoaderTest&) = delete;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(EmbeddedA11yExtensionLoaderTest,
                       InstallsAndRemovesExtension) {
  auto* embedded_a11y_extension_loader =
      EmbeddedA11yExtensionLoader::GetInstance();
  EXPECT_FALSE(
      EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());

  embedded_a11y_extension_loader->InstallA11yHelperExtensionForReadingMode();
  EXPECT_TRUE(EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());
  // Call InstallA11yHelperExtensionForReadingMode a second time, nothing bad
  // happens.
  embedded_a11y_extension_loader->InstallA11yHelperExtensionForReadingMode();

  embedded_a11y_extension_loader->RemoveA11yHelperExtensionForReadingMode();
  EXPECT_FALSE(
      EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());
  // Call RemoveA11yHelperExtensionForReadingMode a second time, nothing bad
  // happens.
  embedded_a11y_extension_loader->RemoveA11yHelperExtensionForReadingMode();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
