// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <string_view>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using base::PassKey;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::IsEmpty;
using testing::Pair;
using testing::SizeIs;

// Encodes data about a bounce (the url, time of bounce, and
// whether it's stateful) for use when testing that the bounce is
// recorded by the DIPSBounceDetector.
using BounceTuple = std::tuple<GURL, base::Time, bool>;
// Encodes data about an event recorded by DIPS event (the url, time of
// bounce, and type of event) for use when testing that the event is recorded
// by the DIPSBounceDetector.
using EventTuple = std::tuple<GURL, base::Time, DIPSRecordedEvent>;

enum class UserGestureStatus { kNoUserGesture, kWithUserGesture };
constexpr auto kNoUserGesture = UserGestureStatus::kNoUserGesture;
constexpr auto kWithUserGesture = UserGestureStatus::kWithUserGesture;

// Returns a simplified URL representation for ease of comparison in tests.
// Just host+path.
std::string FormatURL(const GURL& url) {
  return base::StrCat({url.host_piece(), url.path_piece()});
}

void AppendRedirect(std::vector<std::string>* redirects,
                    const DIPSRedirectInfo& redirect,
                    const DIPSRedirectChainInfo& chain) {
  redirects->push_back(base::StringPrintf(
      "[%zu/%zu] %s -> %s (%s) -> %s", redirect.chain_index.value() + 1,
      chain.length, FormatURL(chain.initial_url.url).c_str(),
      FormatURL(redirect.url.url).c_str(),
      SiteDataAccessTypeToString(redirect.access_type).data(),
      FormatURL(chain.final_url.url).c_str()));
}

std::string URLForRedirectSourceId(ukm::TestUkmRecorder* ukm_recorder,
                                   ukm::SourceId source_id) {
  return FormatURL(ukm_recorder->GetSourceForSourceId(source_id)->url());
}

class FakeNavigation;

class TestBounceDetectorDelegate : public DIPSBounceDetectorDelegate {
 public:
  // DIPSBounceDetectorDelegate overrides:
  UrlAndSourceId GetLastCommittedURL() const override {
    return {committed_url_, source_id_};
  }

  void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                           DIPSRedirectChainInfoPtr chain) override {
    chain->cookie_mode = DIPSCookieMode::kBlock3PC;
    size_t redirect_index = chain->length - redirects.size();

    for (auto& redirect : redirects) {
      redirect->has_interaction = GetSiteHasInteraction(redirect->url.url);
      redirect->chain_id = chain->chain_id;
      redirect->chain_index = redirect_index;
      redirect->has_3pc_exception = false;
      DCHECK(redirect->access_type != SiteDataAccessType::kUnknown);
      AppendRedirect(&redirects_, *redirect, *chain);

      DIPSServiceImpl::HandleRedirectForTesting(
          *redirect, *chain,
          base::BindRepeating(&TestBounceDetectorDelegate::RecordBounce,
                              base::Unretained(this)));

      redirect_index++;
    }
  }

  // The version of this method in the DIPSWebContentsObserver checks
  // DIPSStorage for interactions and runs |callback| with the returned list of
  // sites without interaction. However, for the purpose of testing here, this
  // method just records the sites reported to it in |reported_sites_| without
  // filtering.
  void ReportRedirectors(const std::set<std::string> sites) override {
    if (sites.size() == 0) {
      return;
    }

    reported_sites_.push_back(base::JoinString(
        std::vector<std::string_view>(sites.begin(), sites.end()), ", "));
  }

  void OnSiteStorageAccessed(const GURL& first_party_url,
                             CookieOperation op,
                             bool http_cookie) override {}

  // Get the (committed) URL that the SourceId was generated for.
  const std::string& URLForSourceId(ukm::SourceId source_id) {
    return url_by_source_id_[source_id];
  }

  bool GetSiteHasInteraction(const GURL& url) {
    return site_has_interaction_[GetSiteForDIPS(url)];
  }

  void SetSiteHasInteraction(const GURL& url) {
    site_has_interaction_[GetSiteForDIPS(url)] = true;
  }

  void SetCommittedURL(PassKey<FakeNavigation>,
                       const GURL& url,
                       ukm::SourceId source_id) {
    committed_url_ = url;
    source_id_ = source_id;
    url_by_source_id_[source_id_] = FormatURL(url);
  }

  const std::set<BounceTuple>& GetRecordedBounces() const {
    return recorded_bounces_;
  }

  const std::vector<std::string>& GetReportedSites() const {
    return reported_sites_;
  }

  const std::vector<std::string>& redirects() const { return redirects_; }

  int stateful_bounce_count() const { return stateful_bounce_count_; }

 private:
  void RecordBounce(
      const GURL& url,
      bool has_3pc_exception,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> increment_bounce_callback) {
    recorded_bounces_.insert(std::make_tuple(url, time, stateful));
    if (stateful) {
      stateful_bounce_count_++;
    }
  }

  GURL committed_url_;
  ukm::SourceId source_id_;
  std::map<ukm::SourceId, std::string> url_by_source_id_;
  std::map<std::string, bool> site_has_interaction_;
  std::vector<std::string> redirects_;
  std::set<BounceTuple> recorded_bounces_;
  std::vector<std::string> reported_sites_;
  int stateful_bounce_count_ = 0;
};

