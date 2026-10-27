// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_api.h"

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/gtest_tags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#endif

using testing::Eq;

namespace {

constexpr char kOrigin[] = "https://example.com";
constexpr char kConfigurationUrl1[] = "/conf1.json";
constexpr char kConfigurationUrl2[] = "/conf2.json";
constexpr char kConfigurationHash1[] = "asdas9jasidjd";
constexpr char kConfigurationHash2[] = "ghi289sdfsdfk";
constexpr char kConfigurationData1[] = R"(
{
  "key1": "value1",
  "key2" : 2
}
)";
constexpr char kConfigurationData2[] = R"(
{
  "key1": "value_1",
  "key2" : "value_2"
}
)";
constexpr char kConfigurationData3[] = R"(
[1]
)";
constexpr char kKey1[] = "key1";
constexpr char kKey2[] = "key2";
constexpr char kKey3[] = "key3";
constexpr char kValue1[] = "\"value1\"";
constexpr char kValue2[] = "2";
constexpr char kValue12[] = "\"value_1\"";
constexpr char kValue22[] = "\"value_2\"";

struct ResponseTemplate {
  std::string response_body;
  bool should_post_task = false;
};

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    std::map<std::string, ResponseTemplate> templates,
    const net::test_server::HttpRequest& request) {
  if (!templates.contains(request.relative_url)) {
    return std::make_unique<net::test_server::HungResponse>();
  }

  auto response_template = templates[request.relative_url];
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response;
  if (response_template.should_post_task) {
    http_response = std::make_unique<net::test_server::DelayedHttpResponse>(
        base::Seconds(0));
  } else {
    http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  }

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_template.response_body);
  http_response->set_content_type("text/plain");
  return http_response;
}

bool DictValueEquals(std::optional<base::DictValue> value,
                     const std::map<std::string, std::string>& expected) {
  DCHECK(value);
  std::map<std::string, std::string> actual;
  for (auto entry : *value) {
    if (!entry.second.is_string()) {
      return false;
    }
    actual.insert({entry.first, entry.second.GetString()});
  }

  return actual == expected;
}

// Blocks `task_runner` until Release() is called or the blocker is destroyed,
// so that store operations posted in the meantime queue up behind it.
class StoreSequenceBlocker {
 public:
  explicit StoreSequenceBlocker(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : event_(
            base::MakeRefCounted<base::RefCountedData<base::WaitableEvent>>()) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<base::RefCountedData<base::WaitableEvent>> event) {
              base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
              event->data.Wait();
            },
            event_));
  }
  StoreSequenceBlocker(const StoreSequenceBlocker&) = delete;
  StoreSequenceBlocker& operator=(const StoreSequenceBlocker&) = delete;
  ~StoreSequenceBlocker() { Release(); }

  void Release() { event_->data.Signal(); }

 private:
  scoped_refptr<base::RefCountedData<base::WaitableEvent>> event_;
};

// Thread-safe request counter shared between a test and a request handler that
// runs on the embedded test server's thread.
using RequestCounter = base::RefCountedData<std::atomic<int>>;

// Counts requests for `relative_url` on `server`. Must be called before the
// server is started. The counter is ref-counted and shared with the handler,
// so requests that arrive after the test body returned never touch freed
// memory.
scoped_refptr<RequestCounter> CountRequests(net::EmbeddedTestServer* server,
                                            const std::string& relative_url) {
  auto count = base::MakeRefCounted<RequestCounter>(std::in_place, 0);
  server->RegisterRequestHandler(base::BindRepeating(
      [](scoped_refptr<RequestCounter> count, const std::string& relative_url,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == relative_url) {
          count->data.fetch_add(1);
        }
        return nullptr;
      },
      count, relative_url));
  return count;
}

// Runs the current message loop for `delay`.
void RunFor(base::TimeDelta delay) {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delay);
  run_loop.Run();
}

