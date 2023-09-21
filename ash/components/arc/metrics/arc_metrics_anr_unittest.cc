// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_metrics_anr.h"

#include <array>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kAppTypeArcAppLauncher[] = "ArcAppLauncher";
constexpr char kAppTypeArcOther[] = "ArcOther";
constexpr char kAppTypeFirstParty[] = "FirstParty";
constexpr char kAppTypeGmsCore[] = "GmsCore";
constexpr char kAppTypePlayStore[] = "PlayStore";
constexpr char kAppTypeSystemServer[] = "SystemServer";
constexpr char kAppTypeSystem[] = "SystemApp";
constexpr char kAppTypeOther[] = "Other";
constexpr char kAppOverall[] = "Overall";

constexpr std::array<const char*, 9> kAppTypes{
    kAppTypeArcAppLauncher, kAppTypeArcOther,  kAppTypeFirstParty,
    kAppTypeGmsCore,        kAppTypePlayStore, kAppTypeSystemServer,
    kAppTypeSystem,         kAppTypeOther,     kAppOverall,
};

std::string CreateAnrKey(const std::string& app_type, mojom::AnrType type) {
  std::stringstream output;
  output << app_type << "/" << type;
  return output.str();
}

mojom::AnrPtr GetAnr(mojom::AnrSource source, mojom::AnrType type) {
  return mojom::Anr::New(type, source);
}

void VerifyAnr(const base::HistogramTester& tester,
               const std::map<std::string, int>& expectation) {
  std::map<std::string, int> current;
  for (const char* app_type : kAppTypes) {
    const std::vector<base::Bucket> buckets =
        tester.GetAllSamples("Arc.Anr." + std::string(app_type));
    for (const auto& bucket : buckets) {
      current[CreateAnrKey(app_type, static_cast<mojom::AnrType>(bucket.min))] =
          bucket.count;
    }
  }
  EXPECT_EQ(expectation, current);
}

class ArcMetricsAnrTest : public testing::Test {
 public:
  ArcMetricsAnrTest(const ArcMetricsAnrTest&) = delete;
  ArcMetricsAnrTest& operator=(const ArcMetricsAnrTest&) = delete;

 protected:
  ArcMetricsAnrTest() {
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    context_ = std::make_unique<user_prefs::TestBrowserContextWithPrefs>();
    prefs::RegisterLocalStatePrefs(context_->pref_registry());
    prefs::RegisterProfilePrefs(context_->pref_registry());
  }