class FakeNavigation : public DIPSNavigationHandle {
 public:
  FakeNavigation(DIPSBounceDetector* detector,
                 TestBounceDetectorDelegate* parent,
                 const GURL& url,
                 bool has_user_gesture)
      : detector_(detector),
        delegate_(parent),
        has_user_gesture_(has_user_gesture) {
    chain_.push_back(url);
    detector_->DidStartNavigation(this);
  }
  ~FakeNavigation() override { CHECK(finished_); }

  FakeNavigation& RedirectTo(std::string url) {
    chain_.emplace_back(std::move(url));
    detector_->DidRedirectNavigation(this);
    return *this;
  }

  FakeNavigation& AccessCookie(CookieOperation op) {
    detector_->OnServerCookiesAccessed(this, GetURL(), op);
    return *this;
  }

  void Finish(bool commit) {
    CHECK(!finished_);
    finished_ = true;
    has_committed_ = commit;
    if (commit) {
      previous_url_ = delegate_->GetLastCommittedURL().url;
      delegate_->SetCommittedURL(PassKey<FakeNavigation>(), GetURL(),
                                 GetNextPageUkmSourceId());
    }
    detector_->DidFinishNavigation(this);
  }

 private:
  // DIPSNavigationHandle overrides:
  bool HasUserGesture() const override { return has_user_gesture_; }
  ServerBounceDetectionState* GetServerState() override { return &state_; }
  bool HasCommitted() const override { return has_committed_; }
  ukm::SourceId GetNextPageUkmSourceId() override { return next_source_id_; }
  const GURL& GetPreviousPrimaryMainFrameURL() const override {
    return previous_url_;
  }
  // TODO (crbug.com/1442658): Add support for simulating opening a link in a
  // new tab.
  const GURL GetInitiator() const override {
    return previous_url_.is_empty() ? GURL("about:blank") : previous_url_;
  }
  const std::vector<GURL>& GetRedirectChain() const override { return chain_; }
  bool WasResponseCached() override { return false; }
  int GetHTTPResponseCode() override { return net::HTTP_FOUND; }

  raw_ptr<DIPSBounceDetector> detector_;
  raw_ptr<TestBounceDetectorDelegate> delegate_;
  const bool has_user_gesture_;
  bool finished_ = false;
  const ukm::SourceId next_source_id_ = ukm::AssignNewSourceId();

  ServerBounceDetectionState state_;
  bool has_committed_ = false;
  GURL previous_url_;
  std::vector<GURL> chain_;
};

class DIPSBounceDetectorTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FakeNavigation StartNavigation(const std::string& url,
                                 UserGestureStatus status) {
    return FakeNavigation(&detector_, &delegate_, GURL(url),
                          status == kWithUserGesture);
  }

  void NavigateTo(const std::string& url, UserGestureStatus status) {
    StartNavigation(url, status).Finish(true);
  }

  void AccessClientCookie(CookieOperation op) {
    detector_.OnClientSiteDataAccessed(delegate_.GetLastCommittedURL().url, op);
  }

  void LateAccessClientCookie(const std::string& url, CookieOperation op) {
    if (!detector_.AddLateCookieAccess(GURL(url), op)) {
      detector_.OnClientSiteDataAccessed(GURL(url), op);
    }
  }

  void ActivatePage() { detector_.OnUserActivation(); }
  void TriggerWebAuthnAssertionRequestSucceeded() {
    detector_.WebAuthnAssertionRequestSucceeded();
  }

  const DIPSRedirectContext& CommittedRedirectContext() {
    return detector_.CommittedRedirectContext();
  }

  void AdvanceDIPSTime(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
    task_environment_.RunUntilIdle();
  }

  // Advances the mocked clock by `features::kDIPSClientBounceDetectionTimeout`
  // to trigger the closure of the pending redirect chain.
  void EndPendingRedirectChain() {
    AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get());
  }

  const std::string& URLForNavigationSourceId(ukm::SourceId source_id) {
    return delegate_.URLForSourceId(source_id);
  }

  void SetSiteHasInteraction(const std::string& url) {
    return delegate_.SetSiteHasInteraction(GURL(url));
  }

  std::set<BounceTuple> GetRecordedBounces() const {
    return delegate_.GetRecordedBounces();
  }

  BounceTuple MakeBounceTuple(const std::string& url,
                              const base::Time& time,
                              bool stateful) {
    return std::make_tuple(GURL(url), time, stateful);
  }

  EventTuple MakeEventTuple(const std::string& url,
                            const base::Time& time,
                            DIPSRecordedEvent event) {
    return std::make_tuple(GURL(url), time, event);
  }

  const std::vector<std::string>& GetReportedSites() const {
    return delegate_.GetReportedSites();
  }

  base::Time GetCurrentTime() {
    return task_environment_.GetMockClock()->Now();
  }

  const std::vector<std::string>& redirects() const {
    return delegate_.redirects();
  }

  int stateful_bounce_count() const {
    return delegate_.stateful_bounce_count();
  }

 private:
  TestBounceDetectorDelegate delegate_;
  DIPSBounceDetector detector_{&delegate_, task_environment_.GetMockTickClock(),
                               task_environment_.GetMockClock()};
};

