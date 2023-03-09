// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/extension_cache_impl.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

const char kTestExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtensionId3[] = "cccccccccccccccccccccccccccccccc";

// 2 megabytes are a little less than 2 100 000 bytes, so the third extension
// will not be kept.
const size_t kMaxCacheSize = 2 * 1024 * 1024;
const size_t kExtensionSize1 = 1000 * 1000;
const size_t kExtensionSize2 = 1000 * 1000;
const size_t kExtensionSize3 = 100 * 1000;

static void CreateFile(const base::FilePath& file,
                       size_t size,
                       const base::Time& timestamp) {
  const std::string data(size, 0);
  EXPECT_TRUE(base::WriteFile(file, data));
  EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));
}

static base::FilePath CreateExtensionFile(const base::FilePath& dir,
                                          const std::string& id,
                                          const std::string& version,
                                          size_t size,
                                          const base::Time& timestamp) {
  const base::FilePath filename = dir.Append(
      LocalExtensionCache::ExtensionFileName(id, version, "" /* hash */));
  CreateFile(filename, size, timestamp);
  return filename;
}

}  // namespace

class ExtensionCacheTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

TEST_F(ExtensionCacheTest, SizePolicy) {
  scoped_testing_cros_settings_.device_settings()->SetInteger(
      ash::kExtensionCacheSize, kMaxCacheSize);

  // Create and initialize local cache.
  const base::Time now = base::Time::Now();
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();
  CreateFile(cache_path.Append(LocalExtensionCache::kCacheReadyFlagFileName), 0,
             now);

  // The extension cache only has enough space for two of the three extensions.
  const base::FilePath file1 =
      CreateExtensionFile(cache_path, kTestExtensionId1, "1.0", kExtensionSize1,
                          now - base::Seconds(1));
  const base::FilePath file2 =
      CreateExtensionFile(cache_path, kTestExtensionId2, "2.0", kExtensionSize2,
                          now - base::Seconds(2));
  const base::FilePath file3 =
      CreateExtensionFile(cache_path, kTestExtensionId3, "3.0", kExtensionSize3,
                          now - base::Seconds(3));

  ExtensionCacheImpl cache_impl(
      std::make_unique<ChromeOSExtensionCacheDelegate>(cache_path));

  std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
  cache_impl.Start(run_loop->QuitClosure());
  run_loop->Run();

  run_loop = std::make_unique<base::RunLoop>();
  cache_impl.Shutdown(run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_TRUE(base::PathExists(file1));
  EXPECT_TRUE(base::PathExists(file2));
  EXPECT_FALSE(base::PathExists(file3));
}

}  // namespace extensions
