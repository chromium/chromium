// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/accessibility_state_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace {

constexpr char kActiveClientApisHistogram[] =
    "Accessibility.Win.ActiveClientApis";
constexpr char kRequestedClientApisHistogram[] =
    "Accessibility.Win.RequestedClientApis";

class AccessibilityStateProviderWinTest : public testing::Test {
 protected:
  void CallProvideSystemProfileMetrics() {
    metrics::SystemProfileProto system_profile;
    AccessibilityStateProvider provider;
    provider.ProvideSystemProfileMetrics(&system_profile);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AccessibilityStateProviderWinTest, RecordsMsaaOnly) {
  content::ScopedAccessibilityModeOverride mode_override(
      ui::AXMode{ui::AXMode::kNativeAPIs});
  ui::AXPlatform::GetInstance().SetMsaaActive();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectUniqueSample(
      kActiveClientApisHistogram,
      ui::AXPlatform::ActiveClientApi::kMsaaOnly, 1);
}

TEST_F(AccessibilityStateProviderWinTest, RecordsUiaOnly) {
  content::ScopedAccessibilityModeOverride mode_override(
      ui::AXMode{ui::AXMode::kNativeAPIs});
  ui::AXPlatform::GetInstance().SetUiaActive();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectUniqueSample(
      kActiveClientApisHistogram,
      ui::AXPlatform::ActiveClientApi::kUiaOnly, 1);
}

TEST_F(AccessibilityStateProviderWinTest, RecordsBoth) {
  content::ScopedAccessibilityModeOverride mode_override(
      ui::AXMode{ui::AXMode::kNativeAPIs});
  ui::AXPlatform::GetInstance().SetMsaaActive();
  ui::AXPlatform::GetInstance().SetUiaActive();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectUniqueSample(
      kActiveClientApisHistogram,
      ui::AXPlatform::ActiveClientApi::kBoth, 1);
}

TEST_F(AccessibilityStateProviderWinTest, NoRecordWhenNativeApisNotActive) {
  ui::AXPlatform::GetInstance().SetMsaaActive();
  ui::AXPlatform::GetInstance().SetUiaActive();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectTotalCount(kActiveClientApisHistogram, 0);
}

TEST_F(AccessibilityStateProviderWinTest, NoRecordWhenNoApiUsed) {
  content::ScopedAccessibilityModeOverride mode_override(
      ui::AXMode{ui::AXMode::kNativeAPIs});

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectTotalCount(kActiveClientApisHistogram, 0);
}

TEST_F(AccessibilityStateProviderWinTest, RecordsMsaaRequested) {
  ui::AXPlatform::GetInstance().SetMsaaRequested();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectUniqueSample(
      kRequestedClientApisHistogram,
      ui::AXPlatform::ActiveClientApi::kMsaaOnly, 1);
}

TEST_F(AccessibilityStateProviderWinTest, RecordsUiaRequested) {
  ui::AXPlatform::GetInstance().SetUiaRequested();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectUniqueSample(
      kRequestedClientApisHistogram,
      ui::AXPlatform::ActiveClientApi::kUiaOnly, 1);
}

TEST_F(AccessibilityStateProviderWinTest, RecordsBothRequested) {
  ui::AXPlatform::GetInstance().SetMsaaRequested();
  ui::AXPlatform::GetInstance().SetUiaRequested();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectUniqueSample(
      kRequestedClientApisHistogram,
      ui::AXPlatform::ActiveClientApi::kBoth, 1);
}

TEST_F(AccessibilityStateProviderWinTest,
       RequestedRecordedWithoutNativeApis) {
  // RequestedClientApis should be recorded regardless of kNativeAPIs state.
  ui::AXPlatform::GetInstance().SetMsaaRequested();

  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectUniqueSample(
      kRequestedClientApisHistogram,
      ui::AXPlatform::ActiveClientApi::kMsaaOnly, 1);
  histogram_tester_.ExpectTotalCount(kActiveClientApisHistogram, 0);
}

TEST_F(AccessibilityStateProviderWinTest, NoRecordWhenNoApiRequested) {
  CallProvideSystemProfileMetrics();

  histogram_tester_.ExpectTotalCount(kRequestedClientApisHistogram, 0);
}

}  // namespace