class ManagedConfigurationAPITestBase : public MixinBasedInProcessBrowserTest {
 protected:
  void EnableTestServer(
      const std::map<std::string, ResponseTemplate>& templates) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest, templates));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetConfiguration(const std::string& conf_url,
                        const std::string& conf_hash) {
    base::ListValue trusted_apps;
    base::DictValue entry;
    entry.Set(ManagedConfigurationAPI::kOriginKey, kOrigin);
    entry.Set(ManagedConfigurationAPI::kManagedConfigurationUrlKey,
              embedded_test_server()->GetURL(conf_url).spec());
    entry.Set(ManagedConfigurationAPI::kManagedConfigurationHashKey, conf_hash);
    trusted_apps.Append(std::move(entry));
    profile()->GetPrefs()->SetList(prefs::kManagedConfigurationPerOrigin,
                                   std::move(trusted_apps));
  }

  void ClearConfiguration() {
    profile()->GetPrefs()->SetList(prefs::kManagedConfigurationPerOrigin,
                                   base::ListValue());
  }

  std::optional<base::DictValue> GetValues(
      const std::vector<std::string>& keys) {
    base::test::TestFuture<std::optional<base::DictValue>> value_future;
    api()->GetOriginPolicyConfiguration(origin_, keys,
                                        value_future.GetCallback());
    return value_future.Take();
  }

  std::optional<base::DictValue> GetAllValues() {
    base::test::TestFuture<std::optional<base::DictValue>> value_future;
    api()->GetOriginPolicyConfiguration(origin_, value_future.GetCallback());
    return value_future.Take();
  }

  // Returns the configuration hash committed for the test origin, if any.
  std::optional<std::string> GetCommittedHash() {
    const std::string* hash =
        profile()
            ->GetPrefs()
            ->GetDict(prefs::kLastManagedConfigurationHashForOrigin)
            .FindString(base::HexEncode(origin_.Serialize()));
    return hash ? std::make_optional(*hash) : std::nullopt;
  }

  Profile* profile() { return browser()->GetProfile(); }
  const url::Origin& origin() const { return origin_; }
  ManagedConfigurationAPI* api() {
    return ManagedConfigurationAPIFactory::GetForProfile(profile());
  }

 private:
  const url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
};

}  // namespace

class ManagedConfigurationAPITest : public ManagedConfigurationAPITestBase,
                                    public ManagedConfigurationAPI::Observer {
 public:
  ManagedConfigurationAPITest() = default;
  ManagedConfigurationAPITest(const ManagedConfigurationAPITest&) = delete;
  ManagedConfigurationAPITest& operator=(const ManagedConfigurationAPITest&) =
      delete;
  ~ManagedConfigurationAPITest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    api()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    api()->RemoveObserver(this);
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void WaitForUpdate() {
    if (!updated_) {
      loop_update_ = std::make_unique<base::RunLoop>();
      loop_update_->Run();
    }
    updated_ = false;
  }

  void OnManagedConfigurationChanged() override {
    if (loop_update_ && loop_update_->running()) {
      loop_update_->Quit();
      updated_ = false;
    } else {
      updated_ = true;
    }
  }

  const url::Origin& GetOrigin() const override { return origin(); }

 private:
  bool updated_ = false;
  std::unique_ptr<base::RunLoop> loop_update_;
};

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PRE_DataIsDownloadedAndPersists) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       DataIsDownloadedAndPersists) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-2447f309-0b17-4b53-8879-50ca6eeebc3f");

  // Intentionally do not handle requests so that data has to be read from
  // disk.
  EnableTestServer({});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, AppRemovedFromPolicyList) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  ClearConfiguration();
  WaitForUpdate();
  ASSERT_EQ(GetValues({kKey1, kKey2}), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       ReapplySameConfigurationAfterRemoval) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  ClearConfiguration();
  WaitForUpdate();
  ASSERT_EQ(GetValues({kKey1, kKey2}), std::nullopt);

  // Re-applying the same configuration should re-download and restore values.
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, GetAllValues) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();

  ASSERT_TRUE(
      DictValueEquals(GetAllValues(), {{kKey1, kValue1}, {kKey2, kValue2}}));
}

