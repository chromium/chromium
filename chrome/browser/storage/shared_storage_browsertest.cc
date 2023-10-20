// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "content/public/test/test_shared_storage_header_observer.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace storage {

namespace {

using OperationResult = SharedStorageManager::OperationResult;

const auto& SetOperation =
    content::SharedStorageWriteOperationAndResult::SetOperation;
const auto& AppendOperation =
    content::SharedStorageWriteOperationAndResult::AppendOperation;
const auto& ClearOperation =
    content::SharedStorageWriteOperationAndResult::ClearOperation;

constexpr char kMainHost[] = "a.test";
constexpr char kSimplePagePath[] = "/simple.html";
constexpr char kTitle1Path[] = "/title1.html";
constexpr char kCrossOriginHost[] = "b.test";
constexpr char kThirdOriginHost[] = "c.test";
constexpr char kFourthOriginHost[] = "d.test";
constexpr char kRemainingBudgetPrefix[] = "remaining budget: ";
constexpr char kErrorTypeHistogram[] =
    "Storage.SharedStorage.Worklet.Error.Type";
constexpr char kEntriesQueuedCountHistogram[] =
    "Storage.SharedStorage.AsyncIterator.EntriesQueuedCount";
constexpr char kReceivedEntriesBenchmarksHistogram[] =
    "Storage.SharedStorage.AsyncIterator.ReceivedEntriesBenchmarks";
constexpr char kIteratedEntriesBenchmarksHistogram[] =
    "Storage.SharedStorage.AsyncIterator.IteratedEntriesBenchmarks";
constexpr char kTimingDocumentAddModuleHistogram[] =
    "Storage.SharedStorage.Document.Timing.AddModule";
constexpr char kTimingDocumentRunHistogram[] =
    "Storage.SharedStorage.Document.Timing.Run";
constexpr char kTimingDocumentSelectUrlHistogram[] =
    "Storage.SharedStorage.Document.Timing.SelectURL";
constexpr char kTimingDocumentAppendHistogram[] =
    "Storage.SharedStorage.Document.Timing.Append";
constexpr char kTimingDocumentSetHistogram[] =
    "Storage.SharedStorage.Document.Timing.Set";
constexpr char kTimingDocumentDeleteHistogram[] =
    "Storage.SharedStorage.Document.Timing.Delete";
constexpr char kTimingDocumentClearHistogram[] =
    "Storage.SharedStorage.Document.Timing.Clear";
constexpr char kTimingWorkletAppendHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Append";
constexpr char kTimingWorkletSetHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Set";
constexpr char kTimingWorkletGetHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Get";
constexpr char kTimingWorkletLengthHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Length";
constexpr char kTimingWorkletDeleteHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Delete";
constexpr char kTimingWorkletClearHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Clear";
constexpr char kTimingWorkletKeysHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Keys.Next";
constexpr char kTimingWorkletEntriesHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.Entries.Next";
constexpr char kWorkletNumPerPageHistogram[] =
    "Storage.SharedStorage.Worklet.NumPerPage";
constexpr char kTimingRemainingBudgetHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.RemainingBudget";
constexpr char kPrivateAggregationHostPipeResultHistogram[] =
    "PrivacySandbox.PrivateAggregation.Host.PipeResult";

const double kBudgetAllowed = 5.0;

// In order to cut back on the total number of tests run, we deliberately only
// test three possibilities. In particular, the main host is unenrolled when the
// attestations are unenforced, leaving out the main host enrolled/attestations
// unenforced case. Since this enum is used as a parameter and combined with
// other parameters, using three instead of four cases is especially important
// on Android due to hardware limitations that constrain the total number of
// tests that can be run.
enum class EnforcementAndEnrollmentStatus {
  kAttestationsUnenforced = 0,
  kAttestationsEnforcedMainHostUnenrolled = 1,
  kAttestationsEnforcedMainHostEnrolled = 2,
};

#if BUILDFLAG(IS_ANDROID)
base::FilePath GetChromeTestDataDir() {
  return base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));
}
#endif

// With `WebContentsConsoleObserver`, we can only wait for the last message in a
// group.
base::RepeatingCallback<
    bool(const content::WebContentsConsoleObserver::Message& message)>
MakeFilter(std::vector<std::string> possible_last_messages) {
  return base::BindRepeating(
      [](std::vector<std::string> possible_last_messages,
         const content::WebContentsConsoleObserver::Message& message) {
        return base::Contains(possible_last_messages,
                              base::UTF16ToUTF8(message.message));
      },
      std::move(possible_last_messages));
}

std::string GetSharedStorageDisabledErrorMessage() {
  return base::StrCat({"a JavaScript error: \"Error: ",
                       content::GetSharedStorageDisabledMessage(), "\"\n"});
}

std::string GetSharedStorageSelectURLDisabledErrorMessage() {
  return base::StrCat({"a JavaScript error: \"Error: ",
                       content::GetSharedStorageSelectURLDisabledMessage(),
                       "\"\n"});
}

std::string GetSharedStorageAddModuleDisabledErrorMessage() {
  return base::StrCat({"a JavaScript error: \"Error: ",
                       content::GetSharedStorageAddModuleDisabledMessage(),
                       "\"\n"});
}

void DelayBy(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

// TODO(cammie): Find a way to ensure that histograms are available at the
// necessary time without having to resort to sleeping/polling.
void WaitForHistograms(std::vector<std::string> histogram_names) {
  while (true) {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    std::vector<std::string> still_waiting;
    for (const auto& name : histogram_names) {
      if (!base::StatisticsRecorder::FindHistogram(name))
        still_waiting.push_back(name);
    }

    histogram_names = std::move(still_waiting);

    if (histogram_names.empty())
      break;

    DelayBy(base::Seconds(1));
  }
}

// Return the active RenderFrameHost loaded in the last iframe in `parent_rfh`.
content::RenderFrameHost* LastChild(content::RenderFrameHost* parent_rfh) {
  int child_end = 0;
  while (ChildFrameAt(parent_rfh, child_end))
    child_end++;
  if (child_end == 0)
    return nullptr;
  return ChildFrameAt(parent_rfh, child_end - 1);
}

// Create an <iframe> inside `parent_rfh`, and navigate it toward `url`.
// This returns the new RenderFrameHost associated with new document created in
// the iframe.
content::RenderFrameHost* CreateIframe(content::RenderFrameHost* parent_rfh,
                                       const GURL& url) {
  EXPECT_EQ("iframe loaded",
            content::EvalJs(parent_rfh, content::JsReplace(R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.onload = _ => { resolve("iframe loaded"); };
      document.body.appendChild(iframe);
    }))",
                                                           url)));
  return LastChild(parent_rfh);
}

privacy_sandbox::PrivacySandboxAttestationsMap
MakeSharedStoragePrivacySandboxAttestationsMap(
    const std::vector<GURL>& enrollee_urls,
    bool enroll_for_private_aggregation = false) {
  privacy_sandbox::PrivacySandboxAttestationsMap attestations_map;
  auto attestations_set =
      privacy_sandbox::PrivacySandboxAttestationsGatedAPISet(
          {privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
               kSharedStorage});
  if (enroll_for_private_aggregation) {
    attestations_set.Put(privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                             kPrivateAggregation);
  }
  for (const GURL& url : enrollee_urls) {
    attestations_map[net::SchemefulSite(url)] = attestations_set;
  }
  return attestations_map;
}

class MockChromeContentBrowserClient : public ChromeContentBrowserClient {
 public:
  bool IsSharedStorageAllowed(content::BrowserContext* browser_context,
                              content::RenderFrameHost* rfh,
                              const url::Origin& top_frame_origin,
                              const url::Origin& accessing_origin) override {
    if (bypass_shared_storage_allowed_count_ > 0) {
      bypass_shared_storage_allowed_count_--;
      return true;
    }

    return ChromeContentBrowserClient::IsSharedStorageAllowed(
        browser_context, rfh, top_frame_origin, accessing_origin);
  }

  bool IsSharedStorageSelectURLAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin) override {
    if (bypass_shared_storage_select_url_allowed_count_) {
      bypass_shared_storage_select_url_allowed_count_--;
      return true;
    }

    return ChromeContentBrowserClient::IsSharedStorageSelectURLAllowed(
        browser_context, top_frame_origin, accessing_origin);
  }

  void set_bypass_shared_storage_allowed_count(int count) {
    CHECK_EQ(bypass_shared_storage_allowed_count_, 0);
    bypass_shared_storage_allowed_count_ = count;
  }

  void set_bypass_shared_storage_select_url_allowed_count(int count) {
    CHECK_EQ(bypass_shared_storage_select_url_allowed_count_, 0);
    bypass_shared_storage_select_url_allowed_count_ = count;
  }

 private:
  int bypass_shared_storage_allowed_count_ = 0;
  int bypass_shared_storage_select_url_allowed_count_ = 0;
};

}  // namespace

class SharedStorageChromeBrowserTestBase : public PlatformBrowserTest {
 public:
  SharedStorageChromeBrowserTestBase() {
    base::test::TaskEnvironment task_environment;

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kSharedStorageAPI,
                              privacy_sandbox::kPrivacySandboxSettings4,
                              privacy_sandbox::kPrivacySandboxSettings3,
                              features::kPrivacySandboxAdsAPIsOverride,
                              privacy_sandbox::
                                  kOverridePrivacySandboxSettingsLocalTesting},
        /*disabled_features=*/{});
  }

  ~SharedStorageChromeBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    // `PrivacySandboxAttestations` has a member of type
    // `scoped_refptr<base::SequencedTaskRunner>`, its initialization must be
    // done after a browser process is created.
    PlatformBrowserTest::SetUpOnMainThread();
    scoped_attestations_ =
        std::make_unique<privacy_sandbox::ScopedPrivacySandboxAttestations>(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());

    SetPrefs(EnablePrivacySandbox(), AllowThirdPartyCookies());
    FinishSetUp();

    mock_chrome_content_browser_client_ =
        std::make_unique<MockChromeContentBrowserClient>();
    old_chrome_content_browser_client_ = content::SetBrowserClientForTesting(
        mock_chrome_content_browser_client_.get());
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(old_chrome_content_browser_client_);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::StoragePartition* GetStoragePartition() {
    return content::ToRenderFrameHost(GetActiveWebContents())
        .render_frame_host()
        ->GetStoragePartition();
  }

  Profile* GetProfile() {
#if BUILDFLAG(IS_ANDROID)
    return TabModelList::models()[0]->GetProfile();
#else
    return browser()->profile();
#endif
  }

  privacy_sandbox::PrivacySandboxSettings* GetPrivacySandboxSettings() {
    return PrivacySandboxSettingsFactory::GetForProfile(GetProfile());
  }

  // Virtual so derived classes can delay or perform additional set up before
  // starting the server.
  virtual void FinishSetUp() { CHECK(https_server()->Start()); }

  void SetPrefs(bool enable_privacy_sandbox, bool allow_third_party_cookies) {
    GetProfile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxApisEnabledV2,
                                         enable_privacy_sandbox);
    GetProfile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxManuallyControlledV2, enable_privacy_sandbox);

    GetProfile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            allow_third_party_cookies
                ? content_settings::CookieControlsMode::kOff
                : content_settings::CookieControlsMode::kBlockThirdParty));

    // For `privacy_sandbox::kPrivacySandboxSettings4`, we also need to ensure
    // the `PrivacySandboxDelegate::IsPrivacySandboxRestricted()` response
    // returns the negation of `enable_privacy_sandbox`.
    auto* privacy_sandbox_settings = GetPrivacySandboxSettings();
    auto privacy_sandbox_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    privacy_sandbox_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/!enable_privacy_sandbox);
    privacy_sandbox_delegate->SetUpIsIncognitoProfileResponse(
        /*incognito=*/GetProfile()->IsIncognitoProfile());
    privacy_sandbox_settings->SetDelegateForTesting(
        std::move(privacy_sandbox_delegate));
  }

  void SetThirdPartyCookieSetting(const GURL& main_url) {
    // For `privacy_sandbox::kPrivacySandboxSettings4`, we also need to ensure
    // the specific first-party URL `main_url` used by the test either has its
    // third-party-cookie content setting set to
    // `ContentSetting::CONTENT_SETTING_ALLOW` or
    // `ContentSetting::CONTENT_SETTING_BLOCK`, according to
    // `AllowThirdPartyCookies()`.
    CookieSettingsFactory::GetForProfile(GetProfile())
        ->SetThirdPartyCookieSetting(
            main_url, AllowThirdPartyCookies()
                          ? ContentSetting::CONTENT_SETTING_ALLOW
                          : ContentSetting::CONTENT_SETTING_BLOCK);
  }

  void SetAttestationsMap(
      const privacy_sandbox::PrivacySandboxAttestationsMap& attestations_map) {
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAttestationsForTesting(attestations_map);
  }

  // Unless overridden to do otherwise, enrolls the main host to attest for
  // Shared Storage exactly when `GetEnforcementAndEnrollmentStatus()` is
  // `EnforcementAndEnrollmentStatus::kAttestationsEnforcedMainHostEnrolled`.
  virtual void MaybeEnrollMainHost(const GURL& main_url) {
    privacy_sandbox::PrivacySandboxAttestationsMap attestations_map =
        (GetEnforcementAndEnrollmentStatus() ==
         EnforcementAndEnrollmentStatus::kAttestationsEnforcedMainHostEnrolled)
            ? MakeSharedStoragePrivacySandboxAttestationsMap(
                  std::vector<GURL>({main_url}))
            : privacy_sandbox::PrivacySandboxAttestationsMap();
    SetAttestationsMap(attestations_map);
  }

  void
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage() {
    GURL main_url = https_server()->GetURL(kMainHost, kSimplePagePath);
    SetThirdPartyCookieSetting(main_url);
    MaybeEnrollMainHost(main_url);
    EXPECT_TRUE(NavigateToURL(GetActiveWebContents(), main_url));
  }

  void AddSimpleModule(const content::ToRenderFrameHost& execution_target) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing simple_module.js"}));

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();
    GURL module_script_url =
        https_server()->GetURL(host, "/shared_storage/simple_module.js");

    EXPECT_TRUE(content::ExecJs(
        execution_target,
        content::JsReplace("sharedStorage.worklet.addModule($1)",
                           module_script_url)));

    ASSERT_TRUE(add_module_console_observer.Wait());

    EXPECT_LE(1u,
              content::GetAttachedSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(0u,
              content::GetKeepAliveSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing simple_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));
  }

  bool ExecuteScriptInWorklet(
      const content::ToRenderFrameHost& execution_target,
      const std::string& script,
      const std::string& last_script_message,
      bool use_select_url = false) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing customizable_module.js"}));

    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.emplace_back("{{RUN_FUNCTION_BODY}}", script);

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    GURL module_script_url = https_server()->GetURL(
        host, net::test_server::GetFilePathWithReplacements(
                  "/shared_storage/customizable_module.js",
                  run_function_body_replacement));

    EXPECT_TRUE(content::ExecJs(
        execution_target,
        content::JsReplace("sharedStorage.worklet.addModule($1)",
                           module_script_url)));

    EXPECT_TRUE(add_module_console_observer.Wait());

    EXPECT_LE(1u,
              content::GetAttachedSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(0u,
              content::GetKeepAliveSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing customizable_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));

    content::WebContentsConsoleObserver script_console_observer(
        GetActiveWebContents());
    script_console_observer.SetFilter(MakeFilter(
        {last_script_message, ExpectedSharedStorageDisabledMessage()}));

    if (!use_select_url) {
      content::EvalJsResult result = content::EvalJs(execution_target, R"(
        sharedStorage.run('test-operation');
      )");

      EXPECT_TRUE(script_console_observer.Wait());
      EXPECT_EQ(1u, script_console_observer.messages().size());

      EXPECT_EQ(
          last_script_message,
          base::UTF16ToUTF8(script_console_observer.messages()[0].message));

      return result.error.empty();
    }
    EXPECT_TRUE(
        ExecJs(GetActiveWebContents(),
               content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                  ResolveSelectURLToConfig())));

    // Construct and add the `TestSelectURLFencedFrameConfigObserver` to shared
    // storage worklet host manager.
    content::StoragePartition* storage_partition =
        content::ToRenderFrameHost(GetActiveWebContents())
            .render_frame_host()
            ->GetStoragePartition();
    content::TestSelectURLFencedFrameConfigObserver config_observer(
        storage_partition);
    content::EvalJsResult result = EvalJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-operation',
            [{url: "fenced_frames/title0.html"},
             {url: "fenced_frames/title1.html"},
            ],
            {resolveToConfig: resolveSelectURLToConfig}
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

    EXPECT_TRUE(script_console_observer.Wait());
    EXPECT_EQ(1u, script_console_observer.messages().size());

    EXPECT_EQ(last_script_message,
              base::UTF16ToUTF8(script_console_observer.messages()[0].message));

    if (!result.error.empty()) {
      return false;
    }

    absl::optional<GURL> observed_urn_uuid = config_observer.GetUrnUuid();
    EXPECT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));
    GURL urn_uuid = observed_urn_uuid.value();

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
    }

    return true;
  }

  double RemainingBudget(const content::ToRenderFrameHost& execution_target,
                         bool should_add_module = false,
                         bool keep_alive_after_operation = true) {
    if (should_add_module)
      AddSimpleModule(execution_target);

    content::WebContentsConsoleObserver budget_console_observer(
        GetActiveWebContents());
    const std::string kRemainingBudgetPrefixStr(kRemainingBudgetPrefix);
    budget_console_observer.SetPattern(
        base::StrCat({kRemainingBudgetPrefixStr, "*"}));

    EXPECT_TRUE(ExecJs(execution_target,
                       content::JsReplace("window.keepWorklet = $1;",
                                          keep_alive_after_operation)));

    EXPECT_TRUE(ExecJs(execution_target, R"(
      sharedStorage.run('remaining-budget-operation', {keepAlive: keepWorklet});
    )"));

    bool observed = budget_console_observer.Wait();
    EXPECT_TRUE(observed);
    if (!observed) {
      return nan("");
    }

    EXPECT_EQ(1u, budget_console_observer.messages().size());
    std::string console_message =
        base::UTF16ToUTF8(budget_console_observer.messages()[0].message);
    EXPECT_TRUE(base::StartsWith(console_message, kRemainingBudgetPrefixStr));

    std::string result_string = console_message.substr(
        kRemainingBudgetPrefixStr.size(),
        console_message.size() - kRemainingBudgetPrefixStr.size());

    double result = 0.0;
    EXPECT_TRUE(base::StringToDouble(result_string, &result));

    return result;
  }

  virtual bool ResolveSelectURLToConfig() const { return false; }
  virtual bool EnablePrivacySandbox() const { return true; }
  virtual bool AllowThirdPartyCookies() const { return true; }
  virtual EnforcementAndEnrollmentStatus GetEnforcementAndEnrollmentStatus()
      const {
    return EnforcementAndEnrollmentStatus::kAttestationsUnenforced;
  }

  bool SuccessExpected() {
    return GetEnforcementAndEnrollmentStatus() !=
               EnforcementAndEnrollmentStatus::
                   kAttestationsEnforcedMainHostUnenrolled &&
           EnablePrivacySandbox() && AllowThirdPartyCookies();
  }

  std::string ExpectedSharedStorageDisabledMessage() {
    return "Error: " + content::GetSharedStorageDisabledMessage();
  }

 protected:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<MockChromeContentBrowserClient>
      mock_chrome_content_browser_client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<privacy_sandbox::ScopedPrivacySandboxAttestations>
      scoped_attestations_;
  raw_ptr<content::ContentBrowserClient, AcrossTasksDanglingUntriaged>
      old_chrome_content_browser_client_ = nullptr;
};

