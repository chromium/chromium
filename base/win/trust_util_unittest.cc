// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/trust_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)

namespace base::win {

TEST(TrustUtilTest, UnsignedOrNonexistentBinary) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath nonexistent_file =
      temp_dir.GetPath().AppendASCII("nonexistent.exe");
  EXPECT_FALSE(IsBinaryTrusted(nonexistent_file));
  EXPECT_FALSE(IsBinaryTrusted(nonexistent_file, /*verify_publisher=*/false,
                               /*force_verify_in_dev_builds=*/true));

  base::FilePath dummy_file = temp_dir.GetPath().AppendASCII("dummy.dll");
  ASSERT_TRUE(base::WriteFile(dummy_file, "dummy_content"));

  // For unsigned files, force_verify_in_dev_builds=true must return false.
  EXPECT_FALSE(IsBinaryTrusted(dummy_file, /*verify_publisher=*/false,
                               /*force_verify_in_dev_builds=*/true));

  // Without force_verify_in_dev_builds in non-branded builds, it bypasses
  // verification for existing files.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(NDEBUG)
  EXPECT_FALSE(IsBinaryTrusted(dummy_file));
#else
  // Signature validation always returns true for non-chrome branded or debug
  // builds.
  EXPECT_TRUE(IsBinaryTrusted(dummy_file));
#endif
}

TEST(TrustUtilTest, CurrentExecutableSubject) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &exe_path));

  // Test binaries are never signed, even in branded builds. Therefore, forcing
  // signature verification on the current executable always returns false.
  EXPECT_FALSE(IsBinaryTrusted(exe_path, /*verify_publisher=*/true,
                               /*force_verify_in_dev_builds=*/true));

  // Without force_verify_in_dev_builds, branded builds enforce signature
  // verification by default (returning false for unsigned test binaries),
  // whereas non-branded builds bypass verification by default.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(NDEBUG)
  EXPECT_FALSE(IsBinaryTrusted(exe_path));
#else
  EXPECT_TRUE(IsBinaryTrusted(exe_path));
#endif
}

TEST(TrustUtilTest, ScopedWintrustDataWithHandle) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath dummy_file = temp_dir.GetPath().AppendASCII("dummy.dll");
  ASSERT_TRUE(base::WriteFile(dummy_file, "dummy_content"));

  base::File file(dummy_file, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                  base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_TRUE(file.IsValid());

  ScopedWintrustData wintrust_data(dummy_file, file.GetPlatformFile());
  EXPECT_FALSE(wintrust_data.is_valid());
}

}  // namespace base::win

#endif  // BUILDFLAG(IS_WIN)