namespace {

class TestAnyOriginObserver
    : public ManagedConfigurationAPI::AnyOriginObserver {
 public:
  void OnAnyManagedConfigurationChanged(const url::Origin& origin) override {
    last_origin_ = origin;
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  void WaitForChange() { run_loop_.Run(); }
  const url::Origin& last_origin() const { return last_origin_; }

 private:
  url::Origin last_origin_;
  base::RunLoop run_loop_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, AnyOriginObserverNotified) {
  TestAnyOriginObserver observer;
  api()->AddObserver(&observer);

  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  observer.WaitForChange();
  EXPECT_EQ(observer.last_origin(), origin());

  api()->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, UnknownKeys) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();

  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2, kKey3}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, DataUpdates) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PolicyUpdateWhileDownloadingDifferentHash) {
  EnableTestServer(
      {{kConfigurationUrl1, {kConfigurationData1, true /* post_task */}},
       {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);

  // Even though both requests were sent, the first one must be canceled,
  // since the configuration hash has changed.
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PolicyUpdateWhileDownloadingSameHash) {
  EnableTestServer(
      {{kConfigurationUrl1, {kConfigurationData1, true /* post_task */}},
       {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  SetConfiguration(kConfigurationUrl2, kConfigurationHash1);

  // The second request should not be sent, since the hash did not change.
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       NonDictionaryConfiguration) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData3}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));
}

// Verifies that when a session starts while the configuration URL cannot be
// fetched (e.g., transient 404 or network unavailable), the API automatically
// retries and recovers once the server begins serving valid configuration JSON.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       RecoverAfterInitialDownloadFailure) {
  std::atomic<bool> fail_requests{true};
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<bool>* fail_requests,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 &&
            fail_requests->load()) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      &fail_requests));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});

  // Policy is set while the URL fails to download.
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));

  // Server comes online and serves `kConfigurationData1`; the scheduled retry
  // automatically fetches and applies the configuration.
  fail_requests.store(false);
  WaitForUpdate();

  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

// Verifies that when a policy update points to a new URL and hash whose initial
// download fails transiently, the previous configuration remains intact until
// the retry succeeds and applies the new configuration.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       RecoverAfterPolicyUpdateDownloadFailure) {
  std::atomic<bool> fail_url2{true};
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<bool>* fail_url2,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl2 && fail_url2->load()) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      &fail_url2));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});

  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  // Policy updates to (url2, hash2), but url2 fails to download initially.
  // The existing configuration from url1 remains available.
  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  // Once url2 becomes reachable, the scheduled retry fetches and applies it.
  fail_url2.store(false);
  WaitForUpdate();

  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

// Verifies that network reconnection immediately triggers a retry without
// waiting for the exponential backoff timer to expire, and that read requests
// during the backoff window do not bypass the active backoff timer.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       RecoverOnNetworkReconnectionWithoutBackoffBypass) {
  // Keep the retry timer from firing during the test (the delay is capped at
  // the maximum backoff), so that only the simulated reconnection below can
  // trigger the recovery and the request count cannot race the timer.
  api()->SetRetryBackoffForTesting(base::Hours(1), 2.0);
  std::atomic<int> request_count{0};
  std::atomic<bool> fail_requests{true};
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<int>* request_count, std::atomic<bool>* fail_requests,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1) {
          request_count->fetch_add(1);
          if (fail_requests->load()) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_NOT_FOUND);
            return response;
          }
        }
        return nullptr;
      },
      &request_count, &fail_requests));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});

  // Initial fetch fails and schedules the backoff timer.
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));
  const int requests_after_initial_failure = request_count.load();
  ASSERT_GT(requests_after_initial_failure, 0);

  // Calling GetValues() or re-adding an observer while the backoff timer is
  // active must NOT bypass the timer or trigger extra network requests.
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));
  api()->RemoveObserver(this);
  api()->AddObserver(this);
  EXPECT_FALSE(api()->IsFetchInProgressForTesting(origin()));
  EXPECT_EQ(request_count.load(), requests_after_initial_failure);

  // Simulating network reconnection resets backoff and immediately fetches.
  fail_requests.store(false);
  static_cast<network::NetworkConnectionTracker::NetworkConnectionObserver*>(
      api())
      ->OnConnectionChanged(net::NetworkChangeNotifier::CONNECTION_WIFI);
  WaitForUpdate();

  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

