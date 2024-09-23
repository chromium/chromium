// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/search/ntp_features.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

class NewTabPageUtilBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Disable the field trial testing config as the tests in this file care
    // about whether features are overridden or not.
    cmd->AppendSwitch(variations::switches::kDisableFieldTrialTestingConfig);
    cmd->AppendSwitch(optimization_guide::switches::kDebugLoggingEnabled);
  }

  OptimizationGuideKeyedService* GetOptimizationGuideKeyedService() {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile());
  }

  void CheckInternalsLog(std::string_view message) {
    auto* logger =
        GetOptimizationGuideKeyedService()->GetOptimizationGuideLogger();
    EXPECT_THAT(logger->recent_log_messages_,
                testing::Contains(testing::Field(
                    &OptimizationGuideLogger::LogMessage::message,
                    testing::HasSubstr(message))));
  }

 protected:
  base::test::ScopedFeatureList features_;
};

class NewTabPageUtilEnableFlagBrowserTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilEnableFlagBrowserTest() {
    features_.InitWithFeatures(
        {ntp_features::kNtpChromeCartModule, ntp_features::kNtpDriveModule,
         ntp_features::kNtpCalendarModule,
         ntp_features::kNtpOutlookCalendarModule},
        {});
  }
};

class NewTabPageUtilDisableFlagBrowserTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilDisableFlagBrowserTest() {
    features_.InitWithFeatures(
        {}, {ntp_features::kNtpChromeCartModule, ntp_features::kNtpDriveModule,
             ntp_features::kNtpCalendarModule,
             ntp_features::kNtpOutlookCalendarModule});
  }
};

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, EnableCartByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(IsCartModuleEnabled());
#else
  EXPECT_FALSE(IsCartModuleEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, DisableCartByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest, EnableCartByFlag) {
  EXPECT_TRUE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest,
                       DisableCartByFlag) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, EnableDriveByToT) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(IsDriveModuleEnabled());
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    " enabled: default feature flag value");
#else
  EXPECT_FALSE(IsDriveModuleEnabled());
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    " disabled: default feature flag value");
#endif
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest, EnableDriveByFlag) {
  EXPECT_TRUE(IsDriveModuleEnabled());
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    " enabled: feature flag forced on");
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest,
                       DisableDriveByFlag) {
  EXPECT_FALSE(IsDriveModuleEnabled());
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    " disabled: feature flag forced off");
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest,
                       EnableGoogleCalendarByFlag) {
  EXPECT_TRUE(IsGoogleCalendarModuleEnabled(true));
  CheckInternalsLog(std::string(ntp_features::kNtpCalendarModule.name) +
                    " enabled: feature flag forced on");
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest,
                       DisableGoogleCalendarByFlag) {
  EXPECT_FALSE(IsGoogleCalendarModuleEnabled(true));
  CheckInternalsLog(std::string(ntp_features::kNtpCalendarModule.name) +
                    " disabled: feature flag forced off");
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest,
                       GoogleCalendarIsNotManaged) {
  EXPECT_FALSE(IsGoogleCalendarModuleEnabled(false));
  CheckInternalsLog(std::string(ntp_features::kNtpCalendarModule.name) +
                    " disabled: account not managed");
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest,
                       EnableOutlookCalendarByFlag) {
  EXPECT_TRUE(IsOutlookCalendarModuleEnabled(true));
  CheckInternalsLog(std::string(ntp_features::kNtpOutlookCalendarModule.name) +
                    " enabled: feature flag forced on");
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest,
                       DisableOutlookCalendarByFlag) {
  EXPECT_FALSE(IsOutlookCalendarModuleEnabled(true));
  CheckInternalsLog(std::string(ntp_features::kNtpOutlookCalendarModule.name) +
                    " disabled: feature flag forced off");
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest,
                       OutlookCalendarIsNotManaged) {
  EXPECT_FALSE(IsOutlookCalendarModuleEnabled(false));
  CheckInternalsLog(std::string(ntp_features::kNtpOutlookCalendarModule.name) +
                    " disabled: account not managed");
}
