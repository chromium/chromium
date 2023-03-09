// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "components/feedback/content/feedback_uploader_factory.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "extensions/browser/api/feedback_private/mock_feedback_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::ash::os_feedback_ui::mojom::FeedbackContext;
using ::ash::os_feedback_ui::mojom::FeedbackContextPtr;
using ::ash::os_feedback_ui::mojom::Report;
using ::ash::os_feedback_ui::mojom::ReportPtr;
using ::ash::os_feedback_ui::mojom::SendReportStatus;
using extensions::FeedbackParams;
using extensions::FeedbackPrivateDelegate;
using feedback::FeedbackData;
using testing::_;

constexpr char kExtraDiagnosticsKey[] = "EXTRA_DIAGNOSTICS";
constexpr char kFakeExtraDiagnosticsValue[] =
    "Failed to connect to wifi network.";
constexpr char kFakeCategoryTag[] = "FakeCategoryTag";
constexpr char kPageUrl[] = "https://www.google.com/?q=123";
constexpr char kSignedInUserEmail[] = "test_user_email@gmail.com";
constexpr char kFeedbackUserConsentKey[] = "feedbackUserCtlConsent";
constexpr char kFeedbackUserConsentGrantedValue[] = "true";
constexpr char kFeedbackUserConsentDeniedValue[] = "false";
constexpr char kFeedbackCategoryTag[] = "BluetoothReportWithLogs";
const std::u16string kDescription = u"This is a fake description";
constexpr int kPerformanceTraceId = 1;

constexpr char kFakeKey[] = "fake key";
constexpr char kFakeValue[] = "fake value";
constexpr char kTabTitleValue[] = "some sensitive info";

class FakeFeedbackPrivateDelegate : public FeedbackPrivateDelegate {
 public:
  explicit FakeFeedbackPrivateDelegate(
      base::RepeatingCallback<void(bool)> callback)
      : on_fetch_completed_(std::move(callback)) {}

  FakeFeedbackPrivateDelegate(const FakeFeedbackPrivateDelegate&) = delete;
  FakeFeedbackPrivateDelegate& operator=(const FakeFeedbackPrivateDelegate&) =
      delete;

  ~FakeFeedbackPrivateDelegate() override = default;

  // FeedbackPrivateDelegate:
  base::Value::Dict GetStrings(content::BrowserContext* browser_context,
                               bool from_crash) const override;
  void FetchSystemInformation(
      content::BrowserContext* context,
      system_logs::SysLogsFetcherCallback callback) const override;
  std::unique_ptr<system_logs::SystemLogsSource> CreateSingleLogSource(
      extensions::api::feedback_private::LogSource source_type) const override;
  void FetchExtraLogs(
      scoped_refptr<feedback::FeedbackData> feedback_data,
      extensions::FetchExtraLogsCallback callback) const override;
  extensions::api::feedback_private::LandingPageType GetLandingPageType(
      const feedback::FeedbackData& feedback_data) const override;
  void GetLacrosHistograms(GetHistogramsCallback callback) override;
  std::string GetSignedInUserEmail(
      content::BrowserContext* context) const override;
  void NotifyFeedbackDelayed() const override;
  feedback::FeedbackUploader* GetFeedbackUploaderForContext(
      content::BrowserContext* context) const override;
  void OpenFeedback(
      content::BrowserContext* context,
      extensions::api::feedback_private::FeedbackSource source) const override;

 private:
  base::RepeatingCallback<void(bool)> on_fetch_completed_;
};

base::Value::Dict FakeFeedbackPrivateDelegate::GetStrings(
    content::BrowserContext* browser_context,
    bool from_crash) const {
  NOTIMPLEMENTED();
  return {};
}

void FakeFeedbackPrivateDelegate::FetchSystemInformation(
    content::BrowserContext* context,
    system_logs::SysLogsFetcherCallback callback) const {
  auto sys_info = std::make_unique<system_logs::SystemLogsResponse>();
  sys_info->emplace(kFakeKey, kFakeValue);
  sys_info->emplace(feedback::FeedbackReport::kMemUsageWithTabTitlesKey,
                    kTabTitleValue);
  std::move(callback).Run(std::move(sys_info));

  if (on_fetch_completed_) {
    on_fetch_completed_.Run(true);
  }
}

