// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_chrome_service_delegate_impl.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

// Overrides base::SysInfo::IsRunningOnChromeOS() to return true.
// TODO(jamescook): Switch to the shared helper once crrev.com/c/2538285 lands.
class ScopedIsRunningOnChromeOS {
 public:
  ScopedIsRunningOnChromeOS() {
    base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  }
  ~ScopedIsRunningOnChromeOS() {
    base::SysInfo::SetChromeOSVersionInfoForTest("", base::Time());
  }
};

class LacrosChromeServiceDelegateImplTest : public testing::Test {
 public:
  LacrosChromeServiceDelegateImplTest() = default;
  LacrosChromeServiceDelegateImplTest(
      const LacrosChromeServiceDelegateImplTest&) = delete;
  LacrosChromeServiceDelegateImplTest& operator=(
      const LacrosChromeServiceDelegateImplTest&) = delete;
  ~LacrosChromeServiceDelegateImplTest() override = default;

 private:
  ScopedIsRunningOnChromeOS running_on_chromeos_;
  // Ensure we restore the previous paths for subsequent tests. We don't
  // actually use these paths, so just point them at /tmp to avoid
  // ScopedPathOverride from creating unnecessary temp directories.
  base::ScopedPathOverride documents_override_{chrome::DIR_USER_DOCUMENTS,
                                               base::FilePath("/tmp")};
  base::ScopedPathOverride downloads_override_{chrome::DIR_DEFAULT_DOWNLOADS,
                                               base::FilePath("/tmp")};
  base::ScopedPathOverride downloads_safe_override_{
      chrome::DIR_DEFAULT_DOWNLOADS_SAFE, base::FilePath("/tmp")};
};

TEST_F(LacrosChromeServiceDelegateImplTest, DefaultPaths) {
  LacrosChromeServiceDelegateImpl delegate_impl;

  // Simulate ash sending some paths at startup.
  auto default_paths = crosapi::mojom::DefaultPaths::New();
  default_paths->documents = base::FilePath("/test/documents");
  default_paths->downloads = base::FilePath("/test/downloads");
  auto init_params = crosapi::mojom::LacrosInitParams::New();
  init_params->default_paths = std::move(default_paths);
  delegate_impl.OnInitialized(*init_params);

  // PathService has the new values.
  base::FilePath path;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &path);
  EXPECT_EQ(path.AsUTF8Unsafe(), "/test/documents");
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &path);
  EXPECT_EQ(path.AsUTF8Unsafe(), "/test/downloads");
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &path);
  EXPECT_EQ(path.AsUTF8Unsafe(), "/test/downloads");
}

// TODO(https://crbug.com/1150702): Delete this test after Lacros drops
// support for Chrome OS M89.
TEST_F(LacrosChromeServiceDelegateImplTest, DefaultPathsWithLegacyAsh) {
  LacrosChromeServiceDelegateImpl delegate_impl;

  // Simulate ash not sending paths at startup.
  auto init_params = crosapi::mojom::LacrosInitParams::New();
  ASSERT_TRUE(init_params->default_paths.is_null());
  delegate_impl.OnInitialized(*init_params);

  // PathService has reasonable values.
  base::FilePath path;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &path);
  EXPECT_EQ(path.AsUTF8Unsafe(), "/home/chronos/user/MyFiles");
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &path);
  EXPECT_EQ(path.AsUTF8Unsafe(), "/home/chronos/user/MyFiles/Downloads");
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &path);
  EXPECT_EQ(path.AsUTF8Unsafe(), "/home/chronos/user/MyFiles/Downloads");
}

}  // namespace
