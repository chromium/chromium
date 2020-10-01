// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/blink/public/common/features.h"

namespace federated_learning {

class FlocIdProviderBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    RegisterRequestHandler();

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  virtual void RegisterRequestHandler() {}

  FlocIdProvider* floc_id_provider() {
    return FlocIdProviderFactory::GetForProfile(browser()->profile());
  }

  FlocId GetFlocId() {
    return static_cast<FlocIdProviderImpl*>(floc_id_provider())->floc_id_;
  }

  std::string test_host() const { return "a.test"; }

 protected:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderBrowserTest, NoProviderInIncognitoMode) {
  FlocIdProvider* original_provider = floc_id_provider();
  ASSERT_TRUE(original_provider);

  GURL url = https_server_.GetURL(test_host(), "/title1.html");
  ui_test_utils::NavigateToURL(CreateIncognitoBrowser(), url);

  ASSERT_TRUE(browser()->profile()->HasPrimaryOTRProfile());

  Profile* off_the_record_profile =
      browser()->profile()->GetPrimaryOTRProfile();
  ASSERT_TRUE(off_the_record_profile);

  FlocIdProvider* incognito_floc_id_provider =
      FlocIdProviderFactory::GetForProfile(off_the_record_profile);
  ASSERT_FALSE(incognito_floc_id_provider);
}

class MockFlocRemotePermissionService : public FlocRemotePermissionService {
 public:
  using FlocRemotePermissionService::FlocRemotePermissionService;

  GURL GetQueryFlocPermissionUrl() const override {
    GURL query_url = FlocRemotePermissionService::GetQueryFlocPermissionUrl();

    GURL::Replacements replacements;
    replacements.SetHostStr(replacement_host_);
    replacements.SetPortStr(replacement_port_);

    query_url = query_url.ReplaceComponents(replacements);

    return query_url;
  }

  void SetReplacementHostAndPort(const std::string& replacement_host,
                                 const std::string& replacement_port) {
    replacement_host_ = replacement_host;
    replacement_port_ = replacement_port;
  }

 private:
  std::string replacement_host_;
  std::string replacement_port_;
};