std::unique_ptr<system_logs::SystemLogsSource>
FakeFeedbackPrivateDelegate::CreateSingleLogSource(
    extensions::api::feedback_private::LogSource source_type) const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeFeedbackPrivateDelegate::FetchExtraLogs(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    extensions::FetchExtraLogsCallback callback) const {
  std::move(callback).Run(feedback_data);
}

extensions::api::feedback_private::LandingPageType
FakeFeedbackPrivateDelegate::GetLandingPageType(
    const feedback::FeedbackData& feedback_data) const {
  return extensions::api::feedback_private::LANDING_PAGE_TYPE_NOLANDINGPAGE;
}

void FakeFeedbackPrivateDelegate::GetLacrosHistograms(
    GetHistogramsCallback callback) {
  std::move(callback).Run(std::string());
}

std::string FakeFeedbackPrivateDelegate::GetSignedInUserEmail(
    content::BrowserContext* context) const {
  return std::string();
}

void FakeFeedbackPrivateDelegate::NotifyFeedbackDelayed() const {}

void FakeFeedbackPrivateDelegate::OpenFeedback(
    content::BrowserContext* context,
    extensions::api::feedback_private::FeedbackSource source) const {}

feedback::FeedbackUploader*
FakeFeedbackPrivateDelegate::GetFeedbackUploaderForContext(
    content::BrowserContext* context) const {
  return feedback::FeedbackUploaderFactory::GetForBrowserContext(context);
}

}  // namespace

class ChromeOsFeedbackDelegateTest : public InProcessBrowserTest {
 public:
  ChromeOsFeedbackDelegateTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kOsFeedback);
  }

  ~ChromeOsFeedbackDelegateTest() override = default;

  absl::optional<GURL> GetLastActivePageUrl() {
    ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
    return feedback_delegate_.GetLastActivePageUrl();
  }

 protected:
  void RunSendReport(ReportPtr report,
                     const FeedbackParams& expected_params,
                     scoped_refptr<FeedbackData>& actual_feedback_data,
                     bool preload_system_logs = false) {
    // Will be called when preloading system logs is done.
    base::test::RepeatingTestFuture<bool> fetch_future;
    auto* profile_ = browser()->profile();
    auto mock_private_delegate = std::make_unique<FakeFeedbackPrivateDelegate>(
        fetch_future.GetCallback());
    auto mock_feedback_service =
        base::MakeRefCounted<extensions::MockFeedbackService>(
            profile_, mock_private_delegate.get());
    EXPECT_EQ(mock_private_delegate.get(),
              mock_feedback_service->GetFeedbackPrivateDelegate());

    EXPECT_CALL(*mock_feedback_service, SendFeedback(_, _, _))
        .WillOnce([&](const extensions::FeedbackParams& params,
                      scoped_refptr<FeedbackData> feedback_data,
                      extensions::SendFeedbackCallback callback) {
          // Pass the feedback data out to verify its properties
          actual_feedback_data = feedback_data;

          // Verify that the flags in params are set correctly
          EXPECT_EQ(expected_params.is_internal_email,
                    params.is_internal_email);
          EXPECT_EQ(expected_params.load_system_info, params.load_system_info);
          EXPECT_EQ(expected_params.send_tab_titles, params.send_tab_titles);
          EXPECT_EQ(expected_params.send_histograms, params.send_histograms);
          EXPECT_EQ(expected_params.send_bluetooth_logs,
                    params.send_bluetooth_logs);

          std::move(callback).Run(true);
        });

    auto feedback_delegate_ = std::make_unique<ChromeOsFeedbackDelegate>(
        profile_, std::move(mock_feedback_service));

    if (preload_system_logs) {
      // Trigger preloading.
      feedback_delegate_->GetLastActivePageUrl();
      // Wait for preloading is completed.
      EXPECT_TRUE(fetch_future.Take());
    }

    OsFeedbackScreenshotManager::GetInstance()->SetPngDataForTesting(
        CreateFakePngData());

    base::test::TestFuture<SendReportStatus> future;
    feedback_delegate_->SendReport(std::move(report), future.GetCallback());

    EXPECT_EQ(SendReportStatus::kSuccess, future.Get());
  }

  Browser* LaunchFeedbackAppAndGetBrowser() {
    // Install system apps, namely the Feedback App.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();

    GURL feedback_url_ = GURL(ash::kChromeUIOSFeedbackUrl);

    // Initialize NavigationObserver to start watching for navigation events.
    // NavigationObserver is necessary to avoid crash on opening dialog,
    // because we need to wait for the Feedback app to finish launching
    // before opening the metrics dialog.
    content::TestNavigationObserver navigation_observer(feedback_url_);
    navigation_observer.StartWatchingNewWebContents();

    // Launch the feedback app.
    ui_test_utils::SendToOmniboxAndSubmit(browser(), feedback_url_.spec());

    // Wait for the Feedback app to launch.
    navigation_observer.Wait();

    Browser* feedback_browser = ash::FindSystemWebAppBrowser(
        browser()->profile(), ash::SystemWebAppType::OS_FEEDBACK);

    EXPECT_NE(feedback_browser, nullptr);

    return feedback_browser;
  }

  scoped_refptr<base::RefCountedMemory> CreateFakePngData() {
    const unsigned char kData[] = {12, 11, 99};
    return base::MakeRefCounted<base::RefCountedBytes>(kData, std::size(kData));
  }

  // Find the url of the active tab of the browser if any.
  GURL FindActiveUrl(Browser* browser) {
    if (browser) {
      return browser->tab_strip_model()->GetActiveWebContents()->GetURL();
    }
    return GURL();
  }

  GURL diagnostics_url_ = GURL(ash::kChromeUIDiagnosticsAppUrl);
  GURL explore_url_ = GURL(ash::kChromeUIHelpAppURL);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test GetApplicationLocale returns a valid locale.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetApplicationLocale) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  EXPECT_EQ(feedback_delegate_.GetApplicationLocale(), "en-US");
}

