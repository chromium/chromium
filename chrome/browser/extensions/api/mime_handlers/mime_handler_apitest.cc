// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"

namespace extensions {

class MimeHandlerApiTest : public ExtensionApiTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kApiMimeHandler};
};

IN_PROC_BROWSER_TEST_F(MimeHandlerApiTest,
                       SetMimeHandlerOptionsWritesOnlyPresentMembers) {
  ASSERT_TRUE(RunExtensionTest("mime_handler_options")) << message_;
}

}  // namespace extensions
