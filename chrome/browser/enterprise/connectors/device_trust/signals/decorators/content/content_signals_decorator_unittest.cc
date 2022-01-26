// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/content/content_signals_decorator.h"

#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Content";

}  // namespace

class ContentSignalsDecoratorTest : public testing::Test {
 protected:
  void SetUp() override {
    // Register prefs in test pref services.
    policy::URLBlocklistManager::RegisterProfilePrefs(
        fake_profile_prefs_.registry());
    blocklist_service_.emplace(std::make_unique<policy::URLBlocklistManager>(
        &fake_profile_prefs_, policy::policy_prefs::kUrlBlocklist,
        policy::policy_prefs::kUrlAllowlist));

    decorator_.emplace(&blocklist_service_.value());
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  sync_preferences::TestingPrefServiceSyncable fake_profile_prefs_;
  absl::optional<PolicyBlocklistService> blocklist_service_;
  absl::optional<ContentSignalsDecorator> decorator_;
};

TEST_F(ContentSignalsDecoratorTest, Decorate) {
  bool callback_invoked = false;
  base::OnceClosure closure = base::BindLambdaForTesting(
      [&callback_invoked]() { callback_invoked = true; });

  SignalsType signals;
  decorator_->Decorate(signals, std::move(closure));

  EXPECT_TRUE(signals.has_remote_desktop_available());
  EXPECT_TRUE(signals.has_site_isolation_enabled());

  EXPECT_TRUE(callback_invoked);

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

}  // namespace enterprise_connectors
