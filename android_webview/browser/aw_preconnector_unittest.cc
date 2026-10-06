// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_preconnector.h"

#include <memory>
#include <set>
#include <vector>

#include "android_webview/common/aw_features.h"
#include "base/containers/flat_map.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

// Test stub for NetworkContext that mocks
// PreloadSharedDictionaryInfoForDocument and PreconnectSockets, tracking
// active/disconnected Mojo receivers and observer remotes for AwPreconnector
// unit tests.
class AwPreconnectorTestNetworkContext
    : public network::TestNetworkContext,
      public network::mojom::PreloadedSharedDictionaryInfoHandle {
 public:
  AwPreconnectorTestNetworkContext() = default;

  void PreloadSharedDictionaryInfoForDocument(
      const std::vector<GURL>& urls,
      mojo::PendingReceiver<network::mojom::PreloadedSharedDictionaryInfoHandle>
          preload_handle) override {
    CHECK_EQ(1u, urls.size());
    const GURL& url = urls[0];
    preloaded_urls_.push_back(url);

    auto receiver = std::make_unique<
        mojo::Receiver<network::mojom::PreloadedSharedDictionaryInfoHandle>>(
        this, std::move(preload_handle));
    receiver->set_disconnect_handler(
        base::BindOnce(&AwPreconnectorTestNetworkContext::OnHandleDisconnected,
                       base::Unretained(this), url));
    receivers_[url] = std::move(receiver);
  }

  void PreconnectSockets(
      uint32_t num_streams,
      const GURL& url,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const base::UnguessableToken& network_restrictions_id,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const std::optional<net::ConnectionKeepAliveConfig>& keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          observer_client) override {
    if (observer_client) {
      observers_[url].emplace_back(std::move(observer_client));
    }
  }

  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<network::mojom::ProxyLookupClient>
          proxy_lookup_client) override {
    mojo::Remote<network::mojom::ProxyLookupClient> client(
        std::move(proxy_lookup_client));
    client->OnProxyLookupComplete(net::OK, std::nullopt);
  }

  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient> response_client)
      override {
    mojo::Remote<network::mojom::ResolveHostClient> client(
        std::move(response_client));
    client->OnComplete(net::OK, net::ResolveErrorInfo(net::OK),
                       /*resolved_addresses=*/{},
                       /*endpoint_results_with_metadata=*/{});
  }

  void OnHandleDisconnected(const GURL& url) {
    disconnected_urls_.insert(url);
    receivers_.erase(url);
  }

  void DisconnectPreloadReceiver(const GURL& url) { receivers_.erase(url); }

  void NotifySessionClosed(const GURL& url,
                           size_t index = 0,
                           bool was_ever_used_to_create_streams = false) {
    CHECK(observers_.contains(url));
    CHECK_LT(index, observers_[url].size());
    observers_[url][index]->OnSessionClosed(was_ever_used_to_create_streams);
  }

  void NotifyConnectionFailed(const GURL& url, size_t index = 0) {
    CHECK(observers_.contains(url));
    CHECK_LT(index, observers_[url].size());
    observers_[url][index]->OnConnectionFailed();
  }

  const std::vector<GURL>& preloaded_urls() const { return preloaded_urls_; }
  bool is_handle_bound(const GURL& url) const {
    return receivers_.contains(url) && receivers_.at(url)->is_bound();
  }
  bool is_handle_disconnected(const GURL& url) const {
    return disconnected_urls_.contains(url);
  }

  bool has_observer(const GURL& url) const {
    return observers_.contains(url) && !observers_.at(url).empty();
  }

  size_t observers_count(const GURL& url) const {
    return observers_.contains(url) ? observers_.at(url).size() : 0u;
  }

  size_t active_receivers_count() const { return receivers_.size(); }

 private:
  std::vector<GURL> preloaded_urls_;
  std::set<GURL> disconnected_urls_;
  base::flat_map<GURL,
                 std::unique_ptr<mojo::Receiver<
                     network::mojom::PreloadedSharedDictionaryInfoHandle>>>
      receivers_;
  base::flat_map<
      GURL,
      std::vector<mojo::Remote<network::mojom::ConnectionChangeObserverClient>>>
      observers_;
};