class SharedStorageChromeBrowserTest
    : public SharedStorageChromeBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SharedStorageChromeBrowserTest() {
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());

    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
    attestation_feature_.InitWithFeatureState(
        privacy_sandbox::kEnforcePrivacySandboxAttestations,
        GetEnforcementAndEnrollmentStatus() !=
            EnforcementAndEnrollmentStatus::kAttestationsUnenforced);
  }
  ~SharedStorageChromeBrowserTest() override = default;

  bool ResolveSelectURLToConfig() const override { return GetParam(); }

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;
  base::test::ScopedFeatureList attestation_feature_;
};

using SharedStorageChromeBrowserParams = std::tuple<
    /*enable_privacy_sandbox=*/bool,
    /*allow_third_party_cookies=*/bool,
    /*enforcement_and_enrollment_status=*/EnforcementAndEnrollmentStatus>;

class SharedStoragePrefBrowserTest
    : public SharedStorageChromeBrowserTestBase,
      public testing::WithParamInterface<SharedStorageChromeBrowserParams> {
 public:
  SharedStoragePrefBrowserTest() {
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
    attestation_feature_.InitWithFeatureState(
        privacy_sandbox::kEnforcePrivacySandboxAttestations,
        GetEnforcementAndEnrollmentStatus() !=
            EnforcementAndEnrollmentStatus::kAttestationsUnenforced);
  }

  bool ResolveSelectURLToConfig() const override { return true; }
  bool EnablePrivacySandbox() const override { return std::get<0>(GetParam()); }
  bool AllowThirdPartyCookies() const override {
    return std::get<1>(GetParam());
  }
  EnforcementAndEnrollmentStatus GetEnforcementAndEnrollmentStatus()
      const override {
    return std::get<2>(GetParam());
  }

  void AddSimpleModuleWithPermissionBypassed(
      const content::ToRenderFrameHost& execution_target) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing simple_module.js"}));

    // Bypass the following permissions to allow one `addModule()` call.
    mock_chrome_content_browser_client_
        ->set_bypass_shared_storage_allowed_count(1);
    mock_chrome_content_browser_client_
        ->set_bypass_shared_storage_select_url_allowed_count(1);

    EXPECT_TRUE(content::ExecJs(execution_target, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    EXPECT_TRUE(add_module_console_observer.Wait());

    // Shared Storage is enabled in order to `addModule()`.
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing simple_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));
  }

  bool ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      const content::ToRenderFrameHost& execution_target,
      const std::string& script,
      const std::string& last_script_message) {
    content::WebContentsConsoleObserver add_module_console_observer(
        GetActiveWebContents());
    add_module_console_observer.SetFilter(
        MakeFilter({"Finish executing customizable_module.js"}));

    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.emplace_back("{{RUN_FUNCTION_BODY}}", script);

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    GURL module_script_url = https_server()->GetURL(
        host, net::test_server::GetFilePathWithReplacements(
                  "/shared_storage/customizable_module.js",
                  run_function_body_replacement));

    // Bypass the following permissions to allow one call for `addModule()` and
    // `run()` respectively. Any operations nested within the script run by
    // `run()` will have preferences applied according to test parameters. When
    // the latter disallow Shared Storage, it siumlates the situation where
    // preferences are updated to block Shared Storage during the course of a
    // previously allowed `run()` call.
    mock_chrome_content_browser_client_
        ->set_bypass_shared_storage_allowed_count(2);
    mock_chrome_content_browser_client_
        ->set_bypass_shared_storage_select_url_allowed_count(1);

    EXPECT_TRUE(content::ExecJs(
        execution_target,
        content::JsReplace("sharedStorage.worklet.addModule($1)",
                           module_script_url)));

    EXPECT_TRUE(add_module_console_observer.Wait());

    EXPECT_EQ(1u,
              content::GetAttachedSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(0u,
              content::GetKeepAliveSharedStorageWorkletHostsCount(
                  execution_target.render_frame_host()->GetStoragePartition()));
    EXPECT_EQ(1u, add_module_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing customizable_module.js",
        base::UTF16ToUTF8(add_module_console_observer.messages()[0].message));

    WaitForHistograms({kTimingDocumentAddModuleHistogram});
    histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);

    content::WebContentsConsoleObserver script_console_observer(
        GetActiveWebContents());
    script_console_observer.SetFilter(MakeFilter(
        {last_script_message, ExpectedSharedStorageDisabledMessage()}));

    content::EvalJsResult result = content::EvalJs(execution_target, R"(
        sharedStorage.run('test-operation');
      )");

    EXPECT_TRUE(script_console_observer.Wait());
    EXPECT_EQ(1u, script_console_observer.messages().size());

    if (SuccessExpected()) {
      EXPECT_EQ(
          last_script_message,
          base::UTF16ToUTF8(script_console_observer.messages()[0].message));
    } else {
      EXPECT_EQ(
          ExpectedSharedStorageDisabledMessage(),
          base::UTF16ToUTF8(script_console_observer.messages()[0].message));
    }

    WaitForHistograms({kTimingDocumentRunHistogram});
    histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);

    return result.error.empty();
  }

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;
  base::test::ScopedFeatureList attestation_feature_;
};

