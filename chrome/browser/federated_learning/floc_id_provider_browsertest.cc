// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
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
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/federated_learning/features/features.h"
#include "components/federated_learning/floc_constants.h"
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
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace federated_learning {

class CopyingFileOutputStream
    : public google::protobuf::io::CopyingOutputStream {
 public:
  explicit CopyingFileOutputStream(base::File file) : file_(std::move(file)) {}

  CopyingFileOutputStream(const CopyingFileOutputStream&) = delete;
  CopyingFileOutputStream& operator=(const CopyingFileOutputStream&) = delete;

  ~CopyingFileOutputStream() override = default;

  // google::protobuf::io::CopyingOutputStream:
  bool Write(const void* buffer, int size) override {
    return file_.WriteAtCurrentPos(static_cast<const char*>(buffer), size) ==
           size;
  }

 private:
  base::File file_;
};

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

  PrefService* floc_prefs() {
    return static_cast<FlocIdProviderImpl*>(floc_id_provider())->prefs_;
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

IN_PROC_BROWSER_TEST_F(FlocIdProviderBrowserTest, PrefsMember) {
  EXPECT_EQ(floc_prefs(), browser()->profile()->GetPrefs());
  EXPECT_NE(floc_prefs(), g_browser_process->local_state());
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
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kFederatedLearningOfCohorts,
        {{"minimum_history_domain_size_required", "1"}});
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

  void FinishOutstandingSortingLshQueries() {
    base::RunLoop run_loop;
    const uint64_t dummy_sim_hash = 0u;
    g_browser_process->floc_sorting_lsh_clusters_service()->ApplySortingLsh(
        dummy_sim_hash,
        base::BindLambdaForTesting(
            [&](base::Optional<uint64_t>, base::Version) { run_loop.Quit(); }));
    run_loop.Run();
  }

  void ExpireHistoryBefore(base::Time end_time) {
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    HistoryServiceFactory::GetForProfile(browser()->profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->ExpireHistoryBetween(
            /*restrict_urls=*/{}, /*begin_time=*/base::Time(), end_time,
            /*user_initiated=*/true,
            base::BindLambdaForTesting([&]() { run_loop.Quit(); }), &tracker);
    run_loop.Run();
  }

  base::FilePath GetUniqueTemporaryPath() {
    CHECK(scoped_temp_dir_.IsValid() || scoped_temp_dir_.CreateUniqueTempDir());
    return scoped_temp_dir_.GetPath().AppendASCII(
        base::NumberToString(next_unique_file_suffix_++));
  }

  base::FilePath CreateSortingLshFile(
      const std::vector<std::pair<uint32_t, bool>>& sorting_lsh_entries) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath file_path = GetUniqueTemporaryPath();
    base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    CHECK(file.IsValid());

    CopyingFileOutputStream copying_stream(std::move(file));
    google::protobuf::io::CopyingOutputStreamAdaptor zero_copy_stream_adaptor(
        &copying_stream);

    google::protobuf::io::CodedOutputStream output_stream(
        &zero_copy_stream_adaptor);

    for (const auto& p : sorting_lsh_entries) {
      uint32_t next = p.first;
      bool is_blocked = p.second;
      if (is_blocked) {
        next |= kSortingLshBlockedMask;
      }
      output_stream.WriteVarint32(next);
    }

    CHECK(!output_stream.HadError());

    return file_path;
  }

  // Finish outstanding async queries for a full floc compute cycle to finish.
  void FinishOutstandingAsyncQueries() {
    FinishOutstandingRemotePermissionQueries();
    FinishOutstandingHistoryQueries();

    if (base::FeatureList::IsEnabled(kFlocIdSortingLshBasedComputation))
      FinishOutstandingSortingLshQueries();
  }

  // Turn on sync-history.
  void InitializeHistorySync() {
    sync_service()->SetActiveDataTypes(syncer::ModelTypeSet::All());
    sync_service()->FireStateChanged();
    FinishOutstandingAsyncQueries();
  }

  // Turn on sync-history, set up the sorting-lsh file, and trigger the
  // file-ready event.
  void InitializeSortingLsh(
      const std::vector<std::pair<uint32_t, bool>>& sorting_lsh_entries,
      const base::Version& version) {
    sync_service()->SetActiveDataTypes(syncer::ModelTypeSet::All());
    sync_service()->FireStateChanged();

    g_browser_process->floc_sorting_lsh_clusters_service()
        ->OnSortingLshClustersFileReady(
            CreateSortingLshFile(sorting_lsh_entries), version);

    FinishOutstandingAsyncQueries();
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
        setting);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  base::ScopedTempDir scoped_temp_dir_;
  int next_unique_file_suffix_ = 1;

  base::CallbackListSubscription subscription_;
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

  InitializeHistorySync();

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[0];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(FlocId::SimHashHistory({test_host()}), event.floc_id());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderWithCustomizedServicesBrowserTest,
                       CookieNotSent_RemotePermissionDenied) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/title1.html"));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  InitializeHistorySync();

  // Expect that the FlocIdComputed user event is recorded with sim-hash not
  // set. The floc should also be invalid.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[0];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_FALSE(event.has_floc_id());

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

  InitializeHistorySync();

  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  ExpireHistoryBefore(base::Time::Now());
  FinishOutstandingAsyncQueries();

  // Expect that the 2nd FlocIdComputed event should be due to history deletion.
  ASSERT_EQ(2u, user_event_service()->GetRecordedUserEvents().size());

  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[1];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_FALSE(event.has_floc_id());
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

  InitializeHistorySync();

  // Promise resolved with the expected string.
  EXPECT_EQ(
      base::StrCat({base::NumberToString(FlocId::SimHashHistory({test_host()})),
                    ".1.0"}),
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

  InitializeHistorySync();

  content::NavigateIframeToURL(
      web_contents(),
      /*iframe_id=*/"test", https_server_.GetURL(test_host(), "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Promise resolved with the expected string.
  EXPECT_EQ(
      base::StrCat({base::NumberToString(FlocId::SimHashHistory({test_host()})),
                    ".1.0"}),
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

  InitializeHistorySync();

  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test",
                               https_server_.GetURL("b.test", "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Promise resolved with the expected string.
  EXPECT_EQ(
      base::StrCat({base::NumberToString(FlocId::SimHashHistory({test_host()})),
                    ".1.0"}),
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

  InitializeHistorySync();

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

  // Promise resolved with the expected string.
  EXPECT_EQ(
      base::StrCat({base::NumberToString(FlocId::SimHashHistory({test_host()})),
                    ".1.0"}),
      InvokeInterestCohortJsApi(web_contents()));
}

class FlocIdProviderSortingLshEnabledBrowserTest
    : public FlocIdProviderWithCustomizedServicesBrowserTest {
 public:
  FlocIdProviderSortingLshEnabledBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kFederatedLearningOfCohorts,
          {{"minimum_history_domain_size_required", "1"}}},
         {kFlocIdSortingLshBasedComputation, {}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshEnabledBrowserTest,
                       SingleSortingLshCluster) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  // All sim_hash will be encoded as 0 during sorting-lsh
  InitializeSortingLsh({{kMaxNumberOfBitsInFloc, false}}, base::Version("9.0"));

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  // Check that the original sim_hash is not 0.
  EXPECT_NE(0u, FlocId::SimHashHistory({test_host()}));

  // Expect that the final id is 0 because the sorting-lsh was applied.
  EXPECT_EQ("0.1.9", InvokeInterestCohortJsApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshEnabledBrowserTest,
                       SortingLshBlocked) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  // All sim_hash will be encoded as 0 during sorting-lsh, and that result will
  // be blocked.
  InitializeSortingLsh({{kMaxNumberOfBitsInFloc, true}}, base::Version("2.0"));

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  // Check that the original sim_hash is not 0.
  EXPECT_NE(0u, FlocId::SimHashHistory({test_host()}));

  // Expect that the final id is invalid because it was blocked.
  EXPECT_FALSE(GetFlocId().IsValid());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshEnabledBrowserTest,
                       CorruptedSortingLSH) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  ConfigureReplacementHostAndPortForRemotePermissionService();

  std::string cookies_to_set = "/set-cookie?user_id=123";
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), cookies_to_set));

  EXPECT_EQ(1u, GetHistoryUrls().size());

  EXPECT_FALSE(GetFlocId().IsValid());

  // All sim_hash will be encoded as an invalid id.
  InitializeSortingLsh({}, base::Version("3"));

  // Expect that the FlocIdComputed user event is recorded.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  // Expect that the final id is invalid due to unexpected sorting-lsh file
  // format.
  EXPECT_FALSE(GetFlocId().IsValid());
}

}  // namespace federated_learning