class AwPreconnectorTestBase : public testing::Test {
 public:
  AwPreconnectorTestBase()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUpNetworkContext() {
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    browser_context_->set_is_off_the_record(true);
    test_network_context_ =
        std::make_unique<AwPreconnectorTestNetworkContext>();

    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    network_context_receiver_ =
        std::make_unique<mojo::Receiver<network::mojom::NetworkContext>>(
            test_network_context_.get(),
            network_context_remote.InitWithNewPipeAndPassReceiver());
    browser_context_->GetDefaultStoragePartition()->SetNetworkContextForTesting(
        std::move(network_context_remote));
  }

  void TearDown() override {
    network_context_receiver_.reset();
    browser_context_.reset();
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::ScopedFeatureList feature_list_;
  content::TestContentClientInitializer test_content_client_initializer_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<AwPreconnectorTestNetworkContext> test_network_context_;
  std::unique_ptr<mojo::Receiver<network::mojom::NetworkContext>>
      network_context_receiver_;
};

class AwPreconnectorUnitTest : public AwPreconnectorTestBase {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kWebViewPrewarmDictionaryOnPreconnect,
          {{"MaxEntries", "4"}}},
         {::features::kPreconnectManagerDirectFastPath, {}}},
        {});
    SetUpNetworkContext();
  }
};

class AwPreconnectorFeatureDisabledTest : public AwPreconnectorTestBase {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{::features::kPreconnectManagerDirectFastPath, {}}},
        {features::kWebViewPrewarmDictionaryOnPreconnect});
    SetUpNetworkContext();
  }
};

TEST_F(AwPreconnectorFeatureDisabledTest,
       PrewarmDictionaryDisabledDoesNotPrewarm) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://disabled.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));

  // Verify PreloadSharedDictionaryInfoForDocument is NOT called
  EXPECT_TRUE(test_network_context_->preloaded_urls().empty());
  EXPECT_FALSE(test_network_context_->is_handle_bound(url));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));
}

TEST_F(AwPreconnectorUnitTest, PrewarmDictionaryOnPreconnect) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://prewarm.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));

  // 1. Verify PreloadSharedDictionaryInfoForDocument is called with target
  // URL via Mojo IPC
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->preloaded_urls().size() == 1; }));
  EXPECT_EQ(url, test_network_context_->preloaded_urls()[0]);
  EXPECT_TRUE(test_network_context_->is_handle_bound(url));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));

  // 2. Fast forward 31 seconds to trigger expiry timer
  task_environment_.FastForwardBy(base::Seconds(31));

  // 3. Verify handle was disconnected (reset) after 30s timeout.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
}

TEST_F(AwPreconnectorUnitTest, PrewarmMultipleDictionariesConcurrently) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url1("https://multi-a.example.com/");
  GURL url2("https://multi-b.example.com/");

  // Preconnect to two URLs sequentially
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url1));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url2));

  // Verify BOTH handles remain bound concurrently.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->active_receivers_count() == 2; }));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url1));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url2));

  // Fast forward 31 seconds to expire timers
  task_environment_.FastForwardBy(base::Seconds(31));

  // Verify both handles disconnected
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url1); }));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url2); }));
}

TEST_F(AwPreconnectorUnitTest, EvictOldestDictionaryWhenCapacityExceeded) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url1("https://evict-a.example.com/");
  GURL url2("https://evict-b.example.com/");
  GURL url3("https://evict-c.example.com/");
  GURL url4("https://evict-d.example.com/");
  GURL url5("https://evict-e.example.com/");

  // Preconnect to 4 URLs (reaching default max entries = 4)
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url1));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url2));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url3));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url4));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->active_receivers_count() == 4; }));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url1));

  // Preconnect to 5th URL (should evict url1)
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url5));

  // Wait for Mojo disconnect of url1
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url1); }));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->active_receivers_count() == 4; }));
  EXPECT_TRUE(test_network_context_->is_handle_disconnected(url1));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url2));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url3));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url4));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url5));
}