// Ensures that for every navigation, a client redirect occurring before
// `dips:kClientBounceDetectionTimeout` is considered a bounce whilst leaving
// Server redirects unaffected.
TEST_F(DIPSBounceDetectorTest,
       DetectStatefulRedirects_Before_ClientBounceDetectionTimeout) {
  NavigateTo("http://a.test", kWithUserGesture);
  auto mocked_bounce_time_1 = GetCurrentTime();
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .Finish(true);
  AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get() -
                  base::Seconds(1));
  auto mocked_bounce_time_2 = GetCurrentTime();
  StartNavigation("http://e.test", kNoUserGesture)
      .RedirectTo("http://f.test")
      .RedirectTo("http://g.test")
      .Finish(true);
  AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get() -
                  base::Seconds(1));
  auto mocked_bounce_time_3 = GetCurrentTime();
  StartNavigation("http://h.test", kWithUserGesture)
      .RedirectTo("http://i.test")
      .RedirectTo("http://j.test")
      .Finish(true);

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/5] a.test/ -> b.test/ (None) -> g.test/"),
                               ("[2/5] a.test/ -> c.test/ (None) -> g.test/"),
                               ("[3/5] a.test/ -> d.test/ (None) -> g.test/"),
                               ("[4/5] a.test/ -> e.test/ (None) -> g.test/"),
                               ("[5/5] a.test/ -> f.test/ (None) -> g.test/"),
                               ("[1/2] g.test/ -> h.test/ (None) -> j.test/"),
                               ("[2/2] g.test/ -> i.test/ (None) -> j.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://f.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://h.test", mocked_bounce_time_3,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://i.test", mocked_bounce_time_3,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

// Ensures that for every navigation, a client redirect occurring after
// `dips:kClientBounceDetectionTimeout` is not considered a bounce whilst server
// redirects are unaffected.
TEST_F(DIPSBounceDetectorTest,
       DetectStatefulRedirects_After_ClientBounceDetectionTimeout) {
  NavigateTo("http://a.test", kWithUserGesture);
  AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get());
  auto mocked_bounce_time_1 = GetCurrentTime();
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .Finish(true);
  AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get());
  auto mocked_bounce_time_2 = GetCurrentTime();
  StartNavigation("http://e.test", kNoUserGesture)
      .RedirectTo("http://f.test")
      .RedirectTo("http://g.test")
      .Finish(true);

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/2] a.test/ -> b.test/ (None) -> d.test/"),
                               ("[2/2] a.test/ -> c.test/ (None) -> d.test/"),
                               ("[1/2] d.test/ -> e.test/ (None) -> g.test/"),
                               ("[2/2] d.test/ -> f.test/ (None) -> g.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://f.test", mocked_bounce_time_2,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Server) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .AccessCookie(CookieOperation::kRead)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(),
              testing::ElementsAre(
                  ("[1/3] a.test/ -> b.test/ (Read) -> e.test/"),
                  ("[2/3] a.test/ -> c.test/ (Write) -> e.test/"),
                  ("[3/3] a.test/ -> d.test/ (ReadWrite) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/true),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 2);
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Server_OnStartUp) {
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .AccessCookie(CookieOperation::kRead)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/3] blank -> b.test/ (Read) -> e.test/"),
                           ("[2/3] blank -> c.test/ (Write) -> e.test/"),
                           ("[3/3] blank -> d.test/ (ReadWrite) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/true),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/true)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Server_LateNotification) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .RedirectTo("http://e.test")
      .Finish(true);

  LateAccessClientCookie("http://b.test", CookieOperation::kChange);
  LateAccessClientCookie("http://c.test", CookieOperation::kRead);
  LateAccessClientCookie("http://d.test", CookieOperation::kChange);
  LateAccessClientCookie("http://e.test", CookieOperation::kRead);
  LateAccessClientCookie("http://e.test", CookieOperation::kChange);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/3] a.test/ -> b.test/ (ReadWrite) -> e.test/"),
                           ("[2/3] a.test/ -> c.test/ (Read) -> e.test/"),
                           ("[3/3] a.test/ -> d.test/ (Write) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/true),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 2);
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get() -
                  base::Seconds(1));
  NavigateTo("http://c.test", kNoUserGesture);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/1] a.test/ -> b.test/ (None) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://b.test", mocked_bounce_time, /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client_OnStartUp) {
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kRead);
  AccessClientCookie(CookieOperation::kChange);
  AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get() -
                  base::Seconds(1));
  NavigateTo("http://b.test", kNoUserGesture);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/1] blank -> a.test/ (ReadWrite) -> b.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://a.test", mocked_bounce_time, /*stateful=*/true)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client_MergeCookies) {
  NavigateTo("http://a.test", kWithUserGesture);
  // Server cookie read:
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .Finish(true);
  // Client cookie write:
  // NOTE: This navigation's client redirect will always be considered a bounce
  // because of the (frozen) mocked clock.
  AccessClientCookie(CookieOperation::kChange);
  NavigateTo("http://c.test", kNoUserGesture);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  // Redirect cookie access is reported as ReadWrite.
  EXPECT_THAT(redirects(),
              testing::ElementsAre(
                  ("[1/1] a.test/ -> b.test/ (ReadWrite) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://b.test", mocked_bounce_time, /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 1);
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_ServerClientServer) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .Finish(true);
  StartNavigation("http://d.test", kNoUserGesture)
      .RedirectTo("http://e.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/3] a.test/ -> b.test/ (None) -> e.test/"),
                               ("[2/3] a.test/ -> c.test/ (None) -> e.test/"),
                               ("[3/3] a.test/ -> d.test/ (None) -> e.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Server_Uncommitted) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .Finish(false);
  // Because the previous navigation didn't commit, the following chain still
  // starts from http://a.test/.
  StartNavigation("http://e.test", kWithUserGesture)
      .RedirectTo("http://f.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/3] a.test/ -> b.test/ (None) -> a.test/"),
                               ("[2/3] a.test/ -> c.test/ (None) -> a.test/"),
                               ("[3/3] a.test/ -> d.test/ (None) -> a.test/"),
                               ("[1/1] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client_Uncommitted) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  StartNavigation("http://c.test", kNoUserGesture)
      .RedirectTo("http://d.test")
      .Finish(false);
  // Because the previous navigation didn't commit, the following chain still
  // starts from http://a.test/.
  StartNavigation("http://e.test", kNoUserGesture)
      .RedirectTo("http://f.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/3] a.test/ -> b.test/ (None) -> b.test/"),
                               ("[2/3] a.test/ -> c.test/ (None) -> b.test/"),
                               ("[3/3] a.test/ -> d.test/ (None) -> b.test/"),
                               ("[1/2] a.test/ -> b.test/ (None) -> f.test/"),
                               ("[2/2] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(DIPSBounceDetectorTest,
       ReportRedirectorsInChain_OnEachFinishedNavigation) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "c.test"));
  // Access cookies on d.test.
  AccessClientCookie(CookieOperation::kChange);

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test.
  StartNavigation("http://e.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test", "c.test", "d.test, e.test"));
}