// Verifies that if an initial configuration download (url1, hash1) fails and
// the policy is updated to a different hash (url2, hash2) before url1 ever
// succeeds, the pending retry for url1 is canceled and (url2, hash2) is
// downloaded and applied immediately.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PolicyUpdateAfterInitialDownloadFailure) {
  std::atomic<bool> fail_url1{true};
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<bool>* fail_url1,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 && fail_url1->load()) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      &fail_url1));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});

  // Initial configuration (url1, hash1) fails to download.
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));

  // Before url1 ever succeeds, policy updates to (url2, hash2).
  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

// Verifies that if an initial download fails because the policy URL had a typo
// or was unreachable, and the admin updates `managed_configuration_url` while
// keeping `managed_configuration_hash` identical, the new URL is fetched
// immediately.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       UrlUpdatedWithSameHashAfterFailure) {
  std::atomic<bool> fail_url1{true};
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<bool>* fail_url1,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 && fail_url1->load()) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      &fail_url1));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData1}}});

  // Initial configuration (url1, hash1) fails to download.
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));

  // Policy updates to (url2, hash1) with the same hash.
  SetConfiguration(kConfigurationUrl2, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

// Verifies that configuration keys containing dots (e.g. "a.b.c") are stored
// and retrieved as literal flat keys rather than expanded into nested
// dictionaries.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, DottedConfigurationKeys) {
  constexpr char kDottedConfigData[] = R"({
    "a.b.c": "dotted_value",
    "plain": 42
  })";
  EnableTestServer({{kConfigurationUrl1, {kDottedConfigData}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();

  ASSERT_TRUE(
      DictValueEquals(GetValues({"a.b.c", "plain"}),
                      {{"a.b.c", "\"dotted_value\""}, {"plain", "42"}}));
}

// Verifies that if an existing origin's policy entry is updated to omit the
// configuration URL or hash, its previously stored configuration is cleared.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PolicyEntryMissingUrlOrHashClearsConfiguration) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  // Update policy entry for `kOrigin` with no URL or hash.
  base::ListValue trusted_apps;
  base::DictValue entry;
  entry.Set(ManagedConfigurationAPI::kOriginKey, kOrigin);
  trusted_apps.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kManagedConfigurationPerOrigin,
                                 std::move(trusted_apps));
  WaitForUpdate();

  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));
}

// Verifies that keeping SKIP_ON_SHUTDOWN on the store task runner is safe: if
// the browser shuts down before a configuration download and store write have
// completed (`OnStoreConfigurationCompleted`),
// `kLastManagedConfigurationHashForOrigin` is not committed prematurely, so the
// next session startup downloads and applies the configuration cleanly.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PRE_ShutdownBeforeStoreWriteCompletes) {
  EnableTestServer(
      {{kConfigurationUrl1, {kConfigurationData1, true /* post_task */}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  // Intentionally exit immediately without waiting for the download or
  // SKIP_ON_SHUTDOWN store write to complete.
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       ShutdownBeforeStoreWriteCompletes) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetCommittedHash() == kConfigurationHash1; }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

// Verifies that a read is answered from the store right away while a download
// is in flight, instead of waiting for the download.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, ReadNotHeldBackByDownload) {
  scoped_refptr<base::SequencedTaskRunner> store_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  api()->SetStoreTaskRunnerForTesting(store_task_runner);
  // Requests for kConfigurationUrl2 never receive a response.
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();

  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
  ASSERT_TRUE(api()->IsFetchInProgressForTesting(origin()));
  base::test::TestFuture<std::optional<base::DictValue>> read;
  api()->GetOriginPolicyConfiguration(origin(), {kKey1, kKey2},
                                      read.GetCallback());
  // A read that was sent to the store right away completes before a store task
  // that is posted afterwards.
  base::RunLoop run_loop;
  store_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                      run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(read.IsReady());
  EXPECT_TRUE(
      DictValueEquals(read.Take(), {{kKey1, kValue1}, {kKey2, kValue2}}));
}

