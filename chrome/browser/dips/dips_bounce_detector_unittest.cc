// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using base::PassKey;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::Pair;

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
      "[%d/%d] %s -> %s (%s) -> %s", redirect.index + 1, chain.length,
      FormatURL(chain.initial_url).c_str(), FormatURL(redirect.url).c_str(),
      CookieAccessTypeToString(redirect.access_type).data(),
      FormatURL(chain.final_url).c_str()));
}

std::string URLForRedirectSourceId(ukm::TestUkmRecorder* ukm_recorder,
                                   ukm::SourceId source_id) {
  return FormatURL(ukm_recorder->GetSourceForSourceId(source_id)->url());
}

class FakeNavigation;

class TestBounceDetectorDelegate : public DIPSBounceDetectorDelegate {
 public:
  // DIPSBounceDetectorDelegate overrides:
  const GURL& GetLastCommittedURL() const override { return committed_url_; }
  ukm::SourceId GetPageUkmSourceId() const override { return source_id_; }

  void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                           DIPSRedirectChainInfoPtr chain) override {
    chain->cookie_mode = DIPSCookieMode::kStandard;
    for (auto& redirect : redirects) {
      redirect->has_interaction = GetSiteHasInteraction(redirect->url);
      DCHECK(redirect->access_type != CookieAccessType::kUnknown);
      AppendRedirect(&redirects_, *redirect, *chain);

      DIPSService::HandleRedirectForTesting(
          *redirect, *chain,
          base::BindRepeating(&TestBounceDetectorDelegate::RecordBounce,
                              base::Unretained(this)));
    }
  }

  void RecordEvent(DIPSRecordedEvent event,
                   const GURL& url,
                   const base::Time& time) override {
    recorded_events_.insert(std::make_tuple(url, time, event));
  }

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

  void SetCommittedURL(PassKey<FakeNavigation>, const GURL& url) {
    committed_url_ = url;
    source_id_ = ukm::AssignNewSourceId();
    url_by_source_id_[source_id_] = FormatURL(url);
  }

  std::set<BounceTuple> GetRecordedBounces() const { return recorded_bounces_; }

  std::set<EventTuple> GetRecordedEvents() const { return recorded_events_; }

  const std::vector<std::string>& redirects() const { return redirects_; }

 private:
  void RecordBounce(const GURL& url, base::Time time, bool stateful) {
    recorded_bounces_.insert(std::make_tuple(url, time, stateful));
  }

  GURL committed_url_;
  ukm::SourceId source_id_;
  std::map<ukm::SourceId, std::string> url_by_source_id_;
  std::map<std::string, bool> site_has_interaction_;
  std::vector<std::string> redirects_;
  std::set<BounceTuple> recorded_bounces_;
  std::set<EventTuple> recorded_events_;
};

// If you wait this long, even a navigation without user gesture is not
// considered to be a bounce.
const base::TimeDelta kTooLongForRedirect = base::Seconds(10);

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
      previous_url_ = delegate_->GetLastCommittedURL();
      delegate_->SetCommittedURL(PassKey<FakeNavigation>(), GetURL());
    }
    detector_->DidFinishNavigation(this);
  }

 private:
  // DIPSNavigationHandle overrides:
  bool HasUserGesture() const override { return has_user_gesture_; }
  ServerBounceDetectionState* GetServerState() override { return &state_; }
  bool HasCommitted() const override { return has_committed_; }
  const GURL& GetPreviousPrimaryMainFrameURL() const override {
    return previous_url_;
  }
  const std::vector<GURL>& GetRedirectChain() const override { return chain_; }

  raw_ptr<DIPSBounceDetector> detector_;
  raw_ptr<TestBounceDetectorDelegate> delegate_;
  const bool has_user_gesture_;
  bool finished_ = false;

  ServerBounceDetectionState state_;
  bool has_committed_ = false;
  GURL previous_url_;
  std::vector<GURL> chain_;
};

class DIPSBounceDetectorTest : public ::testing::Test {
 protected:
  FakeNavigation StartNavigation(const std::string& url,
                                 UserGestureStatus status) {
    return FakeNavigation(&detector_, &delegate_, GURL(url),
                          status == kWithUserGesture);
  }

  void NavigateTo(const std::string& url, UserGestureStatus status) {
    StartNavigation(url, status).Finish(true);
  }