// Test GetLastActivePageUrl returns last active page url if any.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetLastActivePageUrl) {
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(GetLastActivePageUrl()->spec(), "about:blank");

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kPageUrl)));
  EXPECT_EQ(GetLastActivePageUrl()->spec(), kPageUrl);
}

// Test GetSignedInUserEmail returns primary account of signed in user if any..
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetSignedInUserEmail) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(identity_manager);

  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  EXPECT_EQ(feedback_delegate_.GetSignedInUserEmail(), "");

  signin::MakePrimaryAccountAvailable(identity_manager, kSignedInUserEmail,
                                      signin::ConsentLevel::kSignin);
  EXPECT_EQ(feedback_delegate_.GetSignedInUserEmail(), kSignedInUserEmail);
}

// Test GetPerformanceTraceId returns id for performance trace data if any.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetPerformanceTraceId) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  EXPECT_EQ(feedback_delegate_.GetPerformanceTraceId(), 0);
  std::unique_ptr<ContentTracingManager> tracing_manager =
      ContentTracingManager::Create();
  EXPECT_EQ(feedback_delegate_.GetPerformanceTraceId(), 1);
}

// Test that feedback params and data are populated with correct data before
// passed to SendFeedback method of the feedback service.
// - System logs and histograms are included.
// - Screenshot is included so tab titles will be sent too.
// - Consent granted.
// - Non-empty extra_diagnostics provided.
// - sentBluetoothLog flag is set true.
// - category_tag is set to "BluetoothReportWithLogs".
// - User is logged in with internal google account.
// - Performance trace id is present.
// - from_assistant flag is set true.
// - Assistant debug info is allowed.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest,
                       FeedbackDataPopulatedIncludeSysLogsAndScreenshot) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  report->description = kDescription;
  report->include_screenshot = true;
  report->contact_user_consent_granted = true;
  report->feedback_context->extra_diagnostics = kFakeExtraDiagnosticsValue;
  report->send_bluetooth_logs = true;
  report->feedback_context->category_tag = kFeedbackCategoryTag;
  report->include_system_logs_and_histograms = true;
  report->feedback_context->is_internal_account = true;
  report->feedback_context->trace_id = kPerformanceTraceId;
  report->feedback_context->from_assistant = true;
  report->feedback_context->assistant_debug_info_allowed = true;
  const FeedbackParams expected_params{/*is_internal_email=*/true,
                                       /*load_system_info=*/true,
                                       /*send_tab_titles=*/true,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/true};

  scoped_refptr<FeedbackData> feedback_data;
  RunSendReport(std::move(report), expected_params, feedback_data);

  EXPECT_EQ("", feedback_data->user_email());
  EXPECT_EQ("", feedback_data->page_url());
  EXPECT_EQ(base::UTF16ToUTF8(kDescription), feedback_data->description());
  // Verify screenshot is added to feedback data.
  EXPECT_GT(feedback_data->image().size(), 0u);
  // Verify consent data appended to sys_info map.
  auto consent_granted =
      feedback_data->sys_info()->find(kFeedbackUserConsentKey);
  EXPECT_NE(feedback_data->sys_info()->end(), consent_granted);
  EXPECT_EQ(kFeedbackUserConsentKey, consent_granted->first);
  EXPECT_EQ(kFeedbackUserConsentGrantedValue, consent_granted->second);
  auto extra_diagnostics =
      feedback_data->sys_info()->find(kExtraDiagnosticsKey);
  EXPECT_EQ(kExtraDiagnosticsKey, extra_diagnostics->first);
  EXPECT_EQ(kFakeExtraDiagnosticsValue, extra_diagnostics->second);
  // Verify category_tag is marked as BluetoothReportWithLogs in the report.
  EXPECT_EQ(kFeedbackCategoryTag, feedback_data->category_tag());
  EXPECT_EQ(kPerformanceTraceId, feedback_data->trace_id());
  EXPECT_TRUE(feedback_data->from_assistant());
  EXPECT_TRUE(feedback_data->assistant_debug_info_allowed());
}

