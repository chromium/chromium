// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/federated_learning/floc_event_logger.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/federated_learning/features/features.h"
#include "components/federated_learning/floc_constants.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/metrics/public/cpp/ukm_builders.h"
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

// To intercept the request so as to handle it later.
class MockFlocEventLogger : public FlocEventLogger {
 public:
  using FlocEventLogger::FlocEventLogger;
  using FlocEventLogger::FlocEventLogger::Event;

  ~MockFlocEventLogger() override = default;

  void LogFlocComputedEvent(Event event) override {
    events_.push_back(std::move(event));
  }

  size_t NumberOfLogAttemptsQueued() const { return events_.size(); }

  void HandleLastRequest() {
    ASSERT_LT(0u, events_.size());
    CheckCanLogEvent(base::BindOnce(&FlocEventLogger::OnCanLogEventDecided,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    events_.back()));
  }

 private:
  std::vector<Event> events_;
};

class FlocIdProviderSortingLshUninitializedBrowserTest
    : public FlocIdProviderBrowserTest {
 public:
  FlocIdProviderSortingLshUninitializedBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kFederatedLearningOfCohorts,
        {{"minimum_history_domain_size_required", "1"}});
  }

  void SetUpOnMainThread() override {
    FlocIdProviderBrowserTest::SetUpOnMainThread();
    ConfigureReplacementHostAndPortForRemotePermissionService();
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
                &FlocIdProviderSortingLshUninitializedBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  // FlocIdProviderBrowserTest::RegisterRequestHandler
  void RegisterRequestHandler() override {
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &FlocIdProviderSortingLshUninitializedBrowserTest::HandleRequest,
        base::Unretained(this)));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const GURL& url = request.GetURL();

    // Use the default handler for unrelated requests.
    if (url.path() != "/settings/do_ad_settings_allow_floc_poc")
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    if (!ShouldAllowRemotePermission()) {
      response->set_code(net::HTTP_UNAUTHORIZED);
      return std::move(response);
    }

    response->set_code(net::HTTP_OK);
    response->set_content(std::string("[true, true, true]"));
    return std::move(response);
  }

  virtual bool ShouldAllowRemotePermission() const { return true; }

  std::string InvokeInterestCohortJsApi(
      const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      document.interestCohort()
      .then(floc => JSON.stringify(floc, Object.keys(floc).sort()))
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
            std::u16string(), history::QueryOptions(),
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

  void ClearCookiesBrowsingData() {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_COOKIES,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
    observer.BlockUntilCompletion();
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

  void FinishOutstandingAsyncQueries() {
    FinishOutstandingHistoryQueries();
    FinishOutstandingSortingLshQueries();
    FinishOutstandingRemotePermissionQueries();
  }

  // Turn on sync-history, set up the sorting-lsh file, and trigger the
  // file-ready event.
  void InitializeSortingLsh(
      const std::vector<std::pair<uint32_t, bool>>& sorting_lsh_entries,
      const base::Version& version) {
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
        base::BindRepeating(&FlocIdProviderSortingLshUninitializedBrowserTest::
                                CreateSyncService,
                            base::Unretained(this)));

    browser_sync::UserEventServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FlocIdProviderSortingLshUninitializedBrowserTest::
                                CreateUserEventService,
                            base::Unretained(this)));

    FlocRemotePermissionServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FlocIdProviderSortingLshUninitializedBrowserTest::
                                CreateFlocRemotePermissionService,
                            base::Unretained(this)));

    FlocIdProviderFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FlocIdProviderSortingLshUninitializedBrowserTest::
                                CreateFlocIdProvider,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateSyncService(
      content::BrowserContext* context) {
    return std::make_unique<syncer::TestSyncService>();
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

  std::unique_ptr<KeyedService> CreateFlocIdProvider(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    syncer::SyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile);

    PrivacySandboxSettings* privacy_sandbox_settings =
        PrivacySandboxSettingsFactory::GetForProfile(profile);

    FlocRemotePermissionService* floc_remote_permission_service =
        FlocRemotePermissionServiceFactory::GetForProfile(profile);

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::IMPLICIT_ACCESS);

    syncer::UserEventService* user_event_service =
        browser_sync::UserEventServiceFactory::GetForProfile(profile);

    auto floc_event_logger = std::make_unique<MockFlocEventLogger>(
        sync_service, floc_remote_permission_service, user_event_service);

    // On ChromeOS, there can be more than one profile, but the tests will only
    // be using the first one. So we fix the |floc_event_logger_| to the first
    // one created.
    if (!floc_event_logger_)
      floc_event_logger_ = floc_event_logger.get();

    // Before creating the floc id provider, add some initial history.
    history::HistoryAddPageArgs add_page_args;
    add_page_args.time = base::Time::Now();
    add_page_args.context_id = reinterpret_cast<history::ContextID>(1);
    add_page_args.nav_entry_id = 1;

    add_page_args.url = GURL(base::StrCat({"https://www.initial-history.com"}));
    history_service->AddPage(add_page_args);
    history_service->SetFlocAllowed(add_page_args.context_id,
                                    add_page_args.nav_entry_id,
                                    add_page_args.url);

    return std::make_unique<FlocIdProviderImpl>(
        profile->GetPrefs(), privacy_sandbox_settings, history_service,
        std::move(floc_event_logger));
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

  // Owned by the floc id provider.
  MockFlocEventLogger* floc_event_logger_ = nullptr;

  base::ScopedTempDir scoped_temp_dir_;
  int next_unique_file_suffix_ = 1;

  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshUninitializedBrowserTest,
                       SortingLshBlocked) {
  // All sim_hash will be encoded as 0 during sorting-lsh, and that result will
  // be blocked.
  InitializeSortingLsh({{kMaxNumberOfBitsInFloc, true}}, base::Version("2.0"));

  // Expect that the final id is invalid because it was blocked.
  EXPECT_FALSE(GetFlocId().IsValid());

  EXPECT_EQ(1u, floc_event_logger_->NumberOfLogAttemptsQueued());
  floc_event_logger_->HandleLastRequest();
  FinishOutstandingAsyncQueries();

  // Expect that the FlocIdComputed user event is recorded with the desired
  // sim-hash.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[0];
  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(FlocId::SimHashHistory({"initial-history.com"}), event.floc_id());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshUninitializedBrowserTest,
                       CorruptedSortingLSH) {
  // All sim_hash will be encoded as an invalid id.
  InitializeSortingLsh({}, base::Version("3"));

  // Expect that the final id is invalid due to unexpected sorting-lsh file
  // format.
  EXPECT_FALSE(GetFlocId().IsValid());

  EXPECT_EQ(1u, floc_event_logger_->NumberOfLogAttemptsQueued());
  floc_event_logger_->HandleLastRequest();
  FinishOutstandingAsyncQueries();

  // Expect that the FlocIdComputed user event is recorded with the desired
  // sim-hash.
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[0];
  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(FlocId::SimHashHistory({"initial-history.com"}), event.floc_id());
}

