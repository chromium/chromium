// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

using ComponentExtensionsTest = ExtensionBrowserTest;

// Tests that the mobile_app component extension loads. It would be nice to get
// rid of this (see https://crbug.com/835391), but for now let's at least make
// sure it is added correctly.
IN_PROC_BROWSER_TEST_F(ComponentExtensionsTest, LoadsMobileAppExtension) {
  constexpr char kMobileActivationExtensionId[] =
      "iadeocfgjdjdmpenejdbfeaocpbikmab";
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
      kMobileActivationExtensionId));
}

}  // namespace extensions