TEST_F(AwPreconnectorUnitTest,
       RePreconnectPromotesToMRUAndProtectsFromEviction) {
  base::HistogramTester histogram_tester;
  AwPreconnector preconnector(browser_context_.get());
  GURL url1("https://mru-a.example.com/");
  GURL url2("https://mru-b.example.com/");
  GURL url3("https://mru-c.example.com/");
  GURL url4("https://mru-d.example.com/");
  GURL url5("https://mru-e.example.com/");

  // 1. Preconnect 4 URLs (fill LRU cache to max entries = 4)
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url1));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url2));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url3));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url4));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->active_receivers_count() == 4; }));

  // 2. Preconnect url1 AGAIN (moves url1 to MRU, making url2 the LRU item)
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url1));

  // 3. Preconnect 5th URL (exceeds max entries = 4)
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url5));

  // 4. Verify url2 was evicted (disconnected), NOT url1
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url2); }));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->active_receivers_count() == 4; }));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url1));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url1));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url3));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url4));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url5));

  EXPECT_FALSE(preconnector.HasPrewarmedDictionaryForTesting(url2));
  EXPECT_TRUE(preconnector.HasPrewarmedDictionaryForTesting(url1));
  EXPECT_TRUE(preconnector.HasPrewarmedDictionaryForTesting(url3));
  EXPECT_TRUE(preconnector.HasPrewarmedDictionaryForTesting(url4));
  EXPECT_TRUE(preconnector.HasPrewarmedDictionaryForTesting(url5));

  histogram_tester.ExpectBucketCount(
      "Android.WebView.Preconnect.PrewarmDictionary.Event",
      AwPrewarmDictionaryEvent::kEvicted, 1);
}

TEST_F(AwPreconnectorUnitTest, RePreconnectRefreshesExpiryTimer) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://refresh.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_bound(url); }));

  // 1. Fast forward 20 seconds (10 seconds remaining before 30s expiry)
  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));

  // 2. Preconnect to same URL again (resets timer back to full 30 seconds)
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));

  // 3. Fast forward another 20 seconds (total elapsed: 40s from original
  // start). Without refresh, it would have expired at 30s. With refresh, 10s
  // remaining.
  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url));

  // 4. Fast forward another 15 seconds (total elapsed from refresh: 35s > 30s
  // timeout). Now it should expire and disconnect.
  task_environment_.FastForwardBy(base::Seconds(15));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
}

TEST_F(AwPreconnectorUnitTest, EarlyCleanupOnSessionClosed) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://session-closed.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_network_context_->is_handle_bound(url) &&
           test_network_context_->has_observer(url);
  }));

  // Fast forward only 5 seconds (well before 30s timeout).
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));

  // Notify session closed over ConnectionChangeObserverClient.
  test_network_context_->NotifySessionClosed(url);

  // Verify the prewarmed dictionary handle is closed immediately.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
}

TEST_F(AwPreconnectorUnitTest, EarlyCleanupOnConnectionFailed) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://conn-failed.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_network_context_->is_handle_bound(url) &&
           test_network_context_->has_observer(url);
  }));

  // Fast forward 2 seconds.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));

  // Notify connection failed over ConnectionChangeObserverClient.
  test_network_context_->NotifyConnectionFailed(url);

  // Verify the prewarmed dictionary handle is closed immediately.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
}

TEST_F(AwPreconnectorUnitTest,
       MultipleSessionsForSameUrlRemainActiveUntilAllClosed) {
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://multi-session.example.com/");

  // Issue two preconnects for the same URL.
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_network_context_->is_handle_bound(url) &&
           test_network_context_->observers_count(url) == 2;
  }));

  // Close the first session.
  test_network_context_->NotifySessionClosed(url, /*index=*/0);

  // Run pending tasks to ensure IPC dispatch.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // The prewarmed dictionary handle should STILL be bound because session 2 is
  // active!
  EXPECT_TRUE(test_network_context_->is_handle_bound(url));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));

  // Close the second session.
  test_network_context_->NotifySessionClosed(url, /*index=*/1);

  // Now that all sessions for this URL have closed, the handle should be
  // disconnected.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
}