// Test that feedback params and data are populated with correct data before
// passed to SendFeedback method of the feedback service.
// - System logs and histograms are included.
// - Screenshot is included so tab titles will be sent too.
// - Consent granted.
// - Non-empty extra_diagnostics provided.
// - sentBluetoothLog flag is set false.
// - category_tag is set to a fake value.
// - User is logged in with internal google account.
// - Performance trace id is present.
// - from_assistant flag is set true.
// - Assistant debug info is allowed.
IN_PROC_BROWSER_TEST_F(
    ChromeOsFeedbackDelegateTest,
    FeedbackDataPopulatedIncludeSysLogsAndScreenshotAndFakeCategoryTag) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  report->description = kDescription;
  report->include_screenshot = true;
  report->contact_user_consent_granted = true;
  report->feedback_context->extra_diagnostics = kFakeExtraDiagnosticsValue;
  report->send_bluetooth_logs = false;
  report->feedback_context->category_tag = kFakeCategoryTag;
  report->include_system_logs_and_histograms = true;
  report->feedback_context->is_internal_account = true;
  report->feedback_context->trace_id = kPerformanceTraceId;
  report->feedback_context->from_assistant = true;
  report->feedback_context->assistant_debug_info_allowed = true;
  const FeedbackParams expected_params{/*is_internal_email=*/true,
                                       /*load_system_info=*/true,
                                       /*send_tab_titles=*/true,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/false};

  scoped_refptr<FeedbackData> feedback_data;
  RunSendReport(std::move(report), expected_params, feedback_data);

  EXPECT_EQ("", feedback_data->user_email());
  EXPECT_EQ("", feedback_data->page_url());
  EXPECT_EQ(base::UTF16ToUTF8(kDescription), feedback_data->description());
  // Verify screenshot is added to feedback data.
  EXPECT_GT(feedback_data->image().size(), 0u);
  // Verify consent data appended to sys_info map.
  auto consent_granted =
      feedback_data->sys_info()->find(kFeedbackUserConsentKey);
  EXPECT_NE(feedback_data->sys_info()->end(), consent_granted);
  EXPECT_EQ(kFeedbackUserConsentKey, consent_granted->first);
  EXPECT_EQ(kFeedbackUserConsentGrantedValue, consent_granted->second);
  auto extra_diagnostics =
      feedback_data->sys_info()->find(kExtraDiagnosticsKey);
  EXPECT_EQ(kExtraDiagnosticsKey, extra_diagnostics->first);
  EXPECT_EQ(kFakeExtraDiagnosticsValue, extra_diagnostics->second);
  // Verify category_tag is marked as a fake category tag in the report.
  EXPECT_EQ(kFakeCategoryTag, feedback_data->category_tag());
  EXPECT_EQ(kPerformanceTraceId, feedback_data->trace_id());
  EXPECT_TRUE(feedback_data->from_assistant());
  EXPECT_TRUE(feedback_data->assistant_debug_info_allowed());
}