  ~ArcMetricsAnrTest() override = default;

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  PrefService* prefs() { return context_->prefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_prefs::TestBrowserContextWithPrefs> context_;
};

TEST_F(ArcMetricsAnrTest, Basic) {
  base::HistogramTester tester;
  std::map<std::string, int> expectation;

  ArcMetricsAnr anrs(prefs());

  anrs.Report(GetAnr(mojom::AnrSource::OTHER, mojom::AnrType::UNKNOWN));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::UNKNOWN)] = 1;
  expectation[CreateAnrKey(kAppTypeOther, mojom::AnrType::UNKNOWN)] = 1;
  VerifyAnr(tester, expectation);

  anrs.Report(GetAnr(mojom::AnrSource::SYSTEM_SERVER, mojom::AnrType::INPUT));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::INPUT)] = 1;
  expectation[CreateAnrKey(kAppTypeSystemServer, mojom::AnrType::INPUT)] = 1;
  VerifyAnr(tester, expectation);

  anrs.Report(GetAnr(mojom::AnrSource::SYSTEM_SERVER,
                     mojom::AnrType::FOREGROUND_SERVICE));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::FOREGROUND_SERVICE)] =
      1;
  expectation[CreateAnrKey(kAppTypeSystemServer,
                           mojom::AnrType::FOREGROUND_SERVICE)] = 1;
  VerifyAnr(tester, expectation);

  anrs.Report(GetAnr(mojom::AnrSource::SYSTEM_SERVER,
                     mojom::AnrType::BACKGROUND_SERVICE));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::BACKGROUND_SERVICE)] =
      1;
  expectation[CreateAnrKey(kAppTypeSystemServer,
                           mojom::AnrType::BACKGROUND_SERVICE)] = 1;
  VerifyAnr(tester, expectation);

  anrs.Report(GetAnr(mojom::AnrSource::GMS_CORE, mojom::AnrType::BROADCAST));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::BROADCAST)] = 1;
  expectation[CreateAnrKey(kAppTypeGmsCore, mojom::AnrType::BROADCAST)] = 1;
  VerifyAnr(tester, expectation);

  anrs.Report(
      GetAnr(mojom::AnrSource::PLAY_STORE, mojom::AnrType::CONTENT_PROVIDER));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::CONTENT_PROVIDER)] = 1;
  expectation[CreateAnrKey(kAppTypePlayStore,
                           mojom::AnrType::CONTENT_PROVIDER)] = 1;
  VerifyAnr(tester, expectation);

  anrs.Report(
      GetAnr(mojom::AnrSource::FIRST_PARTY, mojom::AnrType::APP_REQUESTED));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::APP_REQUESTED)] = 1;
  expectation[CreateAnrKey(kAppTypeFirstParty, mojom::AnrType::APP_REQUESTED)] =
      1;
  VerifyAnr(tester, expectation);

  anrs.Report(GetAnr(mojom::AnrSource::ARC_OTHER, mojom::AnrType::INPUT));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::INPUT)] = 2;
  expectation[CreateAnrKey(kAppTypeArcOther, mojom::AnrType::INPUT)] = 1;
  VerifyAnr(tester, expectation);

  anrs.Report(GetAnr(mojom::AnrSource::ARC_APP_LAUNCHER,
                     mojom::AnrType::FOREGROUND_SERVICE));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::FOREGROUND_SERVICE)] =
      2;
  expectation[CreateAnrKey(kAppTypeArcAppLauncher,
                           mojom::AnrType::FOREGROUND_SERVICE)] = 1;
  VerifyAnr(tester, expectation);
}

TEST_F(ArcMetricsAnrTest, ShortSessionOnStart) {
  base::HistogramTester tester;

  std::unique_ptr<ArcMetricsAnr> anrs =
      std::make_unique<ArcMetricsAnr>(prefs());

  anrs->Report(GetAnr(mojom::AnrSource::OTHER, mojom::AnrType::UNKNOWN));
  // Session shorter than 2 min is discarded and ANR should not be reported
  // on restart.
  FastForwardBy(base::Seconds(119));

  // Use explicit reset to preserve right sequence of DTOR/CTOR.
  anrs.reset();
  anrs = std::make_unique<ArcMetricsAnr>(prefs());
  tester.ExpectTotalCount("Arc.Anr.First10MinutesAfterStart", 0);
  EXPECT_EQ(0, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));

  anrs->Report(GetAnr(mojom::AnrSource::OTHER, mojom::AnrType::UNKNOWN));
  // Shorter than 10 minutes but longer than 2 minutes. ANR count should
  // be reported on restart.
  FastForwardBy(base::Minutes(9));
  tester.ExpectTotalCount("Arc.Anr.First10MinutesAfterStart", 0);
  EXPECT_EQ(0, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));

  anrs.reset();
  // ANR updated on DTOR because session was longer than 2 min.
  EXPECT_EQ(1, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));

  anrs = std::make_unique<ArcMetricsAnr>(prefs());
  tester.ExpectTotalCount("Arc.Anr.First10MinutesAfterStart", 1);
  EXPECT_EQ(1, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));

  // Confirm it is not reported twice.
  FastForwardBy(base::Minutes(10));
  // Note, after 10 min one more ANR is reported with 0 ANR.
  tester.ExpectTotalCount("Arc.Anr.First10MinutesAfterStart", 2);
  EXPECT_EQ(1, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));

  anrs.reset();
  EXPECT_EQ(1, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));

  anrs = std::make_unique<ArcMetricsAnr>(prefs());
  tester.ExpectTotalCount("Arc.Anr.First10MinutesAfterStart", 2);
  EXPECT_EQ(1, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));
}

