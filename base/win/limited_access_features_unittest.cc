// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/limited_access_features.h"

#include "base/win/scoped_com_initializer.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

// Because accessing limited access features requires adding a resource
// to the .rc file, and tests don't have .rc files, all we can test
// is that requesting access to a feature fails gracefully.
// Use the taskbar pinning limited access feature token since it's the only
// token Chrome has. Arm64 Windows seems to be allowing taskbar pinning without
// a token, so ask for a non-existent feature.
TEST(LimitedAccessFeatures, UnregisteredFeature) {
  const std::wstring taskbar_api_token =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      L"InBNYixzyiUzivxj5T/HqA==";
#else
      L"ILzQYl3daXqTIyjmNj5xwg==";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());
  EXPECT_FALSE(TryToUnlockLimitedAccessFeature(
      L"com.microsoft.windows.nonexistent.feature", taskbar_api_token));
}

}  // namespace base::win
