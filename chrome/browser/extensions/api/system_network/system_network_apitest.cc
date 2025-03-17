// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {
namespace {

using SystemNetworkApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(SystemNetworkApiTest, SystemNetworkExtension) {
  ASSERT_TRUE(RunExtensionTest("system_network")) << message_;
}

}  // namespace
}  // namespace extensions