class FlocIdProviderWithCustomizedServicesBrowserTest
    : public FlocIdProviderBrowserTest {
 public:
  FlocIdProviderWithCustomizedServicesBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFlocIdComputedEventLogging,
         features::kFlocIdBlocklistFiltering},
        {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "InterestCohortAPI");
  }

  // BrowserTestBase::SetUpInProcessBrowserTestFixture
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &FlocIdProviderWithCustomizedServicesBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  // FlocIdProviderBrowserTest::RegisterRequestHandler
  void RegisterRequestHandler() override {
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &FlocIdProviderWithCustomizedServicesBrowserTest::HandleRequest,
        base::Unretained(this)));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const GURL& url = request.GetURL();

    // Use the default handler for unrelated requests.
    if (url.path() != "/settings/do_ad_settings_allow_floc_poc")
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    auto it = request.headers.find("Cookie");
    if (it == request.headers.end() || it->second != "user_id=123") {
      response->set_code(net::HTTP_UNAUTHORIZED);
      return std::move(response);
    }

    response->set_code(net::HTTP_OK);
    response->set_content(std::string("[true, true, true]"));
    return std::move(response);
  }

  std::string InvokeInterestCohortJsApi(
      const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      document.interestCohort()
      .then(floc => floc)
      .catch(error => 'rejected');
    )")
        .ExtractString();
  }

  void ConfigureReplacementHostAndPortForRemotePermissionService() {
    MockFlocRemotePermissionService* remote_permission_service =
        static_cast<MockFlocRemotePermissionService*>(
            FlocRemotePermissionServiceFactory::GetForProfile(
                browser()->profile()));
    GURL test_host_base_url = https_server_.GetURL(test_host(), "/");
    remote_permission_service->SetReplacementHostAndPort(
        test_host_base_url.host(), test_host_base_url.port());
  }

  std::vector<GURL> GetHistoryUrls() {
    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    return enumerator.urls();
  }

  void FinishOutstandingRemotePermissionQueries() {
    base::RunLoop run_loop;
    FlocRemotePermissionServiceFactory::GetForProfile(browser()->profile())
        ->QueryFlocPermission(
            base::BindLambdaForTesting([&](bool success) { run_loop.Quit(); }),
            PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
    run_loop.Run();
  }

  void FinishOutstandingHistoryQueries() {
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    HistoryServiceFactory::GetForProfile(browser()->profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->QueryHistory(
            base::string16(), history::QueryOptions(),
            base::BindLambdaForTesting(
                [&](history::QueryResults results) { run_loop.Quit(); }),
            &tracker);
    run_loop.Run();
  }

  void ExpireHistoryBefore(base::Time end_time) {
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    HistoryServiceFactory::GetForProfile(browser()->profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->ExpireHistoryBeforeForTesting(
            end_time, base::BindLambdaForTesting([&]() { run_loop.Quit(); }),
            &tracker);
    run_loop.Run();
  }

  // Turn on sync-history and load the blocklist. Finish outstanding remote
  // permission queries and history queries.
  void InitializeBlocklist(const std::unordered_set<uint64_t>& blocklist) {
    sync_service()->SetActiveDataTypes(syncer::ModelTypeSet::All());
    sync_service()->FireStateChanged();

    g_browser_process->floc_blocklist_service()->OnBlocklistLoadResult(
        blocklist);

    FinishOutstandingRemotePermissionQueries();
    FinishOutstandingHistoryQueries();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(browser()->profile()));
  }

  syncer::FakeUserEventService* user_event_service() {
    return static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetForProfile(
            browser()->profile()));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &FlocIdProviderWithCustomizedServicesBrowserTest::CreateSyncService,
            base::Unretained(this)));

    browser_sync::UserEventServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FlocIdProviderWithCustomizedServicesBrowserTest::
                                CreateUserEventService,
                            base::Unretained(this)));

    FlocRemotePermissionServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FlocIdProviderWithCustomizedServicesBrowserTest::
                                CreateFlocRemotePermissionService,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateSyncService(
      content::BrowserContext* context) {
    auto sync_service = std::make_unique<syncer::TestSyncService>();

    syncer::ModelTypeSet types = syncer::ModelTypeSet::All();
    types.Remove(syncer::HISTORY_DELETE_DIRECTIVES);
    sync_service->SetActiveDataTypes(types);

    return std::move(sync_service);
  }

  std::unique_ptr<KeyedService> CreateUserEventService(
      content::BrowserContext* context) {
    return std::make_unique<syncer::FakeUserEventService>();
  }

  std::unique_ptr<KeyedService> CreateFlocRemotePermissionService(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);

    auto remote_permission_service =
        std::make_unique<MockFlocRemotePermissionService>(
            content::BrowserContext::GetDefaultStoragePartition(profile)
                ->GetURLLoaderFactoryForBrowserProcess());
    return std::move(remote_permission_service);
  }

  void SetPermission(ContentSettingsType content_type,
                     const ContentSettingsPattern& primary_pattern,
                     ContentSetting setting) {
    auto* settings_map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    DCHECK(settings_map);

    settings_map->SetContentSettingCustomScope(
        primary_pattern, ContentSettingsPattern::Wildcard(), content_type,
        std::string() /* resource_identifier */, setting);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<
      BrowserContextDependencyManager::CreateServicesCallbackList::Subscription>
      subscription_;
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       FlocIdValue_OneNavigation) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  InitializeBlocklist({});

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[0];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::NEW,
            event.event_trigger());
  EXPECT_EQ(FlocId::CreateFromHistory({test_host()}).ToUint64(),
            event.floc_id());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       CookieNotSent_RemotePermissionDenied) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/title1.html"));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  InitializeBlocklist({});

  // Expect that the FlocIdComputed user event is not recorded, as we won't
  // record the 1st event after browser/sync startup if there are permission
  // errors. The floc is should also be invalid.
  ASSERT_EQ(0u, user_event_service()->GetRecordedUserEvents().size());
  EXPECT_FALSE(GetFlocId().IsValid());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       HistoryDeleteRecomputeFloc) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  InitializeBlocklist({});

  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  ExpireHistoryBefore(base::Time::Now());
  FinishOutstandingRemotePermissionQueries();
  FinishOutstandingHistoryQueries();

  // Expect that the 2nd FlocIdComputed event should be due to history deletion.
  ASSERT_EQ(2u, user_event_service()->GetRecordedUserEvents().size());

  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[1];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::HISTORY_DELETE,
            event.event_trigger());
  EXPECT_FALSE(event.has_floc_id());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       Blocked_FlocInBlocklist) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  // Load a blocklist that would block the upcoming floc.
  InitializeBlocklist({FlocId::CreateFromHistory({test_host()}).ToUint64()});

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  // Expect that the API call would reject.
  EXPECT_EQ("rejected", InvokeInterestCohortJsApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       NotBlocked_FlocNotInBlocklist) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  // Load a blocklist that would block a floc different from the upcoming floc.
  InitializeBlocklist({FlocId::CreateFromHistory({"b.test"}).ToUint64()});

  // Expect the current floc to have the expected value.
  EXPECT_EQ(GetFlocId(), FlocId::CreateFromHistory({test_host()}));

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  // Expect that the API call would return the expected floc.
  EXPECT_EQ(FlocId::CreateFromHistory({test_host()}).ToString(),
            InvokeInterestCohortJsApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       InterestCohortAPI_FlocNotAvailable) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/title1.html"));

  // Promise rejected as the floc is not yet available.
  EXPECT_EQ("rejected", InvokeInterestCohortJsApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       InterestCohortAPI_MainFrame) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  InitializeBlocklist({});

  // Promise resolved with the expected floc value.
  EXPECT_EQ(FlocId::CreateFromHistory({test_host()}).ToString(),
            InvokeInterestCohortJsApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       InterestCohortAPI_SameOriginSubframe) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/iframe_blank.html"));

  InitializeBlocklist({});

  content::NavigateIframeToURL(
      web_contents(),
      /*iframe_id=*/"test", https_server_.GetURL(test_host(), "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Promise resolved with the expected floc value.
  EXPECT_EQ(FlocId::CreateFromHistory({test_host()}).ToString(),
            InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       InterestCohortAPI_CrossOriginSubframe) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/iframe_blank.html"));

  InitializeBlocklist({});

  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test",
                               https_server_.GetURL("b.test", "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Promise resolved with the expected floc value.
  EXPECT_EQ(FlocId::CreateFromHistory({test_host()}).ToString(),
            InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       InterestCohortAPI_CookiesPermissionDisallow) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/iframe_blank.html"));

  InitializeBlocklist({});

  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test",
                               https_server_.GetURL("b.test", "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Block cookies on "b.test".
  SetPermission(
      ContentSettingsType::COOKIES,
      ContentSettingsPattern::FromURL(https_server_.GetURL("b.test", "/")),
      CONTENT_SETTING_BLOCK);

  // Promise rejected as the cookies permission disallows the child's host.
  EXPECT_EQ("rejected", InvokeInterestCohortJsApi(child));

  // Promise resolved with the expected floc value.
  EXPECT_EQ(FlocId::CreateFromHistory({test_host()}).ToString(),
            InvokeInterestCohortJsApi(web_contents()));
}

}  // namespace federated_learning
