// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Storage) {
  ASSERT_TRUE(RunExtensionTest("storage")) << message_;
}

}  // namespace extensions