// Test that feedback params and data are populated with correct data before
// passed to SendFeedback method of the feedback service.
// - System logs and histograms are not included.
// - Screenshot is not included.
// - Consent not granted.
// - sentBluetoothLogs flag is set false.
// - category_tag is not set to "BluetoothReportWithLogs".
// - Empty string Extra Diagnostics provided.
// - User is not logged in with an internal google account.
// - Performance trace id is absent (set to zero).
// - from_assistant flag is set false.
// - Assistant debug info is not allowed.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest,
                       FeedbackDataPopulatedNotIncludeSysLogsOrScreenshot) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  report->feedback_context->email = kSignedInUserEmail;
  report->feedback_context->page_url = GURL(kPageUrl);
  report->feedback_context->extra_diagnostics = std::string();
  report->description = kDescription;
  report->include_screenshot = false;
  report->contact_user_consent_granted = false;
  report->send_bluetooth_logs = false;
  report->feedback_context->is_internal_account = false;
  report->include_system_logs_and_histograms = false;
  report->feedback_context->trace_id = 0;
  report->feedback_context->from_assistant = false;
  report->feedback_context->assistant_debug_info_allowed = false;
  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/false,
                                       /*send_bluetooth_logs=*/false};

  scoped_refptr<FeedbackData> feedback_data;
  RunSendReport(std::move(report), expected_params, feedback_data);

  EXPECT_EQ(kSignedInUserEmail, feedback_data->user_email());
  EXPECT_EQ(kPageUrl, feedback_data->page_url());
  EXPECT_EQ(base::UTF16ToUTF8(kDescription), feedback_data->description());
  // Verify no screenshot is added to feedback data.
  EXPECT_EQ("", feedback_data->image());
  // Verify consent data appended to sys_info map.
  auto consent_denied =
      feedback_data->sys_info()->find(kFeedbackUserConsentKey);
  EXPECT_NE(feedback_data->sys_info()->end(), consent_denied);
  EXPECT_EQ(kFeedbackUserConsentKey, consent_denied->first);
  EXPECT_EQ(kFeedbackUserConsentDeniedValue, consent_denied->second);
  auto extra_diagnostics =
      feedback_data->sys_info()->find(kExtraDiagnosticsKey);
  EXPECT_EQ(feedback_data->sys_info()->end(), extra_diagnostics);
  // Verify category_tag is not marked as BluetoothReportWithLogs.
  EXPECT_NE(kFeedbackCategoryTag, feedback_data->category_tag());
  EXPECT_EQ(0, feedback_data->trace_id());
  EXPECT_FALSE(feedback_data->from_assistant());
  EXPECT_FALSE(feedback_data->assistant_debug_info_allowed());
}

// Test GetScreenshot returns correct data when there is a screenshot.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, HasScreenshot) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());

  OsFeedbackScreenshotManager::GetInstance()->SetPngDataForTesting(
      CreateFakePngData());

  base::test::TestFuture<const std::vector<uint8_t>&> future;
  feedback_delegate_.GetScreenshotPng(future.GetCallback());

  const std::vector<uint8_t> expected{12, 11, 99};
  const std::vector<uint8_t> result = future.Get();
  EXPECT_EQ(expected, result);
}

// Test GetScreenshot returns empty array when there is not a screenshot.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, NoScreenshot) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  base::test::TestFuture<const std::vector<uint8_t>&> future;
  feedback_delegate_.GetScreenshotPng(future.GetCallback());

  const std::vector<uint8_t> result = future.Get();
  EXPECT_EQ(0u, result.size());
}

// Test if Diagnostics app is opened.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, OpenDiagnosticsApp) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  feedback_delegate_.OpenDiagnosticsApp();
  browser_opened.Wait();

  Browser* app_browser = ash::FindSystemWebAppBrowser(
      browser()->profile(), ash::SystemWebAppType::DIAGNOSTICS);

  EXPECT_TRUE(app_browser);
  EXPECT_EQ(diagnostics_url_, FindActiveUrl(app_browser));
}

// Test if Explore app is opened.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, OpenExploreApp) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  feedback_delegate_.OpenExploreApp();
  browser_opened.Wait();

  Browser* app_browser = ash::FindSystemWebAppBrowser(
      browser()->profile(), ash::SystemWebAppType::HELP);

  EXPECT_TRUE(app_browser);
  EXPECT_EQ(explore_url_, FindActiveUrl(app_browser));
}

