// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

using ResourcesPrivateApiTest = extensions::ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ResourcesPrivateApiTest, GetStrings) {
  ASSERT_TRUE(RunExtensionTest("resources_private/get_strings", {},
                               {.load_as_component = true}))
      << message_;
}