namespace {
std::string DescribePrefBrowserTestParams(
    const testing::TestParamInfo<SharedStoragePrefBrowserTest::ParamType>&
        info) {
  return base::StrCat(
      {"PrivacySandbox", std::get<0>(info.param) ? "Enabled" : "Disabled",
       "_3PCookies", std::get<1>(info.param) ? "Allowed" : "Blocked",
       "_Attestations",
       (std::get<2>(info.param) !=
        EnforcementAndEnrollmentStatus::kAttestationsUnenforced)
           ? base::StrCat({"Enforced_MainHost",
                           (std::get<2>(info.param) ==
                            EnforcementAndEnrollmentStatus::
                                kAttestationsEnforcedMainHostEnrolled)
                               ? "Enrolled"
                               : "Unenrolled"})
           : "Unenforced"});
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStoragePrefBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Values(EnforcementAndEnrollmentStatus::kAttestationsUnenforced,
                        EnforcementAndEnrollmentStatus::
                            kAttestationsEnforcedMainHostUnenrolled,
                        EnforcementAndEnrollmentStatus::
                            kAttestationsEnforcedMainHostEnrolled)),
    DescribePrefBrowserTestParams);

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, AddModule) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetFilter(MakeFilter({"Finish executing simple_module.js"}));

  content::WebContentsConsoleObserver attestations_console_observer(
      GetActiveWebContents());
  attestations_console_observer.SetPattern(
      "Attestation check for Shared Storage on * failed.");

  content::EvalJsResult result = content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageAddModuleDisabledErrorMessage(), result.error);
    EXPECT_EQ(0u, console_observer.messages().size());

    WaitForHistograms({kErrorTypeHistogram});
    histogram_tester_.ExpectUniqueSample(
        kErrorTypeHistogram,
        blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);

    if (GetEnforcementAndEnrollmentStatus() ==
        EnforcementAndEnrollmentStatus::
            kAttestationsEnforcedMainHostUnenrolled) {
      ASSERT_TRUE(attestations_console_observer.Wait());
      EXPECT_FALSE(attestations_console_observer.messages().empty());
    }
    return;
  }

  ASSERT_TRUE(console_observer.Wait());

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(result.error.empty());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  EXPECT_TRUE(attestations_console_observer.messages().empty());

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, RunOperation) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());
  content::WebContentsConsoleObserver run_op_console_observer(
      GetActiveWebContents());
  run_op_console_observer.SetFilter(
      MakeFilter({"Finish executing \'test-operation\'"}));

  content::EvalJsResult run_op_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )");

  WaitForHistograms({kTimingDocumentAddModuleHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), run_op_result.error);

    // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       GURL(url::kAboutBlankURL)));
    WaitForHistograms({kErrorTypeHistogram, kWorkletNumPerPageHistogram});
    histogram_tester_.ExpectUniqueSample(
        kErrorTypeHistogram,
        blink::SharedStorageWorkletErrorType::kRunWebVisible, 1);
    histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
    return;
  }

  ASSERT_TRUE(run_op_console_observer.Wait());

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(run_op_result.error.empty());
  EXPECT_EQ(1u, run_op_console_observer.messages().size());
  EXPECT_EQ("Finish executing \'test-operation\'",
            base::UTF16ToUTF8(run_op_console_observer.messages()[0].message));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentRunHistogram, kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, RunURLSelectionOperation) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  AddSimpleModuleWithPermissionBypassed(GetActiveWebContents());
  content::WebContentsConsoleObserver run_url_op_console_observer(
      GetActiveWebContents());
  run_url_op_console_observer.SetFilter(
      MakeFilter({"Finish executing \'test-url-selection-operation\'"}));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  // Construct and add the `TestSelectURLFencedFrameConfigObserver` to shared
  // storage worklet host manager.
  content::StoragePartition* storage_partition = GetStoragePartition();
  content::TestSelectURLFencedFrameConfigObserver config_observer(
      storage_partition);
  content::EvalJsResult run_url_op_result = EvalJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              },
              {
                url: "fenced_frames/title1.html",
                reportingMetadata: {
                  "click": "fenced_frames/report1.html"
                }
              },
              {
                url: "fenced_frames/title2.html"
              }
            ],
            {
              data: {'mockResult': 1},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

  WaitForHistograms({kTimingDocumentAddModuleHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageSelectURLDisabledErrorMessage(),
              run_url_op_result.error);

    // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       GURL(url::kAboutBlankURL)));
    WaitForHistograms({kErrorTypeHistogram, kWorkletNumPerPageHistogram});
    histogram_tester_.ExpectUniqueSample(
        kErrorTypeHistogram,
        blink::SharedStorageWorkletErrorType::kSelectURLWebVisible, 1);
    histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
    return;
  }

  ASSERT_TRUE(run_url_op_console_observer.Wait());

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(run_url_op_result.error.empty());
  absl::optional<GURL> observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));
  GURL urn_uuid = observed_urn_uuid.value();

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(run_url_op_result.ExtractString(), observed_urn_uuid->spec());
  }

  EXPECT_EQ(1u, run_url_op_console_observer.messages().size());
  EXPECT_EQ(
      "Finish executing \'test-url-selection-operation\'",
      base::UTF16ToUTF8(run_url_op_console_observer.messages()[0].message));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentSelectUrlHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentSelectUrlHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Set) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::EvalJsResult set_result = content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.set('customKey', 'customValue');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), set_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(set_result.error.empty());

  WaitForHistograms({kTimingDocumentSetHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentSetHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Append) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::EvalJsResult append_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.append('customKey', 'customValue');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), append_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(append_result.error.empty());

  WaitForHistograms({kTimingDocumentAppendHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAppendHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Delete) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::EvalJsResult delete_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.delete('customKey');
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), delete_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(delete_result.error.empty());

  WaitForHistograms({kTimingDocumentDeleteHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentDeleteHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, Clear) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::EvalJsResult clear_result =
      content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.clear();
    )");

  if (!SuccessExpected()) {
    // Shared Storage will be disabled.
    EXPECT_EQ(GetSharedStorageDisabledErrorMessage(), clear_result.error);
    return;
  }

  // Privacy Sandbox is enabled and 3P cookies are allowed, so Shared Storage
  // should be allowed.
  EXPECT_TRUE(clear_result.error.empty());

  WaitForHistograms({kTimingDocumentClearHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentClearHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletSet) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `set()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.set('key0', 'value0'));
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletSetHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletSetHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletAppend) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `append()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.append('key0', 'value0'));
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletAppendHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletAppendHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletDelete) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `delete()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.delete('key0'));
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletDeleteHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletDeleteHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletClear) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `clear()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.clear());
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletClearHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletClearHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletGet) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // To prevent failure in the case where Shared Storage is enabled, we set a
  // key before retrieving it; but in the case here we expect failure, we test
  // only `get()` to isolate the behavior and determine if the promise is
  // rejected solely from that call.
  std::string script = SuccessExpected() ? R"(
      console.log(await sharedStorage.set('key0', 'value0'));
      console.log(await sharedStorage.get('key0'));
      console.log('Finished script');
    )"
                                         : R"(
      console.log(await sharedStorage.get('key0'));
      console.log('Finished script');
    )";

  // If `get()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), script, "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletSetHistogram, kTimingWorkletGetHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletSetHistogram, 1);
    histogram_tester_.ExpectTotalCount(kTimingWorkletGetHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletKeys) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `keys()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletKeysHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletEntries) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `entries()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletEntriesHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletLength) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `length()` fails due to Shared Storage being disabled, there will be a
  // console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.length());
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingWorkletLengthHistogram});
    histogram_tester_.ExpectTotalCount(kTimingWorkletLengthHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrefBrowserTest, WorkletRemainingBudget) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  // If `remainingBudget()` fails due to Shared Storage being disabled, there
  // will be a console message verified in the helper
  // `ExecuteScriptInWorkletWithOuterPermissionsBypassed()` rather than an error
  // message since it is wrapped in a `console.log()` call.
  EXPECT_TRUE(ExecuteScriptInWorkletWithOuterPermissionsBypassed(
      GetActiveWebContents(), R"(
      console.log(await sharedStorage.remainingBudget());
      console.log('Finished script');
    )",
      "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);

  if (SuccessExpected()) {
    WaitForHistograms({kTimingRemainingBudgetHistogram});
    histogram_tester_.ExpectTotalCount(kTimingRemainingBudgetHistogram, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_AllIterated) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 150; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 151);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 151);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 150, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 100,
                                      2);
}