TEST_F(ArcMetricsAnrTest, ForPeriod) {
  base::HistogramTester tester;

  std::unique_ptr<ArcMetricsAnr> anrs =
      std::make_unique<ArcMetricsAnr>(prefs());

  anrs->Report(GetAnr(mojom::AnrSource::OTHER, mojom::AnrType::UNKNOWN));
  anrs->Report(GetAnr(mojom::AnrSource::ARC_APP_LAUNCHER,
                      mojom::AnrType::FOREGROUND_SERVICE));
  FastForwardBy(base::Minutes(5));
  tester.ExpectTotalCount("Arc.Anr.First10MinutesAfterStart", 0);
  FastForwardBy(base::Minutes(5));
  // 10 min start period elapsed, UMA should be ready for the start period.
  tester.ExpectTotalCount("Arc.Anr.First10MinutesAfterStart", 1);
  EXPECT_EQ(2, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));
  tester.ExpectTotalCount("Arc.Anr.Per4Hours", 0);

  anrs->Report(GetAnr(mojom::AnrSource::ARC_OTHER, mojom::AnrType::INPUT));
  const int forward_count_per_update = 4 * 60 / 5;
  // Forward to 5 min before 4 hours interval. Note 10 minutes already elapsed.
  for (int i = 0; i < forward_count_per_update - 2 - 1; ++i) {
    FastForwardBy(base::Minutes(5));
  }
  tester.ExpectTotalCount("Arc.Anr.Per4Hours", 0);
  FastForwardBy(base::Minutes(5));
  // One hour elapsed after start period. UMA should be ready for the regular
  // period.
  tester.ExpectTotalCount("Arc.Anr.Per4Hours", 1);
  EXPECT_EQ(3, tester.GetTotalSum("Arc.Anr.Per4Hours"));

  // One more 4-hours period elapsed. 0 ANR rate should be added for the
  // this period.
  for (int i = 0; i < forward_count_per_update; ++i) {
    FastForwardBy(base::Minutes(5));
  }
  tester.ExpectTotalCount("Arc.Anr.Per4Hours", 2);
  EXPECT_EQ(3, tester.GetTotalSum("Arc.Anr.Per4Hours"));

  anrs->Report(
      GetAnr(mojom::AnrSource::PLAY_STORE, mojom::AnrType::CONTENT_PROVIDER));
  tester.ExpectTotalCount("Arc.Anr.Per4Hours", 2);

  for (int i = 0; i < forward_count_per_update; ++i) {
    FastForwardBy(base::Minutes(5));
  }
  tester.ExpectTotalCount("Arc.Anr.Per4Hours", 3);
  EXPECT_EQ(4, tester.GetTotalSum("Arc.Anr.Per4Hours"));

  // Stop ARC and wait. No more updates are expected.
  anrs.reset();
  for (int i = 0; i < forward_count_per_update; ++i) {
    FastForwardBy(base::Minutes(5));
  }
  tester.ExpectTotalCount("Arc.Anr.Per4Hours", 3);
}

// Test that the class also records UMA stats with the suffix given.
TEST_F(ArcMetricsAnrTest, SuffixedUmaRecording) {
  base::HistogramTester tester;

  std::unique_ptr<ArcMetricsAnr> anrs =
      std::make_unique<ArcMetricsAnr>(prefs());

  anrs->Report(GetAnr(mojom::AnrSource::OTHER, mojom::AnrType::UNKNOWN));
  anrs->set_uma_suffix(".FirstBootAfterUpdate");
  anrs->Report(GetAnr(mojom::AnrSource::ARC_APP_LAUNCHER,
                      mojom::AnrType::FOREGROUND_SERVICE));

  FastForwardBy(base::Minutes(10));
  EXPECT_EQ(2, tester.GetTotalSum("Arc.Anr.First10MinutesAfterStart"));
  EXPECT_EQ(2, tester.GetTotalSum(
                   "Arc.Anr.First10MinutesAfterStart.FirstBootAfterUpdate"));
  FastForwardBy(base::Hours(4));
  EXPECT_EQ(2, tester.GetTotalSum("Arc.Anr.Per4Hours"));
  EXPECT_EQ(2, tester.GetTotalSum("Arc.Anr.Per4Hours.FirstBootAfterUpdate"));
}

}  // namespace
}  // namespace arc