TEST_F(AwPreconnectorUnitTest,
       DisconnectHandlerCleansUpOnNetworkServiceDisconnect) {
  base::HistogramTester histogram_tester;
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://disconnect.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_bound(url); }));
  EXPECT_EQ(1u, test_network_context_->preloaded_urls().size());

  // Simulate Network Service dropping the dictionary receiver (e.g. on
  // onTrimMemory or memory pressure).
  test_network_context_->DisconnectPreloadReceiver(url);

  // AwPreconnector's disconnect handler should be invoked, removing the entry.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !preconnector.HasPrewarmedDictionaryForTesting(url); }));

  // Verify kDisconnected was recorded on disconnect.
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Preconnect.PrewarmDictionary.Event",
      AwPrewarmDictionaryEvent::kDisconnected, 1);

  // Re-preconnecting to the same URL should now trigger a fresh preload
  // instead of a refresh, confirming the previous entry was removed.
  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->preloaded_urls().size() == 2; }));
}

class AwPreconnectorCleanupModeTest : public AwPreconnectorTestBase {
 protected:
  void SetUpFeature(const std::string& mode_string) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kWebViewPrewarmDictionaryOnPreconnect,
          {{"MaxEntries", "4"}, {"CleanupMode", mode_string}}},
         {::features::kPreconnectManagerDirectFastPath, {}}},
        {});
    SetUpNetworkContext();
  }
};

TEST_F(AwPreconnectorCleanupModeTest, EventsOnlyClosesOnSessionClosed) {
  SetUpFeature("events_only");
  base::HistogramTester histogram_tester;
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://events-closed.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_network_context_->is_handle_bound(url) &&
           test_network_context_->has_observer(url);
  }));

  // In events_only mode, no timer is set. Advancing beyond 30s should NOT
  // disconnect the handle.
  task_environment_.FastForwardBy(base::Seconds(35));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Android.WebView.Preconnect.PrewarmDictionary.Event",
                   AwPrewarmDictionaryEvent::kExpired));

  // Closing the session should trigger immediate cleanup.
  test_network_context_->NotifySessionClosed(url);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Android.WebView.Preconnect.PrewarmDictionary.Event",
                   AwPrewarmDictionaryEvent::kClosedSessionClosed));
}

TEST_F(AwPreconnectorCleanupModeTest, EventsOnlyClosesOnConnectionFailed) {
  SetUpFeature("events_only");
  base::HistogramTester histogram_tester;
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://events-failed.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_network_context_->is_handle_bound(url) &&
           test_network_context_->has_observer(url);
  }));

  // In events_only mode, advancing beyond 30s should NOT disconnect the handle.
  task_environment_.FastForwardBy(base::Seconds(35));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));

  // Connection failure should trigger immediate cleanup.
  test_network_context_->NotifyConnectionFailed(url);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Android.WebView.Preconnect.PrewarmDictionary.Event",
                   AwPrewarmDictionaryEvent::kClosedConnectionFailed));
}

TEST_F(AwPreconnectorCleanupModeTest,
       TimerOnlyIgnoresConnectionEventsAndExpiresOnTimer) {
  SetUpFeature("timer_only");
  base::HistogramTester histogram_tester;
  AwPreconnector preconnector(browser_context_.get());
  GURL url("https://timer-only.example.com/");

  EXPECT_TRUE(preconnector.Preconnect(nullptr, url));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_network_context_->is_handle_bound(url) &&
           test_network_context_->has_observer(url);
  }));

  // In timer_only mode, session closure events are ignored.
  test_network_context_->NotifySessionClosed(url);
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Android.WebView.Preconnect.PrewarmDictionary.Event",
                   AwPrewarmDictionaryEvent::kClosedSessionClosed));

  // Connection failed events are also ignored.
  test_network_context_->NotifyConnectionFailed(url);
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(test_network_context_->is_handle_bound(url));
  EXPECT_FALSE(test_network_context_->is_handle_disconnected(url));
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Android.WebView.Preconnect.PrewarmDictionary.Event",
                   AwPrewarmDictionaryEvent::kClosedConnectionFailed));

  // Advancing past the 30s expiry timer (remaining: 21s) should disconnect the
  // handle.
  task_environment_.FastForwardBy(base::Seconds(21));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_network_context_->is_handle_disconnected(url); }));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Android.WebView.Preconnect.PrewarmDictionary.Event",
                   AwPrewarmDictionaryEvent::kExpired));
}

}  // namespace
}  // namespace android_webview