TEST_F(DIPSBounceDetectorTest,
       ReportRedirectorsInChain_IncludingUncommittedNavigations) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Start a redirect chain that doesn't commit.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(false);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test, c.test, d.test"));

  // Because the previous navigation didn't commit, the following chain still
  // starts from http://a.test/.
  StartNavigation("http://e.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test, c.test, d.test", "e.test"));
}

TEST_F(DIPSBounceDetectorTest,
       ReportRedirectorsInChain_OmitNonStatefulRedirects) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test (which doesn't access cookies).
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test (which doesn't
  // access cookies).
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test.
  StartNavigation("http://e.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "e.test"));
}

// This test verifies that sites in a redirect chain that are the same as the
// starting site (i.e., last site before the redirect chain started) are not
// reported.
TEST_F(DIPSBounceDetectorTest,
       ReportRedirectorsInChain_OmitSitesMatchingStartSite) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // a.test, which S-redirects to c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://a.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to a.test.
  NavigateTo("http://a.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "c.test"));
  // Access cookies via JS on a.test.
  AccessClientCookie(CookieOperation::kChange);

  // Navigate without a click (i.e. by C-redirecting) to d.test, which
  // S-redirects to e.test, which S-redirects to f.test.
  StartNavigation("http://d.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test", "c.test", "d.test, e.test"));
}

// This test verifies that sites in a (server) redirect chain that are the same
// as the ending site of a navigation are not reported.
TEST_F(DIPSBounceDetectorTest,
       ReportRedirectorsInChain_OmitSitesMatchingEndSite) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test, which S-redirects to c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "c.test"));
  // Access cookies via JS on d.test.
  AccessClientCookie(CookieOperation::kChange);

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test, which S-redirects to e.test.
  StartNavigation("http://e.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test", "c.test", "d.test, f.test"));
}

TEST_F(DIPSBounceDetectorTest,
       ReportRedirectorsInChain_OmitSitesMatchingEndSite_Uncommitted) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test, which S-redirects to c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(false);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test, c.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  // NOTE: Because the previous navigation didn't commit, the chain still
  // starts from http://a.test/.
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test, c.test", "a.test"));
}

const std::vector<std::string>& GetAllRedirectMetrics() {
  static const std::vector<std::string> kAllRedirectMetrics = {
      // clang-format off
      "ClientBounceDelay",
      "CookieAccessType",
      "HasStickyActivation",
      "InitialAndFinalSitesSame",
      "RedirectAndFinalSiteSame",
      "RedirectAndInitialSiteSame",
      "RedirectChainIndex",
      "RedirectChainLength",
      "RedirectType",
      "SiteEngagementLevel",
      "WebAuthnAssertionRequestSucceeded",
      // clang-format on
  };
  return kAllRedirectMetrics;
}

