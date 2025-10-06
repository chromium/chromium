// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_controls/chrome_clipboard_context.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#endif

namespace data_controls {

namespace {

constexpr char kGoogleUrl[] = "https://google.com/";
constexpr char kChromiumUrl[] = "https://chromium.org/";
constexpr char kUserName[] = "test-user@chromium.org";

class DataControlsReportingTest : public testing::Test {
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

#if BUILDFLAG(IS_CHROMEOS)
    network_config_helper_ =
        std::make_unique<ash::network_config::CrosNetworkConfigTestHelper>();
    ash::NetworkHandler::Initialize();
#endif
  }

  void TearDown() override {
    managed_profile_->GetPrefs()->ClearPref(kDataControlsRulesScopePref);
    helper_.reset();

#if BUILDFLAG(IS_CHROMEOS)
    ash::NetworkHandler::Shutdown();
#endif
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

  content::ClipboardEndpoint os_clipboard_endpoint(GURL url,
                                                   bool off_the_record) {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(url, ui::DataTransferEndpointOptions{
                                          .off_the_record = off_the_record,
                                      }));
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
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
#endif
};

}  // namespace

TEST_F(DataControlsReportingTest, NoReportInIncognitoProfile) {
  auto validator = helper_->CreateValidator();
  validator.ExpectNoReport();

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          incognito_managed_profile());
  router->ReportPaste(
      ChromeClipboardContext(
          managed_endpoint(GURL(kGoogleUrl)),
          managed_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::PlainTextType(),
          }),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));
  router->ReportPasteWarningBypassed(
      ChromeClipboardContext(managed_endpoint(GURL(kGoogleUrl)),
                             managed_endpoint(GURL(kChromiumUrl)), {}),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));
  router->ReportCopy(
      ChromeClipboardContext(
          managed_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::PlainTextType(),
          }),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));
  router->ReportCopyWarningBypassed(
      ChromeClipboardContext(managed_endpoint(GURL(kChromiumUrl)), {}),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));

  // This wait call is necessary since all the "Report*" calls trigger async
  // code, so we need to wait a bit so the "validator.ExpectNoReport();" call is
  // properly validated instead of finishing the test too soon.
  base::RunLoop().RunUntilIdle();
}

TEST_F(DataControlsReportingTest, NoReportInUnmanagedProfile) {
  auto validator = helper_->CreateValidator();
  validator.ExpectNoReport();

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          unmanaged_profile_);
  router->ReportPaste(
      ChromeClipboardContext(
          managed_endpoint(GURL(kGoogleUrl)),
          unmanaged_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::PlainTextType(),
          }),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));
  router->ReportPasteWarningBypassed(
      ChromeClipboardContext(managed_endpoint(GURL(kGoogleUrl)),
                             unmanaged_endpoint(GURL(kChromiumUrl)), {}),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));
  router->ReportCopy(
      ChromeClipboardContext(
          unmanaged_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::PlainTextType(),
          }),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));
  router->ReportCopyWarningBypassed(
      ChromeClipboardContext(unmanaged_endpoint(GURL(kChromiumUrl)), {}),
      Verdict::Warn({{{0, true}, {"1", "rule_1_name"}}}));

  // This wait call is necessary since all the "Report*" calls trigger async
  // code, so we need to wait a bit so the "validator.ExpectNoReport();" call is
  // properly validated instead of finishing the test too soon.
  base::RunLoop().RunUntilIdle();
}

