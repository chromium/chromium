// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

using SharedArrayBufferTest = ExtensionApiTest;

// Ensures extensions can use the SharedArrayBuffer API.
IN_PROC_BROWSER_TEST_F(SharedArrayBufferTest, TransferToWorker) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("shared_array_buffers")) << message_;
}

}  // namespace extensions