TEST_F(DIPSBounceDetectorTest, Histograms_UMA) {
  base::HistogramTester histograms;

  SetSiteHasInteraction("http://b.test");

  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(base::Seconds(3));
  AccessClientCookie(CookieOperation::kRead);
  StartNavigation("http://c.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .Finish(true);
  EndPendingRedirectChain();

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Privacy.DIPS.BounceCategoryClient.Block3PC"] = 1;
  expected_counts["Privacy.DIPS.BounceCategoryServer.Block3PC"] = 1;
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("Privacy.DIPS.BounceCategory"),
              testing::ContainerEq(expected_counts));
  // Verify the proper values were recorded. b.test has user engagement and read
  // cookies, while c.test has no user engagement and wrote cookies.
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryClient.Block3PC"),
      testing::ElementsAre(
          // b.test
          Bucket((int)RedirectCategory::kReadCookies_HasEngagement, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryServer.Block3PC"),
      testing::ElementsAre(
          // c.test
          Bucket((int)RedirectCategory::kWriteCookies_NoEngagement, 1)));

  // Verify the time-to-bounce metric was recorded for the client bounce.
  histograms.ExpectBucketCount(
      "Privacy.DIPS.TimeFromNavigationCommitToClientBounce",
      static_cast<base::HistogramBase::Sample>(
          base::Seconds(3).InMilliseconds()),
      /*expected_count=*/1);
}

TEST_F(DIPSBounceDetectorTest, Histograms_UKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetSiteHasInteraction("http://c.test");

  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(base::Seconds(2));
  AccessClientCookie(CookieOperation::kRead);
  TriggerWebAuthnAssertionRequestSucceeded();
  StartNavigation("http://c.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .Finish(true);

  EndPendingRedirectChain();

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
      ukm_recorder.GetEntries("DIPS.Redirect", GetAllRedirectMetrics());
  ASSERT_EQ(2u, ukm_entries.size());

  EXPECT_THAT(URLForNavigationSourceId(ukm_entries[0].source_id),
              Eq("b.test/"));
  EXPECT_THAT(
      ukm_entries[0].metrics,
      ElementsAre(Pair("ClientBounceDelay", 2),
                  Pair("CookieAccessType", (int)SiteDataAccessType::kRead),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 0), Pair("RedirectChainLength", 2),
                  Pair("RedirectType", (int)DIPSRedirectType::kClient),
                  Pair("SiteEngagementLevel", 0),
                  Pair("WebAuthnAssertionRequestSucceeded", true)));

  EXPECT_THAT(URLForRedirectSourceId(&ukm_recorder, ukm_entries[1].source_id),
              Eq("c.test/"));
  EXPECT_THAT(
      ukm_entries[1].metrics,
      ElementsAre(Pair("ClientBounceDelay", 0),
                  Pair("CookieAccessType", (int)SiteDataAccessType::kWrite),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 1), Pair("RedirectChainLength", 2),
                  Pair("RedirectType", (int)DIPSRedirectType::kServer),
                  Pair("SiteEngagementLevel", 1),
                  Pair("WebAuthnAssertionRequestSucceeded", false)));
}

TEST_F(DIPSBounceDetectorTest, SiteHadUserActivation) {
  NavigateTo("http://a.test", kWithUserGesture);
  ActivatePage();
  AdvanceDIPSTime(features::kDIPSClientBounceDetectionTimeout.Get() +
                  base::Seconds(1));

  StartNavigation("http://b.test", kNoUserGesture)
      .RedirectTo("http://c.test")
      .Finish(/*commit=*/true);
  ActivatePage();
  NavigateTo("http://d.test", kNoUserGesture);

  // Expect one initial URL (a.test) and two redirects (b.test, c.test).
  EXPECT_EQ(CommittedRedirectContext().GetInitialURLForTesting(),
            GURL("http://a.test"));
  EXPECT_EQ(CommittedRedirectContext().GetRedirectChainLength(), 2u);

  EXPECT_TRUE(CommittedRedirectContext().SiteHadUserActivation("a.test"));
  EXPECT_FALSE(CommittedRedirectContext().SiteHadUserActivation("b.test"));
  EXPECT_TRUE(CommittedRedirectContext().SiteHadUserActivation("c.test"));
  EXPECT_FALSE(CommittedRedirectContext().SiteHadUserActivation("d.test"));
}

TEST_F(DIPSBounceDetectorTest, ClientCookieAccessDuringNavigation) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);

  auto nav = StartNavigation("http://c.test", kNoUserGesture);
  // b.test accesses cookies after the navigation started.
  AccessClientCookie(CookieOperation::kChange);
  nav.Finish(true);

  EndPendingRedirectChain();

  // The b.test bounce is considered stateful.
  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/1] a.test/ -> b.test/ (Write) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::ElementsAre(testing::FieldsAre(
                  GURL("http://b.test"), testing::_, /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 1);
}

using ChainPair =
    std::pair<DIPSRedirectChainInfoPtr, std::vector<DIPSRedirectInfoPtr>>;

void AppendChainPair(std::vector<ChainPair>& vec,
                     std::vector<DIPSRedirectInfoPtr> redirects,
                     DIPSRedirectChainInfoPtr chain) {
  vec.emplace_back(std::move(chain), std::move(redirects));
}

std::vector<DIPSRedirectInfoPtr> MakeServerRedirects(
    std::vector<std::string> urls,
    SiteDataAccessType access_type = SiteDataAccessType::kReadWrite) {
  std::vector<DIPSRedirectInfoPtr> redirects;
  for (const auto& url : urls) {
    redirects.push_back(std::make_unique<DIPSRedirectInfo>(
        /*url=*/MakeUrlAndId(url),
        /*redirect_type=*/DIPSRedirectType::kServer,
        /*access_type=*/access_type,
        /*time=*/base::Time::Now(),
        /*was_response_cached=*/false,
        /*response_code=*/net::HTTP_FOUND,
        /*server_bounce_delay=*/base::TimeDelta()));
  }
  return redirects;
}

DIPSRedirectInfoPtr MakeClientRedirect(
    std::string url,
    SiteDataAccessType access_type = SiteDataAccessType::kReadWrite,
    bool has_sticky_activation = false) {
  return std::make_unique<DIPSRedirectInfo>(
      /*url=*/MakeUrlAndId(url),
      /*redirect_type=*/DIPSRedirectType::kClient,
      /*access_type=*/access_type,
      /*time=*/base::Time::Now(),
      /*client_bounce_delay=*/base::Seconds(1),
      /*has_sticky_activation=*/has_sticky_activation,
      /*web_authn_assertion_request_succeeded*/ false);
}