// Verifies that the retry backoff keeps growing while every store write fails,
// instead of being reset by each successful download.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       BackoffGrowsWhileStoreWritesKeepFailing) {
  // The first retry happens after 100ms, the next one only after 100s.
  api()->SetRetryBackoffForTesting(base::Milliseconds(100), 1000.0);
  scoped_refptr<RequestCounter> requests =
      CountRequests(embedded_test_server(), kConfigurationUrl1);
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  {
    // A regular file where the origin's LevelDB directory would be created
    // ("<profile>/Managed Configuration/<hex-encoded origin>") makes every
    // store operation fail.
    base::ScopedAllowBlockingForTesting allow_blocking;
    const base::FilePath stores_dir =
        profile()->GetPath().AppendASCII("Managed Configuration");
    ASSERT_TRUE(base::CreateDirectory(stores_dir));
    ASSERT_TRUE(base::WriteFile(
        stores_dir.AppendASCII(base::HexEncode(origin().Serialize())), "x"));
  }

  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  // Both the initial download and the first retry fail to be stored.
  ASSERT_TRUE(base::test::RunUntil([&] { return requests->data.load() >= 2; }));
  // A backoff reset by the successful download would retry ~100ms after the
  // second failed write; the grown backoff waits 100s instead.
  RunFor(base::Seconds(5));
  EXPECT_EQ(requests->data.load(), 2);
}

// Verifies that if the policy reverts to the stored configuration while the
// write of a newer configuration is still queued, the store ends up matching
// the policy again instead of keeping the newer configuration.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PolicyRevertWhileWriteQueuedRestoresConfiguration) {
  scoped_refptr<base::SequencedTaskRunner> store_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  api()->SetStoreTaskRunnerForTesting(store_task_runner);
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_EQ(GetCommittedHash(), kConfigurationHash1);

  {
    StoreSequenceBlocker blocker(store_task_runner);
    // The policy moves to (url2, hash2). Once its download finished, the
    // committed hash is invalidated and the write is queued behind `blocker`.
    SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
    ASSERT_TRUE(base::test::RunUntil([&] { return !GetCommittedHash(); }));
    // The policy reverts to (url1, hash1) before that write lands.
    SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  }

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetCommittedHash() == kConfigurationHash1; }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

// Verifies that a store write with a later write queued behind it does not
// commit its hash. Otherwise the committed hash would match the policy while
// the store holds the later write's configuration, and the configuration would
// not be downloaded again.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       SupersededWriteDoesNotCommitHash) {
  scoped_refptr<base::SequencedTaskRunner> store_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  api()->SetStoreTaskRunnerForTesting(store_task_runner);
  auto fail_url2 =
      base::MakeRefCounted<base::RefCountedData<std::atomic<bool>>>(
          std::in_place, false);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](scoped_refptr<base::RefCountedData<std::atomic<bool>>> fail_url2,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl2 &&
            fail_url2->data.load()) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      fail_url2));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});

  {
    StoreSequenceBlocker blocker(store_task_runner);
    // The writes of (url2, hash2) and then (url1, hash1) are queued behind
    // `blocker`.
    SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
    SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
    // The policy returns to (url2, hash2), but downloading it again fails.
    fail_url2->data.store(true);
    SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
  }
  // Wait until both queued writes completed and their replies ran.
  base::RunLoop run_loop;
  store_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                      run_loop.QuitClosure());
  run_loop.Run();
  // The store holds the configuration of (url1, hash1), so hash2 must not be
  // committed.
  EXPECT_FALSE(GetCommittedHash());

  // Once url2 can be downloaded again, a retry restores its configuration.
  fail_url2->data.store(false);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetCommittedHash() == kConfigurationHash2; }));
  EXPECT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

// Verifies that a read issued while a downloaded configuration is still being
// written waits for that write instead of downloading the configuration again.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       ReadDuringQueuedWriteDoesNotRedownload) {
  scoped_refptr<base::SequencedTaskRunner> store_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  api()->SetStoreTaskRunnerForTesting(store_task_runner);
  scoped_refptr<RequestCounter> url2_requests =
      CountRequests(embedded_test_server(), kConfigurationUrl2);
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();

  base::test::TestFuture<std::optional<base::DictValue>> read;
  {
    StoreSequenceBlocker blocker(store_task_runner);
    // Once the download of (url2, hash2) finished, the committed hash is
    // invalidated and the write is queued behind `blocker`.
    SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
    ASSERT_TRUE(base::test::RunUntil([&] { return !GetCommittedHash(); }));
    api()->GetOriginPolicyConfiguration(origin(), {kKey1, kKey2},
                                        read.GetCallback());
    EXPECT_FALSE(read.IsReady());
  }

  ASSERT_TRUE(
      DictValueEquals(read.Take(), {{kKey1, kValue12}, {kKey2, kValue22}}));
  EXPECT_FALSE(api()->IsFetchInProgressForTesting(origin()));
  EXPECT_EQ(url2_requests->data.load(), 1);
}

