// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

using extensions_helper::DisableExtension;
using extensions_helper::EnableExtension;
using extensions_helper::GetInstalledExtensions;
using extensions_helper::InstallExtension;
using extensions_helper::InstallExtensionsPendingForSync;
using extensions_helper::IsExtensionEnabled;
using extensions_helper::UninstallExtension;
using sync_timing_helper::TimeMutualSyncCycle;

// TODO(braffert): Replicate these tests for apps.

static const int kNumExtensions = 150;

namespace {

constexpr char kMetricPrefixExtensions[] = "Extensions.";
constexpr char kMetricAddExtensionsSyncTime[] = "add_extensions_sync_time";
constexpr char kMetricUpdateExtensionsSyncTime[] =
    "update_extensions_sync_time";
constexpr char kMetricDeleteExtensionsSyncTime[] =
    "delete_extensions_sync_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixExtensions, story);
  reporter.RegisterImportantMetric(kMetricAddExtensionsSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricUpdateExtensionsSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricDeleteExtensionsSyncTime, "ms");
  return reporter;
}

}  // namespace

class ExtensionsSyncPerfTest : public SyncTest {
 public:
  ExtensionsSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  ExtensionsSyncPerfTest(const ExtensionsSyncPerfTest&) = delete;
  ExtensionsSyncPerfTest& operator=(const ExtensionsSyncPerfTest&) = delete;

  // Adds |num_extensions| new unique extensions to |profile|.
  void AddExtensions(int profile, int num_extensions);

  // Updates the enabled/disabled state for all extensions in |profile|.
  void UpdateExtensions(int profile);

  // Uninstalls all currently installed extensions from |profile|.
  void RemoveExtensions(int profile);

  // Returns the number of currently installed extensions for |profile|.
  int GetExtensionCount(int profile);

 private:
  int extension_number_ = 0;
};

void ExtensionsSyncPerfTest::AddExtensions(int profile, int num_extensions) {
  for (int i = 0; i < num_extensions; ++i) {
    InstallExtension(GetProfile(profile), extension_number_++);
  }
}

void ExtensionsSyncPerfTest::UpdateExtensions(int profile) {
  std::vector<int> extensions = GetInstalledExtensions(GetProfile(profile));
  for (int extension : extensions) {
    if (IsExtensionEnabled(GetProfile(profile), extension)) {
      DisableExtension(GetProfile(profile), extension);
    } else {
      EnableExtension(GetProfile(profile), extension);
    }
  }
}

int ExtensionsSyncPerfTest::GetExtensionCount(int profile) {
  return GetInstalledExtensions(GetProfile(profile)).size();
}

void ExtensionsSyncPerfTest::RemoveExtensions(int profile) {
  std::vector<int> extensions = GetInstalledExtensions(GetProfile(profile));
  for (int extension : extensions) {
    UninstallExtension(GetProfile(profile), extension);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionsSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  int num_default_extensions = GetExtensionCount(0);
  int expected_extension_count = num_default_extensions + kNumExtensions;

  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::NumberToString(kNumExtensions) + "_extensions");
  AddExtensions(0, kNumExtensions);
  base::TimeDelta dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  InstallExtensionsPendingForSync(GetProfile(1));
  ASSERT_EQ(expected_extension_count, GetExtensionCount(1));
  reporter.AddResult(kMetricAddExtensionsSyncTime, dt);

  UpdateExtensions(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(expected_extension_count, GetExtensionCount(1));
  reporter.AddResult(kMetricUpdateExtensionsSyncTime, dt);

  RemoveExtensions(0);
  dt = TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(num_default_extensions, GetExtensionCount(1));
  reporter.AddResult(kMetricDeleteExtensionsSyncTime, dt);
}
