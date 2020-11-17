// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

namespace {

using TabGroupsApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupsWorks) {
#if defined(OS_LINUX)
  // TODO(crbug.com/1148195): Fix flakiness of this text on Linux.
  return;
#endif

  ASSERT_TRUE(RunExtensionTestWithFlags("tab_groups",
                                        kFlagIgnoreManifestWarnings, kFlagNone))
      << message_;
}

}  // namespace
}  // namespace extensions