MATCHER_P(HasUrl, url, "") {
  *result_listener << "whose url is " << arg->url.url;
  return ExplainMatchResult(Eq(url), arg->url.url, result_listener);
}

MATCHER_P(HasRedirectType, redirect_type, "") {
  *result_listener << "whose redirect_type is "
                   << DIPSRedirectTypeToString(arg->redirect_type);
  return ExplainMatchResult(Eq(redirect_type), arg->redirect_type,
                            result_listener);
}

MATCHER_P(HasSiteDataAccessType, access_type, "") {
  *result_listener << "whose access_type is "
                   << SiteDataAccessTypeToString(arg->access_type);
  return ExplainMatchResult(Eq(access_type), arg->access_type, result_listener);
}

MATCHER_P(HasInitialUrl, url, "") {
  *result_listener << "whose initial_url is " << arg->initial_url.url;
  return ExplainMatchResult(Eq(url), arg->initial_url.url, result_listener);
}

MATCHER_P(HasFinalUrl, url, "") {
  *result_listener << "whose final_url is " << arg->final_url.url;
  return ExplainMatchResult(Eq(url), arg->final_url.url, result_listener);
}

MATCHER_P(HasLength, length, "") {
  *result_listener << "whose length is " << arg->length;
  return ExplainMatchResult(Eq(length), arg->length, result_listener);
}

TEST(DIPSRedirectContextTest, OneAppend) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      MakeUrlAndId("http://a.test/"),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      MakeUrlAndId("http://d.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.EndChain(MakeUrlAndId("http://d.test/"), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));
}

TEST(DIPSRedirectContextTest, TwoAppends_NoClientRedirect) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      MakeUrlAndId("http://a.test/"),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      MakeUrlAndId("http://d.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(MakeUrlAndId("http://d.test/"),
                          MakeServerRedirects({"http://e.test/"}),
                          MakeUrlAndId("http://f.test/"), false);
  ASSERT_EQ(chains.size(), 1u);
  context.EndChain(MakeUrlAndId("http://f.test/"), false);

  ASSERT_EQ(chains.size(), 2u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));

  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://d.test/"),
                    HasFinalUrl("http://f.test/"), HasLength(1u)));
  EXPECT_THAT(chains[1].second, ElementsAre(HasUrl("http://e.test/")));
}

TEST(DIPSRedirectContextTest, TwoAppends_WithClientRedirect) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      MakeUrlAndId("http://a.test/"),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      MakeUrlAndId("http://d.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      MakeClientRedirect("http://d.test/"),
      MakeServerRedirects({"http://e.test/", "http://f.test/"}),
      MakeUrlAndId("http://g.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.EndChain(MakeUrlAndId("http://g.test/"), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://g.test/"), HasLength(5u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(AllOf(HasUrl("http://b.test/"),
                                HasRedirectType(DIPSRedirectType::kServer)),
                          AllOf(HasUrl("http://c.test/"),
                                HasRedirectType(DIPSRedirectType::kServer)),
                          AllOf(HasUrl("http://d.test/"),
                                HasRedirectType(DIPSRedirectType::kClient)),
                          AllOf(HasUrl("http://e.test/"),
                                HasRedirectType(DIPSRedirectType::kServer)),
                          AllOf(HasUrl("http://f.test/"),
                                HasRedirectType(DIPSRedirectType::kServer))));
}

