// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include "base/files/file_path.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/process_handle.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/file_metrics_provider.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif

namespace {

bool TestIsProcessRunning(base::ProcessId pid) {
  // Odd are running, even are not.
  return (pid & 1) == 1;
}

TEST(ChromeMetricsServiceClientTest, FilterFiles) {
  ChromeMetricsServiceClient::SetIsProcessRunningForTesting(
      &TestIsProcessRunning);

  base::ProcessId my_pid = base::GetCurrentProcId();
  base::FilePath active_dir(FILE_PATH_LITERAL("foo"));
  base::FilePath upload_dir(FILE_PATH_LITERAL("bar"));
  base::FilePath upload_path;
  base::GlobalHistogramAllocator::ConstructFilePathsForUploadDir(
      active_dir, upload_dir, "TestMetrics", &upload_path, nullptr, nullptr);
  EXPECT_EQ(metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID,
            ChromeMetricsServiceClient::FilterBrowserMetricsFiles(upload_path));

  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_PROCESS_FILE,
      ChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 10)));
  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_TRY_LATER,
      ChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 11)));
}

}  // namespace

// This can't be a MAYBE test because it won't compile without the extensions
// header files but those can't even be included if this build flag is not
// set. This can't be in the anonymous namespace because it is a "friend" of
// the ChromeMetricsServiceClient class.
#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST(ChromeMetricsServiceClientTest, IsWebstoreExtension) {
  static const char test_extension_id1[] = "abcdefghijklmnopqrstuvwxyzabcdef";
  static const char test_extension_id2[] = "bhcnanendmgjjeghamaccjnochlnhcgj";

  content::TestBrowserThreadBundle thread_bundle;
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  TestingProfile* test_profile =
      testing_profile_manager.CreateTestingProfile("p1");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(test_profile);
  ASSERT_TRUE(registry);

  scoped_refptr<const extensions::Extension> extension1 =
      extensions::ExtensionBuilder("e1").SetID(test_extension_id1).Build();
  registry->AddEnabled(extension1);

  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("e2")
          .SetID(test_extension_id2)
          .AddFlags(extensions::Extension::FROM_WEBSTORE)
          .Build();
  registry->AddEnabled(extension2);

  EXPECT_FALSE(ChromeMetricsServiceClient::IsWebstoreExtension("foo"));
  EXPECT_FALSE(
      ChromeMetricsServiceClient::IsWebstoreExtension(test_extension_id1));
  EXPECT_TRUE(
      ChromeMetricsServiceClient::IsWebstoreExtension(test_extension_id2));
}
#endif