class FlocIdProviderSortingLshInitializedBrowserTest
    : public FlocIdProviderSortingLshUninitializedBrowserTest {
 public:
  void SetUpOnMainThread() override {
    FlocIdProviderSortingLshUninitializedBrowserTest::SetUpOnMainThread();
    ConfigureReplacementHostAndPortForRemotePermissionService();

    // Initialize the sorting-lsh file to trigger the 1st computation. All
    // sim_hash will be encoded as 0 during sorting-lsh process.
    InitializeSortingLsh({{kMaxNumberOfBitsInFloc, false}},
                         base::Version("9.0"));
  }
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       FlocIdValue_ImmediateComputeOnStartUp) {
  EXPECT_TRUE(GetFlocId().IsValid());

  EXPECT_EQ(1u, floc_event_logger_->NumberOfLogAttemptsQueued());
  floc_event_logger_->HandleLastRequest();
  FinishOutstandingAsyncQueries();

  // Check that the original sim_hash is not 0, and we are recording the
  // sim_hash in the event log.
  EXPECT_NE(0u, FlocId::SimHashHistory({"initial-history.com"}));
  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[0];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());
  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(FlocId::SimHashHistory({"initial-history.com"}), event.floc_id());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       UkmEvent) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_TRUE(GetFlocId().IsValid());

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(0u, entries.size());

  GURL main_frame_url = https_server_.GetURL(test_host(), "/title1.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);

  entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder.ExpectEntrySourceHasUrl(entries.front(), main_frame_url);
  ukm_recorder.ExpectEntryMetric(entries.front(),
                                 ukm::builders::FlocPageLoad::kFlocIdName,
                                 /*expected_value=*/0);
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       ClearCookiesInvalidateFloc) {
  EXPECT_TRUE(GetFlocId().IsValid());

  EXPECT_EQ(1u, floc_event_logger_->NumberOfLogAttemptsQueued());

  ClearCookiesBrowsingData();
  FinishOutstandingAsyncQueries();

  // The floc has been invalidated. Expect no additional event logging.
  EXPECT_FALSE(GetFlocId().IsValid());
  EXPECT_EQ(1u, floc_event_logger_->NumberOfLogAttemptsQueued());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       HistoryDeleteInvalidateFloc) {
  EXPECT_TRUE(GetFlocId().IsValid());

  EXPECT_EQ(1u, floc_event_logger_->NumberOfLogAttemptsQueued());
  floc_event_logger_->HandleLastRequest();
  FinishOutstandingAsyncQueries();

  ASSERT_EQ(1u, user_event_service()->GetRecordedUserEvents().size());

  ExpireHistoryBefore(base::Time::Now());
  FinishOutstandingAsyncQueries();
  EXPECT_EQ(2u, floc_event_logger_->NumberOfLogAttemptsQueued());
  floc_event_logger_->HandleLastRequest();
  FinishOutstandingAsyncQueries();

  // Expect that the 2nd FlocIdComputed event has a missing floc field, that
  // implies invalidation due to history deletion.
  ASSERT_EQ(2u, user_event_service()->GetRecordedUserEvents().size());

  const sync_pb::UserEventSpecifics& specifics =
      user_event_service()->GetRecordedUserEvents()[1];
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_FALSE(event.has_floc_id());
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       InterestCohortAPI_FlocNotAvailable) {
  EXPECT_TRUE(GetFlocId().IsValid());

  ExpireHistoryBefore(base::Time::Now());
  FinishOutstandingAsyncQueries();

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/title1.html"));

  // Promise rejected as the floc is not yet available.
  EXPECT_EQ("rejected", InvokeInterestCohortJsApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       InterestCohortAPI_MainFrame) {
  EXPECT_TRUE(GetFlocId().IsValid());

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/title1.html"));

  // Promise resolved with the expected dictionary object.
  EXPECT_EQ(base::StrCat({"{\"id\":\"0\",\"version\":\"chrome.1.9\"}"}),
            InvokeInterestCohortJsApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       InterestCohortAPI_SameOriginSubframe) {
  EXPECT_TRUE(GetFlocId().IsValid());

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/iframe_blank.html"));

  content::NavigateIframeToURL(
      web_contents(),
      /*iframe_id=*/"test", https_server_.GetURL(test_host(), "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Promise resolved with the expected dictionary object.
  EXPECT_EQ(base::StrCat({"{\"id\":\"0\",\"version\":\"chrome.1.9\"}"}),
            InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       InterestCohortAPI_CrossOriginSubframe) {
  EXPECT_TRUE(GetFlocId().IsValid());

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/iframe_blank.html"));

  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test",
                               https_server_.GetURL("b.test", "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Promise resolved with the expected dictionary object.
  EXPECT_EQ(base::StrCat({"{\"id\":\"0\",\"version\":\"chrome.1.9\"}"}),
            InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocIdProviderSortingLshInitializedBrowserTest,
                       InterestCohortAPI_CookiesPermissionDisallow) {
  EXPECT_TRUE(GetFlocId().IsValid());

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(test_host(), "/iframe_blank.html"));

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

  // Promise resolved with the expected dictionary object.
  EXPECT_EQ(base::StrCat({"{\"id\":\"0\",\"version\":\"chrome.1.9\"}"}),
            InvokeInterestCohortJsApi(web_contents()));
}

class FlocIdProviderAutoDenyRemotePermissionBrowserTest
    : public FlocIdProviderSortingLshInitializedBrowserTest {
 public:
  bool ShouldAllowRemotePermission() const override { return false; }
};

IN_PROC_BROWSER_TEST_F(FlocIdProviderAutoDenyRemotePermissionBrowserTest,
                       CookieNotSent_RemotePermissionDenied_NoEventLogging) {
  EXPECT_TRUE(GetFlocId().IsValid());

  EXPECT_EQ(1u, floc_event_logger_->NumberOfLogAttemptsQueued());
  floc_event_logger_->HandleLastRequest();
  FinishOutstandingAsyncQueries();

  // The event shouldn't have been recorded.
  ASSERT_EQ(0u, user_event_service()->GetRecordedUserEvents().size());
}

}  // namespace federated_learning
