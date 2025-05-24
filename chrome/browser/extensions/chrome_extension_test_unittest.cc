// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_test.h"

#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

// The following tests are meant to be more "smoke tests" for the
// ChromeExtensionTest test suite itself, rather than exercising any
// particular functionality. More specific tests should be proximal to the code
// they are testing.

TEST_F(ChromeExtensionTest, AddSimpleExtension) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("my extension").Build();
  ASSERT_TRUE(extension);

  extension_registrar()->AddExtension(extension.get());

  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension->id()));
}

}  // namespace extensions
