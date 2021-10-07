// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"

#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "chrome/common/pref_names.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/system/fake_statistics_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
#include "components/component_updater/pref_names.h"
#endif  // defined(OS_WIN)

namespace enterprise_connectors {

class CommonSignalsDecoratorTest : public testing::Test {
 protected:
  void SetUp() override {
    // Register prefs in test pref services.
    policy::URLBlocklistManager::RegisterProfilePrefs(
        fake_profile_prefs_.registry());
    safe_browsing::RegisterProfilePrefs(fake_profile_prefs_.registry());
    fake_local_state_.registry()->RegisterBooleanPref(
        prefs::kBuiltInDnsClientEnabled, false);
#if defined(OS_WIN)
    fake_local_state_.registry()->RegisterBooleanPref(prefs::kSwReporterEnabled,
                                                      false);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    fake_local_state_.registry()->RegisterBooleanPref(
        prefs::kThirdPartyBlockingEnabled, false);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

    blocklist_service_.emplace(
        std::make_unique<policy::URLBlocklistManager>(&fake_profile_prefs_));

    decorator_.emplace(&fake_local_state_, &fake_profile_prefs_,
                       &blocklist_service_.value());
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple fake_local_state_;
  sync_preferences::TestingPrefServiceSyncable fake_profile_prefs_;
  absl::optional<PolicyBlocklistService> blocklist_service_;
  absl::optional<CommonSignalsDecorator> decorator_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_F(CommonSignalsDecoratorTest, Decorate_StaticValuesPresent) {
  fake_profile_prefs_.SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                 safe_browsing::PASSWORD_REUSE);

  bool done_called = false;
  base::OnceClosure done_closure =
      base::BindLambdaForTesting([&done_called]() { done_called = true; });

  SignalsType signals;
  decorator_->Decorate(signals, std::move(done_closure));

  EXPECT_TRUE(signals.has_os());
  EXPECT_TRUE(signals.has_os_version());
  EXPECT_TRUE(signals.has_device_model());
  EXPECT_TRUE(signals.has_device_manufacturer());
  EXPECT_TRUE(signals.has_display_name());
  EXPECT_TRUE(signals.has_browser_version());
  EXPECT_TRUE(signals.has_built_in_dns_client_enabled());
  EXPECT_TRUE(signals.has_safe_browsing_protection_level());
  EXPECT_TRUE(signals.has_remote_desktop_available());
  EXPECT_TRUE(signals.has_password_protection_warning_trigger());

  EXPECT_TRUE(done_called);

#if defined(OS_WIN)
  EXPECT_TRUE(signals.has_chrome_cleanup_enabled());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(signals.has_third_party_blocking_enabled());
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_FALSE(signals.has_third_party_blocking_enabled());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else   // defined(OS_WIN)
  EXPECT_FALSE(signals.has_chrome_cleanup_enabled());
#endif  // defined(OS_WIN)
}

}  // namespace enterprise_connectors