// TODO(crbug.com/1453981): fix and re-enable on Android.
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
#if BUILDFLAG(IS_ANDROID)
                       DISABLED_WorkletKeys_PartiallyIterated
#else
                       WorkletKeys_PartiallyIterated
#endif  // BUILDFLAG(IS_ANDROID)
) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(60));

  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 300; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      var keys = sharedStorage.keys();
      for (let i = 0; i < 150; ++i) {
        let key_dict = await keys.next();
        console.log(key_dict['value']);
      }
      var keys2 = sharedStorage.keys();
      for (let i = 0; i < 243; ++i) {
        let key_dict = await keys2.next();
        console.log(key_dict['value']);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kEntriesQueuedCountHistogram, kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 150 + 243);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 300, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      0);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 100,
                                      0);
}

// TODO(crbug.com/1453981): fix and re-enable on Android.
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
#if BUILDFLAG(IS_ANDROID)
                       DISABLED_WorkletEntries_PartiallyIterated
#else
                       WorkletEntries_PartiallyIterated
#endif  // BUILDFLAG(IS_ANDROID)
) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(60));

  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 300; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 101; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 299; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletEntriesHistogram,
       kEntriesQueuedCountHistogram, kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 101 + 299);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 300, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 100,
                                      0);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_AllIteratedLessThanTenKeys) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 5; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 6);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 6);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 5, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 100,
                                      2);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_PartiallyIteratedLessThanTenKeys) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      for (let i = 0; i < 5; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
      var keys = sharedStorage.keys();
      for (let i = 0; i < 4; ++i) {
        let key_dict = await keys.next();
        console.log(key_dict['value']);
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 2; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      var keys2 = sharedStorage.keys();
      for (let i = 0; i < 3; ++i) {
        let key_dict = await keys2.next();
        console.log(key_dict['value']);
      }
      var entries = sharedStorage.entries();
      for (let i = 0; i < 1; ++i) {
        let entry_dict = await entries.next();
        console.log(entry_dict['value']);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 4 + 3);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 2 + 1);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 5, 4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      4);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 20,
                                      4);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 30,
                                      3);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 40,
                                      3);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 70,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 80,
                                      1);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 90,
                                      0);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       WorkletKeysEntries_AllIteratedNoKeys) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      sharedStorage.set('key', 'value');
      sharedStorage.delete('key');
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kWorkletNumPerPageHistogram, kTimingDocumentAddModuleHistogram,
       kTimingDocumentRunHistogram, kTimingWorkletKeysHistogram,
       kTimingWorkletEntriesHistogram, kEntriesQueuedCountHistogram,
       kReceivedEntriesBenchmarksHistogram,
       kIteratedEntriesBenchmarksHistogram});

  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletKeysHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletEntriesHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kEntriesQueuedCountHistogram, 0, 2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 10,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 20,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 30,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 40,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 50,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 60,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 70,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 80,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 90,
                                      2);
  histogram_tester_.ExpectBucketCount(kReceivedEntriesBenchmarksHistogram, 100,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 0,
                                      2);
  histogram_tester_.ExpectBucketCount(kIteratedEntriesBenchmarksHistogram, 10,
                                      0);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_InvalidScriptUrlError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  std::string invalid_url = "http://#";
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", invalid_url));

  EXPECT_EQ(
      base::StrCat(
          {"a JavaScript error: \"Error: The module script url is invalid.\n",
           "    at __const_std::string&_script__:1:24):\n",
           "        {sharedStorage.worklet.addModule(\"", invalid_url, "\")\n",
           "                               ^^^^^\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_CrossOriginScriptError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url = https_server()->GetURL(kCrossOriginHost,
                                           "/shared_storage/simple_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_EQ(
      base::StrCat({"a JavaScript error: \"Error: Only same origin module ",
                    "script is allowed.",
                    "\n    at __const_std::string&_script__:1:24):\n        ",
                    "{sharedStorage.worklet.addModule(\"",
                    script_url.spec().substr(0, 38),
                    "\n                               ^^^^^\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_LoadFailureError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url = https_server()->GetURL(
      kMainHost, "/shared_storage/nonexistent_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_EQ(
      base::StrCat({"a JavaScript error: \"Error: Failed to load ",
                    script_url.spec(), " HTTP status = 404 Not Found.\"\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_UnexpectedRedirectError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url = https_server()->GetURL(
      kMainHost, "/server-redirect?shared_storage/simple_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_EQ(
      base::StrCat({"a JavaScript error: \"Error: Unexpected redirect on ",
                    script_url.spec(), ".\"\n"}),
      result.error);

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_EmptyResultError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module.js");
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_THAT(
      result.error,
      testing::HasSubstr("ReferenceError: undefinedVariable is not defined"));

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       AddModule_MultipleAddModuleError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/simple_module.js");

  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));
  content::EvalJsResult result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url));

  EXPECT_THAT(result.error,
              testing::HasSubstr("sharedStorage.worklet.addModule() can only "
                                 "be invoked once per browsing context"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kAddModuleWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_NotLoadedError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::EvalJsResult result = content::EvalJs(GetActiveWebContents(), R"(
      sharedStorage.run(
          'test-operation', {data: {}});
    )");

  EXPECT_THAT(
      result.error,
      testing::HasSubstr(
          "sharedStorage.worklet.addModule() has to be called before run()"));

  WaitForHistograms({kErrorTypeHistogram});
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram, blink::SharedStorageWorkletErrorType::kRunWebVisible,
      1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_NotRegisteredError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/simple_module.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation-1', {data: {}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_FunctionError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module2.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation', {data: {}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, Run_ScriptError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module4.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation', {data: {}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       Run_UnexpectedCustomDataError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module5.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.run(
          'test-operation', {data: {'customField': 'customValue123'}});
    )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       SelectUrl_NotLoadedError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  content::EvalJsResult result = EvalJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-1',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

  EXPECT_THAT(result.error,
              testing::HasSubstr("sharedStorage.worklet.addModule() has to be "
                                 "called before selectURL()"));

  WaitForHistograms({kErrorTypeHistogram});

  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLWebVisible, 1);
}

// TODO(https://crbug.com/1484437): Fix flakes.
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_SelectUrl_NotRegisteredError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/simple_module.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-1',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

// TODO(crbug.com/1485061): Test is flaky on ChromeOS and Windows.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_SelectUrl_FunctionError DISABLED_SelectUrl_FunctionError
#else
#define MAYBE_SelectUrl_FunctionError SelectUrl_FunctionError
#endif
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       MAYBE_SelectUrl_FunctionError) {
  // TODO(crbug.com/1485061): Test is flaky on linux-bfcache-rel.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module2.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

// TODO(https://crbug.com/1484437): Fix flakes.
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_SelectUrl_ScriptError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module4.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

// TODO(https://crbug.com/1484437): Fix flakes.
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_SelectUrl_UnexpectedCustomDataError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module5.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {'customField': 'customValue123'},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

// TODO(https://crbug.com/1484437): Fix flakes.
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_SelectUrl_OutOfRangeError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module6.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-1',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

// TODO(https://crbug.com/1484437): Fix flakes.
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_SelectUrl_ReturnValueToIntError) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL script_url =
      https_server()->GetURL(kMainHost, "/shared_storage/erroneous_module6.js");
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(GetActiveWebContents(),
                     content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation-2',
            [
              {
                url: "fenced_frames/title0.html"
              }
            ],
            {
              data: {},
              resolveToConfig: resolveSelectURLToConfig
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram, kErrorTypeHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, DocumentTiming) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(60));

  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');
      sharedStorage.append('key2', 'value22');
      sharedStorage.append('key4', 'value4');

      sharedStorage.delete('key0');
      sharedStorage.delete('key2');
      sharedStorage.clear();
    )"));

  WaitForHistograms(
      {kTimingDocumentSetHistogram, kTimingDocumentAppendHistogram,
       kTimingDocumentDeleteHistogram, kTimingDocumentClearHistogram});

  histogram_tester_.ExpectTotalCount(kTimingDocumentSetHistogram, 6);
  histogram_tester_.ExpectTotalCount(kTimingDocumentAppendHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingDocumentDeleteHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentClearHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest, WorkletTiming) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(60));

  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(),
                                     R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');
      sharedStorage.append('key2', 'value22');
      sharedStorage.append('key4', 'value4');

      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.get('key2'));
      console.log(await sharedStorage.get('key3'));
      console.log(await sharedStorage.get('key4'));
      console.log(await sharedStorage.length());

      sharedStorage.delete('key0');
      sharedStorage.delete('key2');

      // It's necessary to `await` this finally promise, since we are not
      // using the option `keepAlive: true` in the `run()` call. The worklet
      // will be closed by the browser once the worklet signals to the browser
      // that the `run()` call has finished.
      await sharedStorage.clear();

      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server()->GetURL(kCrossOriginHost, kSimplePagePath)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kTimingDocumentRunHistogram,
       kTimingWorkletSetHistogram, kTimingWorkletAppendHistogram,
       kTimingWorkletGetHistogram, kTimingWorkletLengthHistogram,
       kTimingWorkletDeleteHistogram, kTimingWorkletClearHistogram,
       kWorkletNumPerPageHistogram});

  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletSetHistogram, 6);
  histogram_tester_.ExpectTotalCount(kTimingWorkletAppendHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingWorkletGetHistogram, 5);
  histogram_tester_.ExpectTotalCount(kTimingWorkletLengthHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingWorkletDeleteHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingWorkletClearHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
}

