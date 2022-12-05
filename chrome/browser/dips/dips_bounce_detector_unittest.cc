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
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom-shared.h"

using base::Bucket;
using base::PassKey;
using blink::mojom::EngagementLevel;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::Pair;

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
  DIPSCookieMode GetCookieMode() const override {
    return DIPSCookieMode::kStandard;
  }
  const GURL& GetLastCommittedURL() const override { return committed_url_; }
  ukm::SourceId GetPageUkmSourceId() const override { return source_id_; }
  EngagementLevel GetEngagementLevel(const GURL& url) const override {
    auto iter = level_by_site.find(GetSiteForDIPS(url));
    if (iter == level_by_site.end()) {
      return EngagementLevel::NONE;
    }
    return iter->second;
  }

  void RecordEvent(DIPSRecordedEvent event,
                   const GURL& url,
                   const base::Time& time) override {
    if (event == DIPSRecordedEvent::kStatefulBounce ||
        event == DIPSRecordedEvent::kStatelessBounce) {
      recorded_bounces_.insert(std::make_tuple(
          url, time, event == DIPSRecordedEvent::kStatefulBounce));
    }
  }

  // Get the (committed) URL that the SourceId was generated for.
  const std::string& URLForSourceId(ukm::SourceId source_id) {
    return url_by_source_id_[source_id];
  }

  // Override the default level (MEDIUM) returned by GetEngagementLevel().
  void SetSiteEngagementLevel(const GURL& url, EngagementLevel level) {
    level_by_site[GetSiteForDIPS(url)] = level;
  }

  void SetCommittedURL(PassKey<FakeNavigation>, const GURL& url) {
    committed_url_ = url;
    source_id_ = ukm::AssignNewSourceId();
    url_by_source_id_[source_id_] = FormatURL(url);
  }

  std::set<std::tuple<GURL, base::Time, bool>> GetRecordedBounces() {
    return recorded_bounces_;
  }

 private:
  GURL committed_url_;
  ukm::SourceId source_id_;
  std::map<ukm::SourceId, std::string> url_by_source_id_;
  std::map<std::string, EngagementLevel> level_by_site;
  std::set<std::tuple<GURL, base::Time, bool>> recorded_bounces_;
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
  // Encodes data about a bounce (the url, time of bounce, and
  // whether it's stateful) for use when testing that the bounce is
  // recorded by the DIPSBounceDetector.
  using RecordedBounce = std::tuple<GURL, base::Time, bool>;

  void RecordBouncesAndAppendRedirects(std::vector<std::string>* redirects,
                                       const DIPSRedirectInfo& redirect,
                                       const DIPSRedirectChainInfo& chain) {
    DCHECK(redirect.access_type != CookieAccessType::kUnknown);
    delegate_.RecordEvent(redirect.access_type > CookieAccessType::kRead
                              ? DIPSRecordedEvent::kStatefulBounce
                              : DIPSRecordedEvent::kStatelessBounce,
                          redirect.url, test_clock_.Now());
    AppendRedirect(redirects, redirect, chain);
  }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    detector_.SetRedirectHandlerForTesting(base::BindRepeating(
        &DIPSBounceDetectorTest::RecordBouncesAndAppendRedirects,
        base::Unretained(this), redirects));
  }

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

  void SetSiteEngagementLevel(const std::string& url, EngagementLevel level) {
    return delegate_.SetSiteEngagementLevel(GURL(url), level);
  }

  std::set<std::tuple<GURL, base::Time, bool>> GetRecordedBounces() {
    return delegate_.GetRecordedBounces();
  }

  RecordedBounce MakeRecordedBounce(const std::string& url,
                                    const base::Time& time,
                                    bool stateful) {
    return std::make_tuple(GURL(url), time, stateful);
  }

  base::Time GetCurrentTime() { return test_clock_.Now(); }

 private:
  TestBounceDetectorDelegate delegate_;
  base::SimpleTestTickClock test_tick_clock_;
  base::SimpleTestClock test_clock_;
  DIPSBounceDetector detector_{&delegate_, &test_tick_clock_, &test_clock_};
};

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Server) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

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

  EXPECT_THAT(redirects,
              testing::ElementsAre(
                  ("[1/3] a.test/ -> b.test/ (Read) -> e.test/"),
                  ("[2/3] a.test/ -> c.test/ (Write) -> e.test/"),
                  ("[3/3] a.test/ -> d.test/ (ReadWrite) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeRecordedBounce("http://b.test", GetCurrentTime(),
                                     /*stateful=*/false),
                  MakeRecordedBounce("http://c.test", GetCurrentTime(),
                                     /*stateful=*/true),
                  MakeRecordedBounce("http://d.test", GetCurrentTime(),
                                     /*stateful=*/true)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(base::TimeDelta(base::Seconds(1)));
  NavigateTo("http://c.test", kNoUserGesture);
  EndRedirectChain();

  EXPECT_THAT(redirects, testing::ElementsAre(
                             ("[1/1] a.test/ -> b.test/ (None) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeRecordedBounce(
                  "http://b.test", GetCurrentTime(), /*stateful=*/false)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client_MergeCookies) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

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
  EXPECT_THAT(redirects,
              testing::ElementsAre(
                  ("[1/1] a.test/ -> b.test/ (ReadWrite) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeRecordedBounce(
                  "http://b.test", GetCurrentTime(), /*stateful=*/true)));
}

TEST_F(DIPSBounceDetectorTest,
       DetectStatefulRedirect_Client_LongDelayNotRedirect) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceDIPSTime(kTooLongForRedirect);
  NavigateTo("http://c.test", kNoUserGesture);
  EndRedirectChain();

  EXPECT_THAT(redirects, testing::IsEmpty());
  EXPECT_THAT(GetRecordedBounces(), testing::IsEmpty());
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_ServerClientServer) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .Finish(true);
  StartNavigation("http://d.test", kNoUserGesture)
      .RedirectTo("http://e.test")
      .Finish(true);
  EndRedirectChain();

  EXPECT_THAT(redirects, testing::ElementsAre(
                             ("[1/3] a.test/ -> b.test/ (None) -> e.test/"),
                             ("[2/3] a.test/ -> c.test/ (None) -> e.test/"),
                             ("[3/3] a.test/ -> d.test/ (None) -> e.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeRecordedBounce("http://b.test", GetCurrentTime(),
                                     /*stateful=*/false),
                  MakeRecordedBounce("http://c.test", GetCurrentTime(),
                                     /*stateful=*/false),
                  MakeRecordedBounce("http://d.test", GetCurrentTime(),
                                     /*stateful=*/false)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Server_Uncommitted) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

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

  EXPECT_THAT(redirects, testing::ElementsAre(
                             ("[1/2] a.test/ -> b.test/ (None) -> d.test/"),
                             ("[2/2] a.test/ -> c.test/ (None) -> d.test/"),
                             ("[1/1] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeRecordedBounce("http://b.test", GetCurrentTime(),
                                     /*stateful=*/false),
                  MakeRecordedBounce("http://c.test", GetCurrentTime(),
                                     /*stateful=*/false),
                  MakeRecordedBounce("http://e.test", GetCurrentTime(),
                                     /*stateful=*/false)));
}

TEST_F(DIPSBounceDetectorTest, DetectStatefulRedirect_Client_Uncommitted) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

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

  EXPECT_THAT(redirects, testing::ElementsAre(
                             ("[1/2] a.test/ -> b.test/ (None) -> d.test/"),
                             ("[2/2] a.test/ -> c.test/ (None) -> d.test/"),
                             ("[1/2] a.test/ -> b.test/ (None) -> f.test/"),
                             ("[2/2] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeRecordedBounce("http://b.test", GetCurrentTime(),
                                     /*stateful=*/false),
                  MakeRecordedBounce("http://c.test", GetCurrentTime(),
                                     /*stateful=*/false),
                  MakeRecordedBounce("http://e.test", GetCurrentTime(),
                                     /*stateful=*/false)));
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

  SetSiteEngagementLevel("http://b.test", EngagementLevel::LOW);

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

  SetSiteEngagementLevel("http://b.test", EngagementLevel::MEDIUM);
  SetSiteEngagementLevel("http://c.test", EngagementLevel::LOW);

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
                  Pair("SiteEngagementLevel", (int)EngagementLevel::MEDIUM)));

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
                  Pair("SiteEngagementLevel", (int)EngagementLevel::LOW)));
}