// Verifies that an empty configuration hash, which the policy schema allows,
// is committed like any other hash, so that reads do not download the
// configuration again.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       EmptyHashIsCommittedAndNotRedownloaded) {
  scoped_refptr<RequestCounter> requests =
      CountRequests(embedded_test_server(), kConfigurationUrl1);
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, /*conf_hash=*/"");
  WaitForUpdate();
  EXPECT_EQ(GetCommittedHash(), "");

  // A read starts any download synchronously, so an extra download is either
  // still in flight or counted below.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                                {{kKey1, kValue1}, {kKey2, kValue2}}));
  }
  EXPECT_FALSE(api()->IsFetchInProgressForTesting(origin()));
  EXPECT_EQ(requests->data.load(), 1);
}

// Verifies that the queued write of a configuration with an empty hash does
// not commit the hash if the configuration was cleared in the meantime, so
// that re-adding the origin downloads its configuration again.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       EmptyHashNotCommittedAfterConfigurationCleared) {
  scoped_refptr<base::SequencedTaskRunner> store_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  api()->SetStoreTaskRunnerForTesting(store_task_runner);
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();

  {
    StoreSequenceBlocker blocker(store_task_runner);
    // The write of (url2, "") is queued behind `blocker` when the origin is
    // removed from the policy.
    SetConfiguration(kConfigurationUrl2, /*conf_hash=*/"");
    ASSERT_TRUE(base::test::RunUntil([&] { return !GetCommittedHash(); }));
    ClearConfiguration();
  }
  // Wait until both queued writes completed and their replies ran.
  base::RunLoop run_loop;
  store_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(GetCommittedHash());

  // Re-adding the origin downloads its configuration again.
  SetConfiguration(kConfigurationUrl2, /*conf_hash=*/"");
  ASSERT_TRUE(base::test::RunUntil([&] { return GetCommittedHash() == ""; }));
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

// Verifies that a configuration sync succeeding on the first attempt records 0
// retries and no retry reasons, and that subsequent reads when the committed
// hash already matches policy do not emit additional samples.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       MetricsRecordedOnImmediateSuccessWithoutDuplicateReads) {
  base::HistogramTester histogram_tester;
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_EQ(GetCommittedHash(), kConfigurationHash1);

  histogram_tester.ExpectTotalCount(
      ManagedConfigurationAPI::kRetryReasonHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 0, 1);

  // Subsequent reads and no-op policy refreshes with the same committed hash
  // must not record extra samples.
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  histogram_tester.ExpectTotalCount(
      ManagedConfigurationAPI::kRetryReasonHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 0, 1);
}

// Verifies that each failed download attempt records `kDownloadFailed` as it is
// scheduled, and that `RetryCountUntilSuccess` records the exact retry count
// once a retry succeeds.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       MetricsRecordedForMultipleDownloadRetries) {
  base::HistogramTester histogram_tester;
  api()->SetRetryBackoffForTesting(base::Milliseconds(10), 1.0);
  auto remaining_failures =
      base::MakeRefCounted<base::RefCountedData<std::atomic<int>>>(
          std::in_place, 2);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](scoped_refptr<base::RefCountedData<std::atomic<int>>>
             remaining_failures,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 &&
            remaining_failures->data.fetch_sub(1) > 0) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      remaining_failures));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});

  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_EQ(GetCommittedHash(), kConfigurationHash1);

  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kDownloadFailed, 2);
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 2, 1);
}

