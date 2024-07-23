// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/reporting_service.h"

#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

constexpr char kGoogleUrl[] = "https://google.com/";
constexpr char kChromiumUrl[] = "https://chromium.org/";
constexpr char kUserName[] = "test-user@chromium.org";

class DataControlsReportingServiceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());
    managed_profile_ = profile_manager_->CreateTestingProfile("managed");
    unmanaged_profile_ = profile_manager_->CreateTestingProfile("unmanaged");
    guest_profile_ = profile_manager_->CreateGuestProfile();

    helper_ = std::make_unique<
        enterprise_connectors::test::EventReportValidatorHelper>(
        managed_profile_);
  }

  Profile* incognito_managed_profile() {
    return managed_profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
  Profile* incognito_unmanaged_profile() {
    return unmanaged_profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  content::WebContents* CreateContentsIfNull(
      std::unique_ptr<content::WebContents>& contents,
      Profile* profile) {
    if (!contents) {
      content::WebContents::CreateParams params(profile);
      contents = content::WebContents::Create(params);
    }
    return contents.get();
  }

  content::WebContents* managed_contents() {
    return CreateContentsIfNull(managed_contents_, managed_profile_);
  }
  content::WebContents* unmanaged_contents() {
    return CreateContentsIfNull(unmanaged_contents_, unmanaged_profile_);
  }
  content::WebContents* incognito_managed_contents() {
    return CreateContentsIfNull(incognito_managed_contents_,
                                incognito_managed_profile());
  }
  content::WebContents* incognito_unmanaged_contents() {
    return CreateContentsIfNull(incognito_unmanaged_contents_,
                                incognito_unmanaged_profile());
  }
  content::WebContents* guest_contents() {
    return CreateContentsIfNull(guest_contents_, guest_profile_);
  }

  content::ClipboardEndpoint managed_endpoint(GURL url) {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(url),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(managed_profile_);
        }),
        *managed_contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint incognito_managed_endpoint(GURL url) {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(url),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(
              incognito_managed_profile());
        }),
        *incognito_managed_contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint unmanaged_endpoint(GURL url) {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(url),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(unmanaged_profile_);
        }),
        *unmanaged_contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint incognito_unmanaged_endpoint(GURL url) {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(url),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(
              incognito_unmanaged_profile());
        }),
        *incognito_unmanaged_contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint guest_endpoint(GURL url) {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(url),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(guest_profile_);
        }),
        *guest_contents()->GetPrimaryMainFrame());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> managed_profile_ = nullptr;
  raw_ptr<TestingProfile> unmanaged_profile_ = nullptr;
  raw_ptr<TestingProfile> guest_profile_ = nullptr;
  std::unique_ptr<content::WebContents> managed_contents_;
  std::unique_ptr<content::WebContents> unmanaged_contents_;
  std::unique_ptr<content::WebContents> incognito_managed_contents_;
  std::unique_ptr<content::WebContents> incognito_unmanaged_contents_;
  std::unique_ptr<content::WebContents> guest_contents_;
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      helper_;
};

}  // namespace

TEST_F(DataControlsReportingServiceTest, NoServiceInIncognito) {
  ASSERT_FALSE(ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      incognito_managed_profile()));
  ASSERT_FALSE(ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      incognito_unmanaged_profile()));
}

TEST_F(DataControlsReportingServiceTest, NoReportInUnmanagedProfile) {
  auto validator = helper_->CreateValidator();
  validator.ExpectNoReport();

  auto* service = ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      unmanaged_profile_);
  service->ReportPaste(
      managed_endpoint(GURL(kGoogleUrl)),
      unmanaged_endpoint(GURL(kChromiumUrl)),
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      Verdict::Warn({{0, {"rule_1_id", "rule_1_name"}}}));
  service->ReportPasteWarningBypassed(
      managed_endpoint(GURL(kGoogleUrl)),
      unmanaged_endpoint(GURL(kChromiumUrl)), {},
      Verdict::Warn({{0, {"rule_1_id", "rule_1_name"}}}));
  service->ReportCopy(
      unmanaged_endpoint(GURL(kChromiumUrl)),
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      Verdict::Warn({{0, {"rule_1_id", "rule_1_name"}}}));
  service->ReportCopyWarningBypassed(
      unmanaged_endpoint(GURL(kChromiumUrl)), {},
      Verdict::Warn({{0, {"rule_1_id", "rule_1_name"}}}));
}