TEST_F(DataControlsReportingTest, NoReportWithoutTriggeredRules) {
  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    router->ReportPaste(
        ChromeClipboardContext(
            managed_endpoint(GURL(kGoogleUrl)),
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::PlainTextType(),
            }),
        Verdict::Warn({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    router->ReportPasteWarningBypassed(
        ChromeClipboardContext(
            incognito_managed_endpoint(GURL(kGoogleUrl)),
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::PlainTextType(),
            }),
        Verdict::Block({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    router->ReportPaste(
        ChromeClipboardContext(
            unmanaged_endpoint(GURL(kGoogleUrl)),
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::SvgType(),
            }),
        Verdict::Report({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    router->ReportCopy(
        ChromeClipboardContext(
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::PlainTextType(),
            }),
        Verdict::Warn({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    router->ReportCopyWarningBypassed(
        ChromeClipboardContext(
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::PlainTextType(),
            }),
        Verdict::Block({}));
  }
  {
    auto validator = helper_->CreateValidator();
    validator.ExpectNoReport();
    router->ReportCopy(
        ChromeClipboardContext(
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::SvgType(),
            }),
        Verdict::Report({}));
  }
}

TEST_F(DataControlsReportingTest, PasteInManagedProfile_OSClipboardSource) {
  Verdict::TriggeredRules triggered_rules = {{{0, true}, {"1", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kChromiumUrl,
      /*expected_tab_url=*/kChromiumUrl,
      /*expected_source=*/"CLIPBOARD",
      /*expected_destination=*/kChromiumUrl,
      /*expected_mimetypes=*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*expected_trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/triggered_rules,
      /*expected_result=*/"EVENT_RESULT_WARNED",
      /*expected_profile_username=*/kUserName,
      /*expected_profile_identifier=*/
      managed_profile_->GetPath().AsUTF8Unsafe(),
      /*expected_content_size=*/1234);

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);
  router->ReportPaste(
      ChromeClipboardContext(
          os_clipboard_endpoint(GURL(kGoogleUrl), /*off_the_record=*/false),
          managed_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::PlainTextType(),
          }),
      Verdict::Warn(triggered_rules));
  validator_run_loop.Run();
}

TEST_F(DataControlsReportingTest,
       PasteInManagedProfile_IncognitoOSClipboardSource) {
  Verdict::TriggeredRules triggered_rules = {{{0, true}, {"1", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kChromiumUrl,
      /*expected_tab_url=*/kChromiumUrl,
      /*expected_source=*/"INCOGNITO",
      /*expected_destination=*/kChromiumUrl,
      /*expected_mimetypes=*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*expected_trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/triggered_rules,
      /*expected_result=*/"EVENT_RESULT_WARNED",
      /*expected_profile_username=*/kUserName,
      /*expected_profile_identifier=*/
      managed_profile_->GetPath().AsUTF8Unsafe(),
      /*expected_content_size=*/1234);

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);
  router->ReportPaste(
      ChromeClipboardContext(
          os_clipboard_endpoint(GURL(kGoogleUrl), /*off_the_record=*/true),
          managed_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::PlainTextType(),
          }),
      Verdict::Warn(triggered_rules));
  validator_run_loop.Run();
}

TEST_F(DataControlsReportingTest, PasteInManagedProfile_ManagedSourceProfile) {
  Verdict::TriggeredRules triggered_rules = {{{0, true}, {"1", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);
  router->ReportPaste(
      ChromeClipboardContext(
          managed_endpoint(GURL(kGoogleUrl)),
          managed_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::PlainTextType(),
          }),
      Verdict::Warn(triggered_rules));
  validator_run_loop.Run();
}

TEST_F(DataControlsReportingTest,
       PasteInManagedProfile_IncognitoManagedSourceProfile) {
  Verdict::TriggeredRules triggered_rules = {
      {{0, true}, {"1", "rule_1_name"}},
      {{1, true}, {"2", "rule_2_name"}},
  };
  auto validator = helper_->CreateValidator();
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);
  router->ReportPasteWarningBypassed(
      ChromeClipboardContext(
          incognito_managed_endpoint(GURL(kGoogleUrl)),
          managed_endpoint(GURL(kChromiumUrl)),
          {
              .size = 1234,
              .format_type = ui::ClipboardFormatType::HtmlType(),
          }),
      Verdict::Block(triggered_rules));
  validator_run_loop.Run();
}

TEST_F(DataControlsReportingTest,
       PasteInManagedProfile_UnmanagedSourceProfile) {
  Verdict::TriggeredRules triggered_rules = {{{0, true}, {"1", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);
  router->ReportPaste(ChromeClipboardContext(
                          unmanaged_endpoint(GURL(kGoogleUrl)),
                          managed_endpoint(GURL(kChromiumUrl)),
                          {
                              .size = 1234,
                              .format_type = ui::ClipboardFormatType::SvgType(),
                          }),
                      Verdict::Report(triggered_rules));
  validator_run_loop.Run();
}

TEST_F(DataControlsReportingTest,
       PasteInManagedProfile_UnmanagedSourceProfileOnManagedDevice) {
  // Having Data Controls applied to the whole browser implies even unmanaged
  // profiles are in scope for reporting. This is mocked in this tests by
  // setting the scope pref directly.
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_MACHINE);

  Verdict::TriggeredRules triggered_rules = {{{0, true}, {"1", "rule_1_name"}}};
  auto validator = helper_->CreateValidator();
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);
  router->ReportPaste(ChromeClipboardContext(
                          unmanaged_endpoint(GURL(kGoogleUrl)),
                          managed_endpoint(GURL(kChromiumUrl)),
                          {
                              .size = 1234,
                              .format_type = ui::ClipboardFormatType::RtfType(),
                          }),
                      Verdict::Block(triggered_rules));
  validator_run_loop.Run();
}

TEST_F(DataControlsReportingTest, CopyInManagedProfile) {
  Verdict::TriggeredRules triggered_rules = {{{0, true}, {"1", "rule_1_name"}}};
  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          managed_profile_);

  {
    auto validator = helper_->CreateValidator();
    base::RunLoop validator_run_loop;
    validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

    router->ReportCopy(
        ChromeClipboardContext(
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::PlainTextType(),
            }),
        Verdict::Warn(triggered_rules));
    validator_run_loop.Run();
  }
  {
    auto validator = helper_->CreateValidator();
    base::RunLoop validator_run_loop;
    validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

    router->ReportCopyWarningBypassed(
        ChromeClipboardContext(
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::PngType(),
            }),
        Verdict::Block(triggered_rules));
    validator_run_loop.Run();
  }
  {
    auto validator = helper_->CreateValidator();
    base::RunLoop validator_run_loop;
    validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

    router->ReportCopy(
        ChromeClipboardContext(
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::SvgType(),
            }),
        Verdict::Block(triggered_rules));
    validator_run_loop.Run();
  }
  {
    auto validator = helper_->CreateValidator();
    base::RunLoop validator_run_loop;
    validator.SetDoneClosure(validator_run_loop.QuitClosure());
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

    router->ReportCopy(
        ChromeClipboardContext(
            managed_endpoint(GURL(kChromiumUrl)),
            {
                .size = 1234,
                .format_type = ui::ClipboardFormatType::RtfType(),
            }),
        Verdict::Report(triggered_rules));
    validator_run_loop.Run();
  }
}

TEST_F(DataControlsReportingTest, GetClipboardSource_SameProfile) {
  auto same_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/managed_endpoint(GURL(kGoogleUrl)),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      same_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::SAME_PROFILE);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          same_copy_source),
      "https://google.com/");
}

