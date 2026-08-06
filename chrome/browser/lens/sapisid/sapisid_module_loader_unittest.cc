// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/sapisid/sapisid_module_loader.h"

#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sapisid {

TEST(SapisidModuleLoaderTest, LoadLibrary) {
  const auto& library = SapisidModuleLoader::GetInstance()->library();
  // Since the Base CL does not fetch the internal module (data_deps is commented out
  // to avoid CQ Cq-Depend limitations), the library should gracefully fail to load
  // everywhere, whether branded or unbranded.
  EXPECT_FALSE(library.is_valid());
}

}  // namespace sapisid