// Flaky: https://crbug.com/1406845
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_WorkletNumPerPage_Two) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();

  EXPECT_TRUE(ExecuteScriptInWorklet(main_frame,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  content::RenderFrameHost* iframe = CreateIframe(
      main_frame, https_server()->GetURL(kCrossOriginHost, kSimplePagePath));

  EXPECT_TRUE(ExecuteScriptInWorklet(iframe,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentRunHistogram, kTimingWorkletSetHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 2);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 2, 1);
  EXPECT_LE(1u,
            histogram_tester_.GetAllSamples(kTimingWorkletSetHistogram).size());
}

// Flaky: https://crbug.com/1406845
IN_PROC_BROWSER_TEST_P(SharedStorageChromeBrowserTest,
                       DISABLED_WorkletNumPerPage_Three) {
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();

  EXPECT_TRUE(ExecuteScriptInWorklet(main_frame,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  content::RenderFrameHost* iframe = CreateIframe(
      main_frame, https_server()->GetURL(kCrossOriginHost, kSimplePagePath));

  EXPECT_TRUE(ExecuteScriptInWorklet(iframe,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  content::RenderFrameHost* nested_iframe = CreateIframe(
      iframe, https_server()->GetURL(kThirdOriginHost, kSimplePagePath));

  EXPECT_TRUE(ExecuteScriptInWorklet(nested_iframe,
                                     R"(
      sharedStorage.set('key0', 'value0');
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kTimingDocumentAddModuleHistogram,
                     kTimingDocumentRunHistogram, kTimingWorkletSetHistogram,
                     kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 3);
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 3, 1);
  EXPECT_LE(1u,
            histogram_tester_.GetAllSamples(kTimingWorkletSetHistogram).size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageChromeBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<
                             SharedStorageChromeBrowserTest::ParamType>& info) {
                           return base::StrCat({"ResolveSelectURLTo",
                                                info.param ? "Config" : "URN"});
                         });

class SharedStorageFencedFrameChromeBrowserTest
    : public SharedStorageChromeBrowserTestBase {
 public:
  SharedStorageFencedFrameChromeBrowserTest() {
    base::test::TaskEnvironment task_environment;

    shared_storage_feature_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kSharedStorageAPI,
          {{"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)}}}},
        /*disabled_features=*/{});

    fenced_frame_api_change_feature_.InitAndEnableFeature(
        blink::features::kFencedFramesAPIChanges);

    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);

    attestation_feature_.InitWithFeatureState(
        privacy_sandbox::kEnforcePrivacySandboxAttestations,
        GetEnforcementAndEnrollmentStatus() !=
            EnforcementAndEnrollmentStatus::kAttestationsUnenforced);
  }

  ~SharedStorageFencedFrameChromeBrowserTest() override = default;

  bool ResolveSelectURLToConfig() const override { return true; }

  EnforcementAndEnrollmentStatus GetEnforcementAndEnrollmentStatus()
      const override {
    return EnforcementAndEnrollmentStatus::
        kAttestationsEnforcedMainHostEnrolled;
  }

  void Set3rdPartyCookieAndAttestationSettingsThenNavigateToMainHostPage() {
    main_url_ = https_server()->GetURL(kMainHost, kSimplePagePath);
    iframe_url_ = https_server()->GetURL(kCrossOriginHost, kSimplePagePath);
    new_page_url1_ = https_server()->GetURL(kThirdOriginHost, kSimplePagePath);
    new_page_url2_ = https_server()->GetURL(kFourthOriginHost, kSimplePagePath);

    SetThirdPartyCookieSetting(main_url_);
    SetAttestationsMap(
        MakeSharedStoragePrivacySandboxAttestationsMap(std::vector<GURL>(
            {main_url_, iframe_url_, new_page_url1_, new_page_url2_})));
    EXPECT_TRUE(NavigateToURL(GetActiveWebContents(), main_url_));
  }

  content::RenderFrameHost* SelectURLAndCreateFencedFrame(
      content::RenderFrameHost* render_frame_host,
      bool should_add_module = true,
      bool keep_alive_after_operation = true) {
    if (should_add_module)
      AddSimpleModule(render_frame_host);

    content::WebContentsConsoleObserver run_url_op_console_observer(
        GetActiveWebContents());
    run_url_op_console_observer.SetFilter(
        MakeFilter({"Finish executing \'test-url-selection-operation\'"}));

    EXPECT_TRUE(
        ExecJs(render_frame_host,
               content::JsReplace("window.resolveSelectURLToConfig = $1;",
                                  ResolveSelectURLToConfig())));

    EXPECT_TRUE(ExecJs(render_frame_host,
                       content::JsReplace("window.keepWorklet = $1;",
                                          keep_alive_after_operation)));

    // Construct and add the `TestSelectURLFencedFrameConfigObserver` to shared
    // storage worklet host manager.
    content::StoragePartition* storage_partition = GetStoragePartition();
    content::TestSelectURLFencedFrameConfigObserver config_observer(
        storage_partition);
    content::EvalJsResult run_url_op_result = EvalJs(render_frame_host, R"(
        (async function() {
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            [
              {
                url: "fenced_frames/title0.html"
              },
              {
                url: "fenced_frames/title1.html",
                reportingMetadata:
                {
                  "click": "fenced_frames/report1.html"
                }
              },
              {
                url: "fenced_frames/title2.html"
              }
            ],
            {
              data: {'mockResult': 1},
              resolveToConfig: resolveSelectURLToConfig,
              keepAlive: keepWorklet
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

    EXPECT_TRUE(run_url_op_console_observer.Wait());
    EXPECT_TRUE(run_url_op_result.error.empty());
    const absl::optional<GURL>& observed_urn_uuid =
        config_observer.GetUrnUuid();
    EXPECT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(run_url_op_result.ExtractString(), observed_urn_uuid->spec());
    }

    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));
    EXPECT_EQ(1u, run_url_op_console_observer.messages().size());
    EXPECT_EQ(
        "Finish executing \'test-url-selection-operation\'",
        base::UTF16ToUTF8(run_url_op_console_observer.messages()[0].message));

    return content::CreateFencedFrame(
        render_frame_host,
        ResolveSelectURLToConfig()
            ? content::FencedFrameNavigationTarget("select_url_result")
            : content::FencedFrameNavigationTarget(observed_urn_uuid.value()));
  }

 protected:
  GURL main_url_;
  GURL iframe_url_;
  GURL new_page_url1_;
  GURL new_page_url2_;

 private:
  base::test::ScopedFeatureList shared_storage_feature_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;
  base::test::ScopedFeatureList attestation_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameChromeBrowserTest,
                       FencedFrameNavigateTop_BudgetWithdrawal) {
  Set3rdPartyCookieAndAttestationSettingsThenNavigateToMainHostPage();

  content::RenderFrameHost* iframe =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url_);

  content::RenderFrameHost* fenced_frame_root_node =
      SelectURLAndCreateFencedFrame(iframe);
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe), kBudgetAllowed);

  content::TestNavigationObserver top_navigation_observer(
      GetActiveWebContents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node,
      content::JsReplace("window.open($1, '_unfencedTop')", new_page_url1_)));
  top_navigation_observer.Wait();

  content::RenderFrameHost* new_iframe =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url_);

  // After the top navigation, log(3) bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(RemainingBudget(new_iframe, /*should_add_module=*/true),
                   kBudgetAllowed - std::log2(3));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kTimingDocumentSelectUrlHistogram,
       kTimingDocumentRunHistogram, kTimingRemainingBudgetHistogram,
       kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentSelectUrlHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingRemainingBudgetHistogram, 2);

  // In the MPArch case, some additional pageloads with worklet count 0 are
  // recorded, so we do not use `ExpectUniqueSample()` here.
  histogram_tester_.ExpectBucketCount(kWorkletNumPerPageHistogram, 1, 2);
  EXPECT_EQ(2, histogram_tester_.GetTotalSum(kWorkletNumPerPageHistogram));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameChromeBrowserTest,
    TwoFencedFrames_DifferentURNs_EachNavigateOnce_BudgetWithdrawalTwice) {
  Set3rdPartyCookieAndAttestationSettingsThenNavigateToMainHostPage();

  content::RenderFrameHost* iframe1 =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url_);

  content::RenderFrameHost* fenced_frame_root_node1 =
      SelectURLAndCreateFencedFrame(iframe1);
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe1), kBudgetAllowed);

  content::TestNavigationObserver top_navigation_observer1(
      GetActiveWebContents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node1,
      content::JsReplace("window.open($1, '_unfencedTop')", new_page_url1_)));
  top_navigation_observer1.Wait();

  content::RenderFrameHost* iframe2 =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url_);

  // After the top navigation, log(3) bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe2, /*should_add_module=*/true),
                   kBudgetAllowed - std::log2(3));

  content::RenderFrameHost* fenced_frame_root_node2 =
      SelectURLAndCreateFencedFrame(iframe2, /*should_add_module=*/false);
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe2), kBudgetAllowed - std::log2(3));

  content::TestNavigationObserver top_navigation_observer2(
      GetActiveWebContents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node2,
      content::JsReplace("window.open($1, '_unfencedTop')", new_page_url2_)));
  top_navigation_observer2.Wait();

  content::RenderFrameHost* iframe3 =
      CreateIframe(GetActiveWebContents()->GetPrimaryMainFrame(), iframe_url_);

  // After the top navigation, another log(3) bits should have been withdrawn
  // from the original shared storage origin.
  EXPECT_DOUBLE_EQ(RemainingBudget(iframe3, /*should_add_module=*/true),
                   kBudgetAllowed - std::log2(3) - std::log2(3));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kTimingDocumentAddModuleHistogram, kTimingDocumentSelectUrlHistogram,
       kTimingDocumentRunHistogram, kTimingRemainingBudgetHistogram,
       kWorkletNumPerPageHistogram});
  histogram_tester_.ExpectTotalCount(kTimingDocumentAddModuleHistogram, 3);
  histogram_tester_.ExpectTotalCount(kTimingDocumentSelectUrlHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTimingDocumentRunHistogram, 4);
  histogram_tester_.ExpectTotalCount(kTimingRemainingBudgetHistogram, 4);

  // In the MPArch case, some additional pageloads with worklet count 0 are
  // recorded, so we do not use `ExpectUniqueSample()` here.
  histogram_tester_.ExpectBucketCount(kWorkletNumPerPageHistogram, 1, 3);
  EXPECT_EQ(3, histogram_tester_.GetTotalSum(kWorkletNumPerPageHistogram));
}