// Verifies that recovering via `OnConnectionChanged()` (which resets
// `retry_backoff_` to retry immediately) preserves the retry count for the
// active sync rather than reporting 0 retries.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       MetricsPreservedAcrossNetworkReconnectionRecovery) {
  base::HistogramTester histogram_tester;
  api()->SetRetryBackoffForTesting(base::Hours(1), 2.0);
  auto fail_requests =
      base::MakeRefCounted<base::RefCountedData<std::atomic<bool>>>(
          std::in_place, true);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](scoped_refptr<base::RefCountedData<std::atomic<bool>>> fail_requests,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 &&
            fail_requests->data.load()) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      fail_requests));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});

  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));

  // `RetryReason` is emitted immediately when the retry is scheduled, before
  // recovery succeeds.
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kDownloadFailed, 1);
  histogram_tester.ExpectTotalCount(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 0);

  fail_requests->data.store(false);
  static_cast<network::NetworkConnectionTracker::NetworkConnectionObserver*>(
      api())
      ->OnConnectionChanged(net::NetworkChangeNotifier::CONNECTION_WIFI);
  WaitForUpdate();
  ASSERT_EQ(GetCommittedHash(), kConfigurationHash1);

  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kDownloadFailed, 1);
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 1, 1);
}

// Verifies that an invalid or non-dictionary JSON payload records
// `kInvalidJson` without prematurely emitting `RetryCountUntilSuccess` when
// clearing the store, and emits the retry count once valid JSON is committed.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       MetricsRecordedForInvalidJsonRetry) {
  base::HistogramTester histogram_tester;
  api()->SetRetryBackoffForTesting(base::Milliseconds(10), 1.0);
  auto remaining_invalid =
      base::MakeRefCounted<base::RefCountedData<std::atomic<int>>>(
          std::in_place, 1);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](scoped_refptr<base::RefCountedData<std::atomic<int>>>
             remaining_invalid,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 &&
            remaining_invalid->data.fetch_sub(1) > 0) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content(kConfigurationData3);
          response->set_content_type("text/plain");
          return response;
        }
        return nullptr;
      },
      remaining_invalid));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});

  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetCommittedHash() == kConfigurationHash1; }));

  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kInvalidJson, 1);
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 1, 1);
}

// Verifies that a LevelDB write failure records `kStoreWriteFailed` without
// emitting `RetryCountUntilSuccess`, and emits the retry count once a
// subsequent write succeeds.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       MetricsRecordedForStoreWriteRetry) {
  base::HistogramTester histogram_tester;
  scoped_refptr<base::SequencedTaskRunner> store_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  api()->SetStoreTaskRunnerForTesting(store_task_runner);
  api()->SetRetryBackoffForTesting(base::Hours(1), 2.0);
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});

  const base::FilePath origin_store_path =
      profile()
          ->GetPath()
          .AppendASCII("Managed Configuration")
          .AppendASCII(base::HexEncode(origin().Serialize()));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(origin_store_path.DirName()));
    ASSERT_TRUE(base::WriteFile(origin_store_path, "x"));
  }

  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));
  // Wait until the queued write completed and its reply ran on the UI thread.
  base::RunLoop store_flush_loop;
  store_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                      store_flush_loop.QuitClosure());
  store_flush_loop.Run();

  EXPECT_FALSE(GetCommittedHash());
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kStoreWriteFailed, 1);
  histogram_tester.ExpectTotalCount(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 0);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::DeleteFile(origin_store_path));
  }
  static_cast<network::NetworkConnectionTracker::NetworkConnectionObserver*>(
      api())
      ->OnConnectionChanged(net::NetworkChangeNotifier::CONNECTION_WIFI);
  WaitForUpdate();
  ASSERT_EQ(GetCommittedHash(), kConfigurationHash1);

  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kStoreWriteFailed, 1);
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 1, 1);
}

// Verifies that when a failing configuration sync is superseded by a new policy
// before recovering, the scheduled retry for the abandoned configuration is
// still recorded in `RetryReason`, while the new configuration starts with a
// fresh retry count of 0.
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       MetricsResetRetryCountWhenPolicyChangesAfterFailure) {
  base::HistogramTester histogram_tester;
  api()->SetRetryBackoffForTesting(base::Hours(1), 2.0);
  auto fail_url1 =
      base::MakeRefCounted<base::RefCountedData<std::atomic<bool>>>(
          std::in_place, true);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](scoped_refptr<base::RefCountedData<std::atomic<bool>>> fail_url1,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 &&
            fail_url1->data.load()) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      fail_url1));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});

  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !api()->IsFetchInProgressForTesting(origin()); }));

  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kDownloadFailed, 1);
  histogram_tester.ExpectTotalCount(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 0);

  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
  WaitForUpdate();
  ASSERT_EQ(GetCommittedHash(), kConfigurationHash2);

  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryReasonHistogram,
      ManagedConfigurationAPI::RetryReason::kDownloadFailed, 1);
  histogram_tester.ExpectUniqueSample(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 0, 1);
}