TEST_F(DataControlsReportingServiceTest, NoReportWithoutTriggeredRules) {
  auto* service = ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      managed_profile_);
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    service->ReportPaste(
        managed_endpoint(GURL(kGoogleUrl)),
        managed_endpoint(GURL(kChromiumUrl)),
        {
            .size = 1234,
            .format_type = ui::ClipboardFormatType::PlainTextType(),
        },
        Verdict::Warn({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    service->ReportPasteWarningBypassed(
        incognito_managed_endpoint(GURL(kGoogleUrl)),
        managed_endpoint(GURL(kChromiumUrl)),
        {
            .size = 1234,
            .format_type = ui::ClipboardFormatType::PlainTextType(),
        },
        Verdict::Block({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    service->ReportPaste(unmanaged_endpoint(GURL(kGoogleUrl)),
                         managed_endpoint(GURL(kChromiumUrl)),
                         {
                             .size = 1234,
                             .format_type = ui::ClipboardFormatType::SvgType(),
                         },
                         Verdict::Report({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    service->ReportCopy(
        managed_endpoint(GURL(kChromiumUrl)),
        {
            .size = 1234,
            .format_type = ui::ClipboardFormatType::PlainTextType(),
        },
        Verdict::Warn({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    service->ReportCopyWarningBypassed(
        managed_endpoint(GURL(kChromiumUrl)),
        {
            .size = 1234,
            .format_type = ui::ClipboardFormatType::PlainTextType(),
        },
        Verdict::Block({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    service->ReportCopy(managed_endpoint(GURL(kChromiumUrl)),
                        {
                            .size = 1234,
                            .format_type = ui::ClipboardFormatType::SvgType(),
                        },
                        Verdict::Report({}));
  }
}

TEST_F(DataControlsReportingServiceTest,
       PasteInManagedProfile_ManagedSourceProfile) {
  Verdict::TriggeredRules triggered_rules = {{0, {"rule_1_id", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kChromiumUrl,
      /*expected_tab_url=*/kChromiumUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/kChromiumUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/triggered_rules,
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  auto* service = ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      managed_profile_);
  service->ReportPaste(
      managed_endpoint(GURL(kGoogleUrl)), managed_endpoint(GURL(kChromiumUrl)),
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      Verdict::Warn(triggered_rules));
}

TEST_F(DataControlsReportingServiceTest,
       PasteInManagedProfile_IncognitoManagedSourceProfile) {
  Verdict::TriggeredRules triggered_rules = {
      {0, {"rule_1_id", "rule_1_name"}},
      {1, {"rule_2_id", "rule_2_name"}},
  };
  auto validator = helper_->CreateValidator();
  validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kChromiumUrl,
      /*expected_tab_url=*/kChromiumUrl,
      /*source=*/"INCOGNITO",
      /*destination=*/kChromiumUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/html"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/triggered_rules,
      /*event_result=*/"EVENT_RESULT_BYPASSED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  auto* service = ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      managed_profile_);
  service->ReportPasteWarningBypassed(
      incognito_managed_endpoint(GURL(kGoogleUrl)),
      managed_endpoint(GURL(kChromiumUrl)),
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      Verdict::Block(triggered_rules));
}

TEST_F(DataControlsReportingServiceTest,
       PasteInManagedProfile_UnmanagedSourceProfile) {
  Verdict::TriggeredRules triggered_rules = {{0, {"rule_1_id", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kChromiumUrl,
      /*expected_tab_url=*/kChromiumUrl,
      /*source=*/"OTHER_PROFILE",
      /*destination=*/kChromiumUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/svg+xml"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/triggered_rules,
      /*event_result=*/"EVENT_RESULT_ALLOWED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  auto* service = ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      managed_profile_);
  service->ReportPaste(unmanaged_endpoint(GURL(kGoogleUrl)),
                       managed_endpoint(GURL(kChromiumUrl)),
                       {
                           .size = 1234,
                           .format_type = ui::ClipboardFormatType::SvgType(),
                       },
                       Verdict::Report(triggered_rules));
}

TEST_F(DataControlsReportingServiceTest,
       PasteInManagedProfile_UnmanagedSourceProfileOnManagedDevice) {
  // Having Data Controls applied to the whole browser implies even unmanaged
  // profiles are in scope for reporting. This is mocked in this tests by
  // setting the scope pref directly.
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_MACHINE);

  Verdict::TriggeredRules triggered_rules = {{0, {"rule_1_id", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kChromiumUrl,
      /*expected_tab_url=*/kChromiumUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/kChromiumUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/rtf"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/triggered_rules,
      /*event_result=*/"EVENT_RESULT_BLOCKED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  auto* service = ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      managed_profile_);
  service->ReportPaste(unmanaged_endpoint(GURL(kGoogleUrl)),
                       managed_endpoint(GURL(kChromiumUrl)),
                       {
                           .size = 1234,
                           .format_type = ui::ClipboardFormatType::RtfType(),
                       },
                       Verdict::Block(triggered_rules));
}

TEST_F(DataControlsReportingServiceTest, CopyInManagedProfile) {
  Verdict::TriggeredRules triggered_rules = {{0, {"rule_1_id", "rule_1_name"}}};
  auto* service = ReportingServiceFactory::GetInstance()->GetForBrowserContext(
      managed_profile_);

  {
    auto validator = helper_->CreateValidator();
    validator.ExpectDataControlsSensitiveDataEvent(
        /*expected_url=*/
        kChromiumUrl,
        /*expected_tab_url=*/kChromiumUrl,
        /*source=*/kChromiumUrl,
        /*destination=*/"",
        /*mime_types=*/
        []() {
          static std::set<std::string> set = {"text/plain"};
          return &set;
        }(),
        /*trigger=*/"CLIPBOARD_COPY",
        /*triggered_rules=*/triggered_rules,
        /*event_result=*/"EVENT_RESULT_WARNED",
        /*profile_username=*/kUserName,
        /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
        /*content_size=*/1234);

    service->ReportCopy(
        managed_endpoint(GURL(kChromiumUrl)),
        {
            .size = 1234,
            .format_type = ui::ClipboardFormatType::PlainTextType(),
        },
        Verdict::Warn(triggered_rules));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectDataControlsSensitiveDataEvent(
        /*expected_url=*/
        kChromiumUrl,
        /*expected_tab_url=*/kChromiumUrl,
        /*source=*/kChromiumUrl,
        /*destination=*/"",
        /*mime_types=*/
        []() {
          static std::set<std::string> set = {"image/png"};
          return &set;
        }(),
        /*trigger=*/"CLIPBOARD_COPY",
        /*triggered_rules=*/triggered_rules,
        /*event_result=*/"EVENT_RESULT_BYPASSED",
        /*profile_username=*/kUserName,
        /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
        /*content_size=*/1234);

    service->ReportCopyWarningBypassed(
        managed_endpoint(GURL(kChromiumUrl)),
        {
            .size = 1234,
            .format_type = ui::ClipboardFormatType::PngType(),
        },
        Verdict::Block(triggered_rules));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectDataControlsSensitiveDataEvent(
        /*expected_url=*/
        kChromiumUrl,
        /*expected_tab_url=*/kChromiumUrl,
        /*source=*/kChromiumUrl,
        /*destination=*/"",
        /*mime_types=*/
        []() {
          static std::set<std::string> set = {"image/svg+xml"};
          return &set;
        }(),
        /*trigger=*/"CLIPBOARD_COPY",
        /*triggered_rules=*/triggered_rules,
        /*event_result=*/"EVENT_RESULT_BLOCKED",
        /*profile_username=*/kUserName,
        /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
        /*content_size=*/1234);

    service->ReportCopy(managed_endpoint(GURL(kChromiumUrl)),
                        {
                            .size = 1234,
                            .format_type = ui::ClipboardFormatType::SvgType(),
                        },
                        Verdict::Block(triggered_rules));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectDataControlsSensitiveDataEvent(
        /*expected_url=*/
        kChromiumUrl,
        /*expected_tab_url=*/kChromiumUrl,
        /*source=*/kChromiumUrl,
        /*destination=*/"",
        /*mime_types=*/
        []() {
          static std::set<std::string> set = {"text/rtf"};
          return &set;
        }(),
        /*trigger=*/"CLIPBOARD_COPY",
        /*triggered_rules=*/triggered_rules,
        /*event_result=*/"EVENT_RESULT_ALLOWED",
        /*profile_username=*/kUserName,
        /*profile_identifier=*/managed_profile_->GetPath().AsUTF8Unsafe(),
        /*content_size=*/1234);

    service->ReportCopy(managed_endpoint(GURL(kChromiumUrl)),
                        {
                            .size = 1234,
                            .format_type = ui::ClipboardFormatType::RtfType(),
                        },
                        Verdict::Report(triggered_rules));
  }
}

TEST_F(DataControlsReportingServiceTest, GetClipboardSourceString) {
  ASSERT_EQ(ReportingService::GetClipboardSourceString(
                /*source=*/managed_endpoint(GURL(kGoogleUrl)),
                /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
                kDataControlsRulesScopePref),
            "https://google.com/");
  ASSERT_EQ(ReportingService::GetClipboardSourceString(
                /*source=*/incognito_managed_endpoint(GURL(kGoogleUrl)),
                /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
                kDataControlsRulesScopePref),
            "INCOGNITO");

  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_MACHINE);
  ASSERT_EQ(ReportingService::GetClipboardSourceString(
                /*source=*/unmanaged_endpoint(GURL(kGoogleUrl)),
                /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
                kDataControlsRulesScopePref),
            "https://google.com/");
  ASSERT_EQ(ReportingService::GetClipboardSourceString(
                /*source=*/guest_endpoint(GURL(kGoogleUrl)),
                /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
                kDataControlsRulesScopePref),
            "https://google.com/");

  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_USER);
  ASSERT_EQ(ReportingService::GetClipboardSourceString(
                /*source=*/unmanaged_endpoint(GURL(kGoogleUrl)),
                /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
                kDataControlsRulesScopePref),
            "OTHER_PROFILE");
  ASSERT_EQ(ReportingService::GetClipboardSourceString(
                /*source=*/guest_endpoint(GURL(kGoogleUrl)),
                /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
                kDataControlsRulesScopePref),
            "OTHER_PROFILE");
}

}  // namespace data_controls