TEST_F(DataControlsReportingTest, GetClipboardSource_Incognito) {
  auto incognito_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/incognito_managed_endpoint(GURL(kGoogleUrl)),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      incognito_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::INCOGNITO);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          incognito_copy_source),
      "INCOGNITO");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_OSClipboardOnManagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_MACHINE);
  auto os_clipboard_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/os_clipboard_endpoint(GURL(kGoogleUrl),
                                       /*off_the_record=*/false),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      os_clipboard_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::CLIPBOARD);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          os_clipboard_copy_source),
      "https://google.com/");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_IncognitoOSClipboardOnManagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_MACHINE);
  auto os_clipboard_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/os_clipboard_endpoint(GURL(kGoogleUrl),
                                       /*off_the_record=*/true),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      os_clipboard_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::INCOGNITO);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          os_clipboard_copy_source),
      "INCOGNITO");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_UnmanagedProfileOnManagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_MACHINE);
  auto unmanaged_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/unmanaged_endpoint(GURL(kGoogleUrl)),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      unmanaged_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::OTHER_PROFILE);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          unmanaged_copy_source),
      "https://google.com/");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_GuestProfileOnManagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_MACHINE);
  auto guest_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/guest_endpoint(GURL(kGoogleUrl)),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      guest_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::OTHER_PROFILE);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          guest_copy_source),
      "https://google.com/");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_OSClipboardOnUnmanagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_USER);
  auto os_clipboard_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/os_clipboard_endpoint(GURL(kGoogleUrl),
                                       /*off_the_record=*/false),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      os_clipboard_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::CLIPBOARD);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          os_clipboard_copy_source),
      "CLIPBOARD");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_IncognitoOSClipboardOnUnmanagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_USER);
  auto os_clipboard_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/os_clipboard_endpoint(GURL(kGoogleUrl),
                                       /*off_the_record=*/true),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      os_clipboard_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::INCOGNITO);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          os_clipboard_copy_source),
      "INCOGNITO");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_UnmanagedProfileOnUnmanagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_USER);
  auto unmanaged_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/unmanaged_endpoint(GURL(kGoogleUrl)),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      unmanaged_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::OTHER_PROFILE);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          unmanaged_copy_source),
      "OTHER_PROFILE");
}

TEST_F(DataControlsReportingTest,
       GetClipboardSource_GuestProfileOnUnmanagedBrowser) {
  managed_profile_->GetPrefs()->SetInteger(kDataControlsRulesScopePref,
                                           policy::POLICY_SCOPE_USER);
  auto guest_copy_source = ChromeClipboardContext::GetClipboardSource(
      /*source=*/guest_endpoint(GURL(kGoogleUrl)),
      /*destination=*/managed_endpoint(GURL(kChromiumUrl)),
      kDataControlsRulesScopePref);
  ASSERT_EQ(
      guest_copy_source.context(),
      enterprise_connectors::ContentMetaData::CopiedTextSource::OTHER_PROFILE);
  ASSERT_EQ(
      enterprise_connectors::ReportingEventRouter ::GetClipboardSourceString(
          guest_copy_source),
      "OTHER_PROFILE");
}

}  // namespace data_controls