  void AccessClientCookie(CookieOperation op) {
    detector_.OnClientCookiesAccessed(delegate_.GetLastCommittedURL(), op);
  }

  void ActivatePage() { detector_.OnUserActivation(); }

  void EndRedirectChain() {
    // Committing a new navigation that began with a user gesture will terminate
    // any previous redirect chain.
    NavigateTo("http://endchain", kWithUserGesture);
  }

  void AdvanceDIPSTime(base::TimeDelta delta) {
    test_clock_.Advance(delta);
    test_tick_clock_.Advance(delta);
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

  std::set<EventTuple> GetRecordedEvents() const {
    return delegate_.GetRecordedEvents();
  }

  EventTuple MakeEventTuple(const std::string& url,
                            const base::Time& time,
                            DIPSRecordedEvent event) {
    return std::make_tuple(GURL(url), time, event);
  }

  base::Time GetCurrentTime() { return test_clock_.Now(); }

  const std::vector<std::string>& redirects() const {
    return delegate_.redirects();
  }

 private:
  TestBounceDetectorDelegate delegate_;
  base::SimpleTestTickClock test_tick_clock_;
  base::SimpleTestClock test_clock_;
  DIPSBounceDetector detector_{&delegate_, &test_tick_clock_, &test_clock_};
};

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

  EndRedirectChain();

  EXPECT_THAT(redirects(),
              testing::ElementsAre(
                  ("[1/3] a.test/ -> b.test/ (Read) -> e.test/"),
                  ("[2/3] a.test/ -> c.test/ (Write) -> e.test/"),
                  ("[3/3] a.test/ -> d.test/ (ReadWrite) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", GetCurrentTime(),
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", GetCurrentTime(),
                                  /*stateful=*/true),
                  MakeBounceTuple("http://d.test", GetCurrentTime(),
                                  /*stateful=*/true)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  NavigateTo("http://c.test", kNoUserGesture);
  EndRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/1] a.test/ -> b.test/ (None) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://b.test", GetCurrentTime(), /*stateful=*/false)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client_MergeCookies) {
  NavigateTo("http://a.test", kWithUserGesture);
  // Server cookie read:
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .Finish(true);
  // Client cookie write:
  AccessClientCookie(CookieOperation::kChange);
  NavigateTo("http://c.test", kNoUserGesture);
  EndRedirectChain();

  // Redirect cookie access is reported as ReadWrite.
  EXPECT_THAT(redirects(),
              testing::ElementsAre(
                  ("[1/1] a.test/ -> b.test/ (ReadWrite) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://b.test", GetCurrentTime(), /*stateful=*/true)));
}

TEST_F(DIPSBounceDetectorTest,
       DetectStatefulRedirect_Client_LongDelayNotRedirect) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(kTooLongForRedirect);
  NavigateTo("http://c.test", kNoUserGesture);
  EndRedirectChain();

  EXPECT_THAT(redirects(), testing::IsEmpty());
  EXPECT_THAT(GetRecordedBounces(), testing::IsEmpty());
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_ServerClientServer) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .Finish(true);
  StartNavigation("http://d.test", kNoUserGesture)
      .RedirectTo("http://e.test")
      .Finish(true);
  EndRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/3] a.test/ -> b.test/ (None) -> e.test/"),
                               ("[2/3] a.test/ -> c.test/ (None) -> e.test/"),
                               ("[3/3] a.test/ -> d.test/ (None) -> e.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", GetCurrentTime(),
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", GetCurrentTime(),
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", GetCurrentTime(),
                                  /*stateful=*/false)));
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
  EndRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/2] a.test/ -> b.test/ (None) -> d.test/"),
                               ("[2/2] a.test/ -> c.test/ (None) -> d.test/"),
                               ("[1/1] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", GetCurrentTime(),
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", GetCurrentTime(),
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", GetCurrentTime(),
                                  /*stateful=*/false)));
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
  EndRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/2] a.test/ -> b.test/ (None) -> d.test/"),
                               ("[2/2] a.test/ -> c.test/ (None) -> d.test/"),
                               ("[1/2] a.test/ -> b.test/ (None) -> f.test/"),
                               ("[2/2] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", GetCurrentTime(),
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", GetCurrentTime(),
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", GetCurrentTime(),
                                  /*stateful=*/false)));
}