TEST(DIPSRedirectContextTest, OnlyClientRedirects) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(MakeUrlAndId("http://a.test/"), {},
                          MakeUrlAndId("http://b.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(MakeClientRedirect("http://b.test/"), {},
                          MakeUrlAndId("http://c.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(MakeClientRedirect("http://c.test/"), {},
                          MakeUrlAndId("http://d.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.EndChain(MakeUrlAndId("http://d.test"), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));
}

TEST(DIPSRedirectContextTest, OverflowMaxChain_TrimsFromFront) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  context.AppendCommitted(MakeUrlAndId("http://a.test/"), {},
                          MakeUrlAndId("http://c.test/"), false);
  for (size_t ind = 0; ind < kDIPSRedirectChainMax; ind++) {
    std::string redirect_url =
        base::StrCat({"http://", base::NumberToString(ind), ".test/"});
    context.AppendCommitted(MakeClientRedirect(redirect_url), {},
                            MakeUrlAndId("http://c.test/"), false);
  }
  // Each redirect was added to the chain.
  ASSERT_EQ(context.size(), kDIPSRedirectChainMax);
  ASSERT_EQ(chains.size(), 0u);

  // The next redirect overflows the chain and evicts the first one.
  context.AppendCommitted(MakeClientRedirect("http://b.test/"), {},
                          MakeUrlAndId("http://c.test/"), false);
  ASSERT_EQ(context.size(), kDIPSRedirectChainMax);
  ASSERT_EQ(chains.size(), 1u);
  context.EndChain(MakeUrlAndId("http://c.test/"), false);

  // Expect two chains handled: one partial chain with the dropped redirect, and
  // one with the other redirects.
  ASSERT_EQ(chains.size(), 2u);
  EXPECT_THAT(chains[0].first, AllOf(HasInitialUrl("http://a.test/"),
                                     HasLength(kDIPSRedirectChainMax + 1)));
  ASSERT_THAT(chains[0].second, SizeIs(1));
  EXPECT_THAT(chains[0].second.at(0),
              AllOf(HasUrl("http://0.test/"),
                    HasRedirectType(DIPSRedirectType::kClient)));

  // DIPSRedirectChainInfo.length is computed from DIPSRedirectInfo.index, so it
  // includes the length of the partial chains.
  EXPECT_THAT(chains[1].first, AllOf(HasInitialUrl("http://a.test/"),
                                     HasFinalUrl("http://c.test/"),
                                     HasLength(kDIPSRedirectChainMax + 1)));
  ASSERT_THAT(chains[1].second, SizeIs(kDIPSRedirectChainMax));
  // Check that the first redirect in the chain is the second that was added in
  // the setup.
  EXPECT_THAT(chains[1].second.at(0),
              AllOf(HasUrl("http://1.test/"),
                    HasRedirectType(DIPSRedirectType::kClient)));
  // Check the last redirect in the full chain.
  EXPECT_THAT(chains[1].second.back(),
              AllOf(HasUrl("http://b.test/"),
                    HasRedirectType(DIPSRedirectType::kClient)));
}

TEST(DIPSRedirectContextTest, Uncommitted_NoClientRedirects) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      MakeUrlAndId("http://a.test/"),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      MakeUrlAndId("http://d.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  context.HandleUncommitted(
      MakeUrlAndId("http://d.test/"),
      MakeServerRedirects({"http://e.test/", "http://f.test/"}));
  ASSERT_EQ(chains.size(), 1u);
  context.AppendCommitted(MakeUrlAndId("http://h.test/"),
                          MakeServerRedirects({"http://i.test/"}),
                          MakeUrlAndId("http://j.test/"), false);
  ASSERT_EQ(chains.size(), 2u);
  context.EndChain(MakeUrlAndId("http://j.test/"), false);

  ASSERT_EQ(chains.size(), 3u);
  // First, the uncommitted (middle) chain.
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://d.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://e.test/"), HasUrl("http://f.test/")));
  // Then the initially-started chain.
  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://h.test/"), HasLength(2u)));
  EXPECT_THAT(chains[1].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));
  // Then the last chain.
  EXPECT_THAT(chains[2].first,
              AllOf(HasInitialUrl("http://h.test/"),
                    HasFinalUrl("http://j.test/"), HasLength(1u)));
  EXPECT_THAT(chains[2].second, ElementsAre(HasUrl("http://i.test/")));
}

TEST(DIPSRedirectContextTest, Uncommitted_IncludingClientRedirects) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      MakeUrlAndId("http://a.test/"),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      MakeUrlAndId("http://d.test/"), false);
  ASSERT_EQ(chains.size(), 0u);
  // Uncommitted navigation:
  context.HandleUncommitted(
      MakeClientRedirect("http://d.test/"),
      MakeServerRedirects({"http://e.test/", "http://f.test/"}));
  ASSERT_EQ(chains.size(), 1u);
  context.AppendCommitted(MakeClientRedirect("http://h.test/"),
                          MakeServerRedirects({"http://i.test/"}),
                          MakeUrlAndId("http://j.test/"), false);
  ASSERT_EQ(chains.size(), 1u);
  context.EndChain(MakeUrlAndId("http://j.test/"), false);

  ASSERT_EQ(chains.size(), 2u);
  // First, the uncommitted chain. The overall length includes the
  // already-committed part of the chain (2 redirects, starting from a.test)
  // plus the uncommitted part (3 redirects, starting from d.test).
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(5u)));
  // But only the 3 uncommitted redirects are included in the vector.
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://d.test/"), HasUrl("http://e.test/"),
                          HasUrl("http://f.test/")));
  // Then the initially-started chain.
  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://j.test/"), HasLength(4u)));
  // Committed chains include all redirects in the vector.
  EXPECT_THAT(chains[1].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/"),
                          HasUrl("http://h.test/"), HasUrl("http://i.test/")));
}

TEST(DIPSRedirectContextTest, NoRedirects) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);

  context.AppendCommitted(MakeUrlAndId("http://a.test/"), {},
                          MakeUrlAndId("http://b.test/"), false);
  ASSERT_EQ(chains.size(), 0u);

  context.AppendCommitted(MakeUrlAndId("http://b.test/"), {},
                          MakeUrlAndId("http://c.test/"), false);
  ASSERT_EQ(chains.size(), 1u);

  context.EndChain(MakeUrlAndId("http://e.test/"), false);
  ASSERT_EQ(chains.size(), 2u);

  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://b.test/"), HasLength(0u)));
  EXPECT_THAT(chains[0].second, IsEmpty());

  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://b.test/"),
                    HasFinalUrl("http://e.test/"), HasLength(0u)));
  EXPECT_THAT(chains[1].second, IsEmpty());
}

