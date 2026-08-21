// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/llvm_profile_util.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LLVMProfileUtilTest, ReplacesDefaultPrefix) {
  // Simple filenames without path.
  EXPECT_EQ(GetLLVMProfileFilename("default-%2m.profraw",
                                   ProfileProcessType::kRenderer),
            "renderer-%2m.profraw");
  EXPECT_EQ(GetLLVMProfileFilename("default-%p-%m.profraw",
                                   ProfileProcessType::kRenderer),
            "renderer-%p-%m.profraw");

  // POSIX directory paths.
  EXPECT_EQ(GetLLVMProfileFilename("/path/to/profile/default-%2m.profraw",
                                   ProfileProcessType::kRenderer),
            "/path/to/profile/renderer-%2m.profraw");
  EXPECT_EQ(GetLLVMProfileFilename("/default-%p.profraw",
                                   ProfileProcessType::kRenderer),
            "/renderer-%p.profraw");

#if BUILDFLAG(IS_WIN)
  // Windows directory paths with backslashes.
  EXPECT_EQ(GetLLVMProfileFilename("C:\\Users\\profile\\default-%p-%m.profraw",
                                   ProfileProcessType::kRenderer),
            "C:\\Users\\profile\\renderer-%p-%m.profraw");
  EXPECT_EQ(GetLLVMProfileFilename("\\\\server\\share\\default-%p.profraw",
                                   ProfileProcessType::kRenderer),
            "\\\\server\\share\\renderer-%p.profraw");
#else
  // On non-Windows, backslashes are treated as regular filename characters.
  EXPECT_EQ(GetLLVMProfileFilename("/path/to/profile\\default-%2m.profraw",
                                   ProfileProcessType::kRenderer),
            "");
#endif

  // Paths without "default-" prefix are left untouched (returns empty string).
  EXPECT_EQ(GetLLVMProfileFilename("custom-%p.profraw",
                                   ProfileProcessType::kRenderer),
            "");
  EXPECT_EQ(GetLLVMProfileFilename("/path/to/custom-%p.profraw",
                                   ProfileProcessType::kRenderer),
            "");
  EXPECT_EQ(
      GetLLVMProfileFilename("default.profraw", ProfileProcessType::kRenderer),
      "");
  EXPECT_EQ(GetLLVMProfileFilename("", ProfileProcessType::kRenderer), "");
}