class ManagedConfigurationAPIRecoveryDisabledTest
    : public ManagedConfigurationAPITest {
 public:
  ManagedConfigurationAPIRecoveryDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kManagedConfigurationRecoverFromFailedFetches);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that when `kManagedConfigurationRecoverFromFailedFetches` is
// disabled, the API falls back to the legacy behavior (successful downloads and
// updates still work, while failed fetches are not automatically retried).
IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPIRecoveryDisabledTest,
                       FallbackToLegacyBehaviorWhenFlagDisabled) {
  base::HistogramTester histogram_tester;
  base::RunLoop url1_request_loop;
  std::atomic<bool> fail_url1{true};
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<bool>* fail_url1, base::RepeatingClosure on_url1_failed,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kConfigurationUrl1 && fail_url1->load()) {
          on_url1_failed.Run();
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_NOT_FOUND);
          return response;
        }
        return nullptr;
      },
      &fail_url1, url1_request_loop.QuitClosure()));
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});

  // Initial fetch of url1 fails; with the recovery flag disabled, re-adding an
  // observer or calling GetValues() does not trigger a re-sync for the same
  // hash.
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  url1_request_loop.Run();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));

  fail_url1.store(false);
  api()->RemoveObserver(this);
  api()->AddObserver(this);
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));

  // Updating policy to a new hash (url2, hash2) succeeds normally.
  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
  histogram_tester.ExpectTotalCount(
      ManagedConfigurationAPI::kRetryReasonHistogram, 0);
  histogram_tester.ExpectTotalCount(
      ManagedConfigurationAPI::kRetryCountUntilSuccessHistogram, 0);
}

#if BUILDFLAG(IS_CHROMEOS)

// Test the API behavior in the Guest Session.
class ManagedConfigurationAPIGuestTest
    : public ManagedConfigurationAPITestBase {
 public:
  ManagedConfigurationAPIGuestTest() {
    // Suppress the InProcessBrowserTest's default behavior of opening
    // about://blank pages and let the standard startup code open the
    // chrome://newtab page. The reason is that the navigator.managed API
    // doesn't work on about://blank pages.
    set_open_about_blank_on_browser_launch(false);
  }

  ~ManagedConfigurationAPIGuestTest() override = default;
  ManagedConfigurationAPIGuestTest(const ManagedConfigurationAPIGuestTest&) =
      delete;
  ManagedConfigurationAPIGuestTest& operator=(
      const ManagedConfigurationAPIGuestTest&) = delete;

 protected:
  // Returns the result of navigator.managed.getManagedConfiguration().
  content::EvalJsResult GetValuesFromJsApi(
      const std::vector<std::string>& keys) {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    if (!tab) {
      ADD_FAILURE() << "No tab active";
      return {base::Value(), std::string()};
    }
    base::ListValue keys_value;
    for (const auto& key : keys) {
      keys_value.Append(key);
    }
    return content::EvalJs(
        tab, content::JsReplace("navigator.managed.getManagedConfiguration($1)",
                                base::Value(std::move(keys_value))));
  }

 private:
  ash::GuestSessionMixin guest_session_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPIGuestTest, Disabled) {
  EXPECT_EQ(api(), nullptr);
  // The JS API should return an error (but not cause a crash - it's a
  // regression test for b/231283325).
  EXPECT_THAT(GetValuesFromJsApi({kKey1}),
              content::EvalJsResult::ErrorIs(
                  Eq("a JavaScript error: \"NotAllowedError: Service "
                     "connection error. This API is available only for "
                     "managed apps.\"\n")));
}

#endif  // BUILDFLAG(IS_CHROMEOS)