TEST(DIPSRedirectContextTest, AddLateCookieAccess) {
  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(
      MakeUrlAndId("http://a.test/"),
      MakeServerRedirects(
          {"http://b.test/", "http://c.test/", "http://d.test/"},
          SiteDataAccessType::kNone),
      MakeUrlAndId("http://e.test/"), false);

  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://d.test/"),
                                          CookieOperation::kChange));
  // Update c.test even though it preceded d.test:
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://c.test/"),
                                          CookieOperation::kRead));

  context.AppendCommitted(
      MakeClientRedirect("http://e.test/", SiteDataAccessType::kNone),
      MakeServerRedirects({"http://f.test/", "http://g.test/"},
                          SiteDataAccessType::kRead),
      MakeUrlAndId("http://h.test/"), false);

  context.AppendCommitted(
      MakeClientRedirect("http://h.test/", SiteDataAccessType::kNone),
      MakeServerRedirects({"http://i.test/"}, SiteDataAccessType::kRead),
      MakeUrlAndId("http://j.test/"), false);

  // Since kMaxLookback=5, AddLateCookieAccess() can attribute late accesses to
  // the last 5 redirects:
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://i.test/"),
                                          CookieOperation::kRead));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://h.test/"),
                                          CookieOperation::kRead));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://g.test/"),
                                          CookieOperation::kChange));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://f.test/"),
                                          CookieOperation::kRead));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://e.test/"),
                                          CookieOperation::kRead));
  // But it will fail to update d.test since it's too far back in the chain.
  EXPECT_FALSE(context.AddLateCookieAccess(GURL("http://d.test/"),
                                           CookieOperation::kRead));

  context.EndChain(MakeUrlAndId("http://j.test/"), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://j.test/"), HasLength(8u)));
  EXPECT_THAT(
      chains[0].second,
      ElementsAre(AllOf(HasUrl("http://b.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kNone)),
                  AllOf(HasUrl("http://c.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kRead)),
                  AllOf(HasUrl("http://d.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kWrite)),
                  AllOf(HasUrl("http://e.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kRead)),
                  AllOf(HasUrl("http://f.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kRead)),
                  AllOf(HasUrl("http://g.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kReadWrite)),
                  AllOf(HasUrl("http://h.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kRead)),
                  AllOf(HasUrl("http://i.test/"),
                        HasSiteDataAccessType(SiteDataAccessType::kRead))));
}

TEST(DIPSRedirectContextTest, GetRedirectHeuristicURLs_NoRequirements) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      content_settings::features::kTpcdHeuristicsGrants,
      {{"TpcdRedirectHeuristicRequireABAFlow", "false"}});

  UrlAndSourceId first_party_url = MakeUrlAndId("http://a.test/");
  UrlAndSourceId current_interaction_url = MakeUrlAndId("http://b.test/");
  GURL no_current_interaction_url("http://c.test/");

  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(first_party_url,
                          {MakeServerRedirects({"http://c.test"})},
                          current_interaction_url, false);
  context.AppendCommitted(
      MakeClientRedirect("http://b.test/", SiteDataAccessType::kNone,
                         /*has_sticky_activation=*/true),
      {}, first_party_url, false);

  ASSERT_EQ(context.size(), 2u);

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction = context.GetRedirectHeuristicURLs(
          first_party_url.url, std::nullopt,
          /*require_current_interaction=*/false);
  EXPECT_THAT(
      sites_to_url_and_current_interaction,
      testing::UnorderedElementsAre(
          std::pair<std::string, std::pair<GURL, bool>>(
              "b.test", std::make_pair(current_interaction_url.url, true)),
          std::pair<std::string, std::pair<GURL, bool>>(
              "c.test", std::make_pair(no_current_interaction_url, false))));
}

TEST(DIPSRedirectContextTest, GetRedirectHeuristicURLs_RequireABAFlow) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      content_settings::features::kTpcdHeuristicsGrants,
      {{"TpcdRedirectHeuristicRequireABAFlow", "true"}});

  UrlAndSourceId first_party_url = MakeUrlAndId("http://a.test/");
  GURL aba_url("http://b.test/");
  GURL no_aba_url("http://c.test/");

  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(
      first_party_url,
      {MakeServerRedirects({"http://b.test", "http://c.test"})},
      first_party_url, false);

  ASSERT_EQ(context.size(), 2u);

  std::set<std::string> allowed_sites = {GetSiteForDIPS(aba_url)};

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction = context.GetRedirectHeuristicURLs(
          first_party_url.url, allowed_sites,
          /*require_current_interaction=*/false);
  EXPECT_THAT(sites_to_url_and_current_interaction,
              testing::UnorderedElementsAre(
                  std::pair<std::string, std::pair<GURL, bool>>(
                      "b.test", std::make_pair(aba_url, false))));
}

TEST(DIPSRedirectContextTest,
     GetRedirectHeuristicURLs_RequireCurrentInteraction) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      content_settings::features::kTpcdHeuristicsGrants,
      {{"TpcdRedirectHeuristicRequireABAFlow", "false"}});

  UrlAndSourceId first_party_url = MakeUrlAndId("http://a.test/");
  UrlAndSourceId current_interaction_url = MakeUrlAndId("http://b.test/");
  GURL no_current_interaction_url("http://c.test/");

  std::vector<ChainPair> chains;
  DIPSRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      UrlAndSourceId(),
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(first_party_url,
                          {MakeServerRedirects({"http://c.test"})},
                          current_interaction_url, false);
  context.AppendCommitted(
      MakeClientRedirect("http://b.test/", SiteDataAccessType::kNone,
                         /*has_sticky_activation=*/true),
      {}, first_party_url, false);

  ASSERT_EQ(context.size(), 2u);

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction = context.GetRedirectHeuristicURLs(
          first_party_url.url, std::nullopt,
          /*require_current_interaction=*/true);
  EXPECT_THAT(
      sites_to_url_and_current_interaction,
      testing::UnorderedElementsAre(
          std::pair<std::string, std::pair<GURL, bool>>(
              "b.test", std::make_pair(current_interaction_url.url, true))));
}
