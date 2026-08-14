// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/sapisid/sapisid_module_loader.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sapisid {

TEST(SapisidModuleLoaderTest, LoadLibrary) {
  const auto& library = SapisidModuleLoader::GetInstance()->library();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)
  // On branded builds, the library should be correctly built.
  EXPECT_TRUE(library.is_valid());
#else
  // Unbranded builds definitely shouldn't find it.
  EXPECT_FALSE(library.is_valid());
#endif
}

}  // namespace sapisid