class SharedStoragePrivateAggregationChromeBrowserTest
    : public SharedStorageChromeBrowserTestBase,
      public testing::WithParamInterface<EnforcementAndEnrollmentStatus> {
 public:
  SharedStoragePrivateAggregationChromeBrowserTest() {
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
    attestation_feature_.InitWithFeatureState(
        privacy_sandbox::kEnforcePrivacySandboxAttestations,
        GetEnforcementAndEnrollmentStatus() !=
            EnforcementAndEnrollmentStatus::kAttestationsUnenforced);
    private_aggregation_feature_.InitAndEnableFeature(
        blink::features::kPrivateAggregationApi);
  }

  ~SharedStoragePrivateAggregationChromeBrowserTest() override = default;

  bool ResolveSelectURLToConfig() const override { return true; }
  EnforcementAndEnrollmentStatus GetEnforcementAndEnrollmentStatus()
      const override {
    return GetParam();
  }

  // This always enrolls the main host for Shared Storage, but only enrolls the
  // main host for Private Aggregation exactly when
  // `(GetEnforcementAndEnrollmentStatus() ==
  // EnforcementAndEnrollmentStatus::kAttestationsEnforcedMainHostEnrolled)` is
  // true.
  void MaybeEnrollMainHost(const GURL& main_url) override {
    privacy_sandbox::PrivacySandboxAttestationsMap attestations_map =
        MakeSharedStoragePrivacySandboxAttestationsMap(
            std::vector<GURL>({main_url}),
            /*enroll_for_private_aggregation=*/(
                GetEnforcementAndEnrollmentStatus() ==
                EnforcementAndEnrollmentStatus::
                    kAttestationsEnforcedMainHostEnrolled));
    SetAttestationsMap(attestations_map);
  }

 private:
  base::test::ScopedFeatureList shared_storage_feature_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;
  base::test::ScopedFeatureList attestation_feature_;
  base::test::ScopedFeatureList private_aggregation_feature_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStoragePrivateAggregationChromeBrowserTest,
    testing::Values(
        EnforcementAndEnrollmentStatus::kAttestationsUnenforced,
        EnforcementAndEnrollmentStatus::kAttestationsEnforcedMainHostUnenrolled,
        EnforcementAndEnrollmentStatus::kAttestationsEnforcedMainHostEnrolled),
    [](const testing::TestParamInfo<
        SharedStoragePrivateAggregationChromeBrowserTest::ParamType>& info) {
      return base::StrCat(
          {"Attestations",
           (info.param !=
            EnforcementAndEnrollmentStatus::kAttestationsUnenforced)
               ? base::StrCat(
                     {"Enforced_MainHost",
                      (info.param == EnforcementAndEnrollmentStatus::
                                         kAttestationsEnforcedMainHostEnrolled)
                          ? "Enrolled"
                          : "Unenrolled"})
               : "Unenforced"});
    });