// Test that the Metrics (Histograms) dialog opens
// when OpenMetricsDialog is invoked.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, OpenMetricsDialog) {
  Browser* feedback_browser = LaunchFeedbackAppAndGetBrowser();

  gfx::NativeWindow feedback_window =
      feedback_browser->window()->GetNativeWindow();

  std::set<views::Widget*> owned_widgets_pre_dialog;
  views::Widget::GetAllOwnedWidgets(feedback_window, &owned_widgets_pre_dialog);

  EXPECT_EQ(owned_widgets_pre_dialog.size(), 0u);

  // Initialize the delegate.
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());

  feedback_delegate_.OpenMetricsDialog();

  std::set<views::Widget*> owned_widgets_post_dialog;
  views::Widget::GetAllOwnedWidgets(feedback_window,
                                    &owned_widgets_post_dialog);

  EXPECT_EQ(owned_widgets_post_dialog.size(), 1u);
}

// Test that the SystemInfo (Histograms) dialog opens
// when OpenSystemInfoDialog is invoked.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, OpenSystemInfoDialog) {
  Browser* feedback_browser = LaunchFeedbackAppAndGetBrowser();

  gfx::NativeWindow feedback_window =
      feedback_browser->window()->GetNativeWindow();

  std::set<views::Widget*> owned_widgets_pre_dialog;
  views::Widget::GetAllOwnedWidgets(feedback_window, &owned_widgets_pre_dialog);

  EXPECT_EQ(owned_widgets_pre_dialog.size(), 0u);

  // Initialize the delegate.
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());

  feedback_delegate_.OpenSystemInfoDialog();

  std::set<views::Widget*> owned_widgets_post_dialog;
  views::Widget::GetAllOwnedWidgets(feedback_window,
                                    &owned_widgets_post_dialog);

  EXPECT_EQ(owned_widgets_post_dialog.size(), 1u);
}

// Test that system logs are preloaded and they are needed.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest,
                       PreloadSystemLogsSuccessful) {
  ReportPtr report = Report::New();
  report->description = kDescription;
  report->feedback_context = FeedbackContext::New();
  // System logs are needed.
  report->include_system_logs_and_histograms = true;
  // FeedbackParams.load_system_info should be false so that system logs will
  // not be loaded again by feedback service.
  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/false};

  scoped_refptr<FeedbackData> feedback_data;
  RunSendReport(std::move(report), expected_params, feedback_data,
                /*preload=*/true);

  // Verify that the system logs have been added to the feedback data when
  // feedback service receives it.
  EXPECT_EQ(3u, feedback_data->sys_info()->size());
  EXPECT_EQ("false",
            feedback_data->sys_info()->find(kFeedbackUserConsentKey)->second);
  EXPECT_EQ(kFakeValue, feedback_data->sys_info()->find(kFakeKey)->second);
  EXPECT_EQ(kTabTitleValue,
            feedback_data->sys_info()
                ->find(feedback::FeedbackReport::kMemUsageWithTabTitlesKey)
                ->second);
}

// Test that system logs are preloaded but they are not needed.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest,
                       PreloadSystemLogsSuccessfulButLogsNotNeeded) {
  ReportPtr report = Report::New();
  report->description = kDescription;
  report->feedback_context = FeedbackContext::New();
  // System logs are not needed.
  report->include_system_logs_and_histograms = false;
  // FeedbackParams.load_system_info should be false so that system logs will
  // not be loaded by feedback service.
  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/false,
                                       /*send_bluetooth_logs=*/false};

  scoped_refptr<FeedbackData> feedback_data;
  RunSendReport(std::move(report), expected_params, feedback_data,
                /*preload=*/true);

  // Verify that the system logs have not been added to the feedback data when
  // feedback service receives it.
  EXPECT_EQ(1u, feedback_data->sys_info()->size());
  EXPECT_EQ("false",
            feedback_data->sys_info()->find(kFeedbackUserConsentKey)->second);
}

// Test that preloading did not finish when the report is being sent.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest,
                       PreloadSystemLogsNotCompleted) {
  ReportPtr report = Report::New();
  report->description = kDescription;
  report->feedback_context = FeedbackContext::New();
  // System logs are needed.
  report->include_system_logs_and_histograms = true;
  // FeedbackParams.load_system_info should be true so that system logs will be
  // loaded by feedback service.
  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/true,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/false};

  scoped_refptr<FeedbackData> feedback_data;
  // Set preload to false to simulate preloading did not complete before sending
  // report.
  RunSendReport(std::move(report), expected_params, feedback_data,
                /*preload=*/false);

  // Verify that the system logs have not been added to the feedback data when
  // feedback service receives it.
  EXPECT_EQ(1u, feedback_data->sys_info()->size());
  EXPECT_EQ("false",
            feedback_data->sys_info()->find(kFeedbackUserConsentKey)->second);
}

}  // namespace ash
