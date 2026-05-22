// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_atoms_logger.h"

#include "base/containers/fixed_flat_map.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome::android::westworld {

namespace {

struct LoggedAtom {
  int atom_id;
  MetricType type;
  base::HistogramBase::Sample32 sample;
};

class TestAndroidAtomsLogger : public AndroidAtomsLogger {
 public:
  explicit TestAndroidAtomsLogger(base::span<const HistogramInfo> allowlist)
      : AndroidAtomsLogger(allowlist) {}

  ~TestAndroidAtomsLogger() override = default;

  void LogAtom(int atom_id,
               MetricType type,
               base::HistogramBase::Sample32 sample) override {
    logged_atoms_.push_back({atom_id, type, sample});
  }

  std::vector<LoggedAtom> logged_atoms_;
};

}  // namespace

class AndroidAtomsLoggerTest : public testing::Test {
 public:
  AndroidAtomsLoggerTest() = default;
  ~AndroidAtomsLoggerTest() override = default;

 protected:
  static constexpr HistogramInfo kTestAllowlist[] = {
      {"TEST_HISTOGRAM", 12345, MetricType::kInt},
  };

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AndroidAtomsLoggerTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAndroidAtomsLogging);

  TestAndroidAtomsLogger logger(kTestAllowlist);

  EXPECT_TRUE(logger.observers_.empty());
}

TEST_F(AndroidAtomsLoggerTest, FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAndroidAtomsLogging);

  TestAndroidAtomsLogger logger(kTestAllowlist);

  EXPECT_EQ(logger.observers_.size(), std::size(kTestAllowlist));
}

}  // namespace chrome::android::westworld
