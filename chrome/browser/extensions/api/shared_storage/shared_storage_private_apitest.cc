// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

using SharedStoragePrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateApiTest, Test) {
  ASSERT_TRUE(RunExtensionTest("shared_storage_private")) << message_;
}

}  // namespace extensions
