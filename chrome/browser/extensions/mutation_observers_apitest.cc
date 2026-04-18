// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MutationObservers) {
  ASSERT_TRUE(RunExtensionTest("mutation_observers")) << message_;
}

}  // namespace extensions