TEST_F(DIPSBounceDetectorTest, InteractionRecording_Throttled) {
  base::Time first_time = GetCurrentTime();
  NavigateTo("http://a.test", kNoUserGesture);
  ActivatePage();

  AdvanceDIPSTime(DIPSBounceDetector::kInteractionUpdateInterval / 2);
  ActivatePage();

  AdvanceDIPSTime(DIPSBounceDetector::kInteractionUpdateInterval / 2);
  base::Time last_time = GetCurrentTime();
  ActivatePage();

  // Verify only the first and last interactions were recorded. The second
  // interaction happened less than |kInteractionUpdateInterval| after the
  // first, so it should be ignored.
  EXPECT_THAT(GetRecordedEvents(), testing::SizeIs(2));
  EXPECT_THAT(GetRecordedEvents(),
              testing::UnorderedElementsAre(
                  MakeEventTuple("http://a.test", first_time,
                                 /*event=*/DIPSRecordedEvent::kInteraction),
                  MakeEventTuple("http://a.test", last_time,
                                 /*event=*/DIPSRecordedEvent::kInteraction)));
}

TEST_F(DIPSBounceDetectorTest, InteractionRecording_NotThrottled_AfterRefresh) {
  base::Time first_time = GetCurrentTime();
  NavigateTo("http://a.test", kNoUserGesture);
  ActivatePage();

  AdvanceDIPSTime(DIPSBounceDetector::kInteractionUpdateInterval / 4);
  NavigateTo("http://a.test", kWithUserGesture);

  AdvanceDIPSTime(DIPSBounceDetector::kInteractionUpdateInterval / 4);
  base::Time last_time = GetCurrentTime();
  ActivatePage();

  // Verify the first and last interactions were both recorded. Despite the last
  // interaction happening less than |kInteractionUpdateInterval| after the
  // first, it happened after the page was refreshed, so it should be recorded.
  EXPECT_THAT(GetRecordedEvents(), testing::SizeIs(2));
  EXPECT_THAT(GetRecordedEvents(),
              testing::UnorderedElementsAre(
                  MakeEventTuple("http://a.test", first_time,
                                 /*event=*/DIPSRecordedEvent::kInteraction),
                  MakeEventTuple("http://a.test", last_time,
                                 /*event=*/DIPSRecordedEvent::kInteraction)));
}

const std::vector<std::string>& GetAllRedirectMetrics() {
  static const std::vector<std::string> kAllRedirectMetrics = {
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
  EndRedirectChain();

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Privacy.DIPS.BounceCategoryClient.Standard"] = 1;
  expected_counts["Privacy.DIPS.BounceCategoryServer.Standard"] = 1;
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("Privacy.DIPS.BounceCategory"),
              testing::ContainerEq(expected_counts));
  // Verify the proper values were recorded. b.test has user engagement and read
  // cookies, while c.test has no user engagement and wrote cookies.
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryClient.Standard"),
      testing::ElementsAre(
          // b.test
          Bucket((int)RedirectCategory::kReadCookies_HasEngagement, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryServer.Standard"),
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
  base::test::SingleThreadTaskEnvironment task_environment;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetSiteHasInteraction("http://c.test");

  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(base::Seconds(2));
  AccessClientCookie(CookieOperation::kRead);
  StartNavigation("http://c.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .Finish(true);
  EndRedirectChain();

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
      ukm_recorder.GetEntries("DIPS.Redirect", GetAllRedirectMetrics());
  ASSERT_EQ(2u, ukm_entries.size());

  EXPECT_THAT(URLForNavigationSourceId(ukm_entries[0].source_id),
              Eq("b.test/"));
  EXPECT_THAT(
      ukm_entries[0].metrics,
      ElementsAre(Pair("ClientBounceDelay", 2),
                  Pair("CookieAccessType", (int)CookieAccessType::kRead),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 0), Pair("RedirectChainLength", 2),
                  Pair("RedirectType", (int)DIPSRedirectType::kClient),
                  Pair("SiteEngagementLevel", 0)));

  EXPECT_THAT(URLForRedirectSourceId(&ukm_recorder, ukm_entries[1].source_id),
              Eq("c.test/"));
  EXPECT_THAT(
      ukm_entries[1].metrics,
      ElementsAre(Pair("ClientBounceDelay", 0),
                  Pair("CookieAccessType", (int)CookieAccessType::kWrite),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 1), Pair("RedirectChainLength", 2),
                  Pair("RedirectType", (int)DIPSRedirectType::kServer),
                  Pair("SiteEngagementLevel", 1)));
}