IN_PROC_BROWSER_TEST_P(SharedStoragePrivateAggregationChromeBrowserTest,
                       ContributeToHistogramViaRun) {
  // This always enrolls the main host for Shared Storage, but only enrolls the
  // main host for Private Aggregation exactly when `ShouldEnrollMainHost()` is
  // true.
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      console.log('Finished script');
    )",
                                     "Finished script"));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram,
                     kPrivateAggregationHostPipeResultHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationHostPipeResultHistogram,
      SuccessExpected()
          ? content::GetPrivateAggregationHostPipeReportSuccessValue()
          : content::GetPrivateAggregationHostPipeApiDisabledValue(),
      1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrivateAggregationChromeBrowserTest,
                       ContributeToHistogramViaSelectURL) {
  // This always enrolls the main host for Shared Storage, but only enrolls the
  // main host for Private Aggregation exactly when `ShouldEnrollMainHost()` is
  // true.
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  EXPECT_TRUE(ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      console.log('Finished script');
      return 1;
    )",
                                     "Finished script",
                                     /*use_select_url=*/true));

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram,
                     kPrivateAggregationHostPipeResultHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationHostPipeResultHistogram,
      SuccessExpected()
          ? content::GetPrivateAggregationHostPipeReportSuccessValue()
          : content::GetPrivateAggregationHostPipeApiDisabledValue(),
      1);
}

IN_PROC_BROWSER_TEST_P(SharedStoragePrivateAggregationChromeBrowserTest,
                       WithContextId_NoPrivateAggregationJS) {
  // This always enrolls the main host for Shared Storage, but only enrolls the
  // main host for Private Aggregation exactly when `ShouldEnrollMainHost()` is
  // true.
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();
  AddSimpleModule(GetActiveWebContents());

  content::EvalJsResult result = content::EvalJs(GetActiveWebContents(), R"(
        sharedStorage.run('test-operation',
                          {data: {},
                           privateAggregationConfig: {contextId:
                                                      'example_id'}});
      )");

  // Navigate away to record `kWorkletNumPerPageHistogram` histogram.
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  WaitForHistograms({kWorkletNumPerPageHistogram,
                     kPrivateAggregationHostPipeResultHistogram});
  histogram_tester_.ExpectUniqueSample(kWorkletNumPerPageHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationHostPipeResultHistogram,
      SuccessExpected()
          ? content::GetPrivateAggregationHostPipeReportSuccessValue()
          : content::GetPrivateAggregationHostPipeApiDisabledValue(),
      1);
}

class SharedStorageHeaderPrefBrowserTest : public SharedStoragePrefBrowserTest {
 public:
  SharedStorageHeaderPrefBrowserTest() {
    shared_storage_m118_feature_.InitAndEnableFeature(
        blink::features::kSharedStorageAPIM118);
  }

  void FinishSetUp() override {
    observer_ = content::CreateAndOverrideSharedStorageHeaderObserver(
        GetStoragePartition());
  }

 protected:
  base::WeakPtr<content::TestSharedStorageHeaderObserver> observer_;

 private:
  base::test::ScopedFeatureList shared_storage_m118_feature_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStorageHeaderPrefBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Values(EnforcementAndEnrollmentStatus::kAttestationsUnenforced,
                        EnforcementAndEnrollmentStatus::
                            kAttestationsEnforcedMainHostUnenrolled,
                        EnforcementAndEnrollmentStatus::
                            kAttestationsEnforcedMainHostEnrolled)),
    DescribePrefBrowserTestParams);

IN_PROC_BROWSER_TEST_P(SharedStorageHeaderPrefBrowserTest, Basic) {
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      kTitle1Path);
  ASSERT_TRUE(https_server()->Start());
  Set3rdPartyCookieAndMainHostAttestationSettingsThenNavigateToMainHostPage();

  GURL fetch_url = https_server()->GetURL(kMainHost, kTitle1Path);
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(R"(
      fetch($1, {sharedStorageWritable: true});
    )",
                         fetch_url.spec()),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  response.WaitForRequest();
  ASSERT_TRUE(base::Contains(response.http_request()->headers,
                             "Sec-Shared-Storage-Writable"));
  EXPECT_EQ(response.http_request()->content, "");
  response.Send(
      /*http_status=*/net::HTTP_OK,
      /*content_type=*/"text/plain;charset=UTF-8",
      /*content=*/{}, /*cookies=*/{}, /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);

  if (!SuccessExpected()) {
    // Shared Storage is disabled, so the `SharedStorageHeaderObserver` ignores
    // the header and no operations are invoked.
    EXPECT_TRUE(observer_->header_results().empty());
    EXPECT_TRUE(observer_->operations().empty());
    response.Done();
    return;
  }

  // Shared Storage is enabled.

  observer_->WaitForOperations(3);

  url::Origin fetch_origin = url::Origin::Create(fetch_url);
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first, fetch_origin);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(fetch_origin, OperationResult::kSuccess),
                  SetOperation(fetch_origin, "hello", "world", true,
                               OperationResult::kSet),
                  AppendOperation(fetch_origin, "hello", "there",
                                  OperationResult::kSet)));

  response.Done();

  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  ExecuteScriptInWorklet(GetActiveWebContents(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
      console.log('Finished script');
    )",
                         "Finished script");

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing customizable_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing customizable_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finished script",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
}

}  // namespace storage
