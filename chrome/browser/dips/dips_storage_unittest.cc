// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_storage.h"

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_state.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

class TestStorage : public DIPSStorage {
 public:
  TestStorage() : DIPSStorage(absl::nullopt) {}

  void WriteForTesting(GURL url, const StateValue& state) {
    Write(DIPSState(this, GetSiteForDIPS(url), state));
  }
};

scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

void StoreState(absl::optional<StateValue>* state_value,
                const DIPSState& state) {
  *state_value = state.was_loaded() ? absl::make_optional(state.ToStateValue())
                                    : absl::nullopt;
}

class ScopedDIPSFeatureEnabledWithParams {
 public:
  explicit ScopedDIPSFeatureEnabledWithParams(
      const base::FieldTrialParams& params) {
    features_.InitAndEnableFeatureWithParameters(dips::kFeature, params);
  }

 private:
  base::test::ScopedFeatureList features_;
};

}  // namespace

TEST(GetSitesToClearTest, FiltersByTriggerParam) {
  TestStorage storage;

  GURL kBounceUrl("https://bounce.com");
  GURL kStorageUrl("https://storage.com");
  GURL kStatefulBounceUrl("https://stateful_bounce.com");

  TimestampRange event(
      {base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)});
  storage.WriteForTesting(kBounceUrl, StateValue{.bounce_times = event});
  storage.WriteForTesting(kStorageUrl, StateValue{.site_storage_times = event});
  storage.WriteForTesting(kStatefulBounceUrl,
                          StateValue{.site_storage_times = event,
                                     .stateful_bounce_times = event,
                                     .bounce_times = event});
  // Call 'GetSitesToClear' when the trigger is unset.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(dips::kFeature);
    EXPECT_THAT(storage.GetSitesToClear(), testing::IsEmpty());
  }
  // Call 'GetSitesToClear' when DIPS is triggered by bounces.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        dips::kFeature, {{"triggering_action", "bounce"}});
    EXPECT_THAT(storage.GetSitesToClear(),
                testing::ElementsAre(GetSiteForDIPS(kBounceUrl),
                                     GetSiteForDIPS(kStatefulBounceUrl)));
  }
  // Call 'GetSitesToClear' when DIPS is triggered by storage.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        dips::kFeature, {{"triggering_action", "storage"}});
    EXPECT_THAT(storage.GetSitesToClear(),
                testing::ElementsAre(GetSiteForDIPS(kStatefulBounceUrl),
                                     GetSiteForDIPS(kStorageUrl)));
  }
  // Call 'GetSitesToClear' when DIPS is triggered by stateful bounces.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        dips::kFeature, {{"triggering_action", "stateful_bounce"}});
    EXPECT_THAT(storage.GetSitesToClear(),
                testing::ElementsAre(GetSiteForDIPS(kStatefulBounceUrl)));
  }
}

class DIPSStorageTest : public testing::Test {
 public:
  DIPSStorageTest() = default;

 protected:
  base::test::TaskEnvironment env_;
  ScopedDIPSFeatureEnabledWithParams feature{{{"interaction_ttl", "inf"}}};
  TestStorage storage_;
};

TEST(DirtyBit, Constructor) {
  ASSERT_FALSE(DirtyBit());
  ASSERT_TRUE(DirtyBit(true));
  ASSERT_FALSE(DirtyBit(false));
}

TEST(DirtyBit, Assignment) {
  DirtyBit bit;

  bit = true;
  ASSERT_TRUE(bit);

  bit = false;
  ASSERT_FALSE(bit);
}

TEST(DirtyBit, Move) {
  DirtyBit bit(true);
  DirtyBit moved(std::move(bit));

  ASSERT_TRUE(moved);
  ASSERT_FALSE(bit);  // NOLINT
}

TEST(DIPSUtilsTest, GetSiteForDIPS) {
  EXPECT_EQ("example.com", GetSiteForDIPS(GURL("http://example.com/foo")));
  EXPECT_EQ("example.com", GetSiteForDIPS(GURL("https://www.example.com/bar")));
  EXPECT_EQ("example.com",
            GetSiteForDIPS(GURL("http://other.example.com/baz")));
  EXPECT_EQ("bar.baz.r.appspot.com",
            GetSiteForDIPS(GURL("http://foo.bar.baz.r.appspot.com/baz")));
  EXPECT_EQ("localhost", GetSiteForDIPS(GURL("http://localhost:8000/qux")));
  EXPECT_EQ("127.0.0.1", GetSiteForDIPS(GURL("http://127.0.0.1:8888/")));
  EXPECT_EQ("[::1]", GetSiteForDIPS(GURL("http://[::1]/")));
}

TEST_F(DIPSStorageTest, NewURL) {
  DIPSState state = storage_.Read(GURL("http://example.com/"));
  EXPECT_FALSE(state.was_loaded());
  EXPECT_FALSE(state.site_storage_times().has_value());
  EXPECT_FALSE(state.user_interaction_times().has_value());
}

TEST_F(DIPSStorageTest, SetValues) {
  GURL url("https://example.com");
  auto time1 = base::Time::FromDoubleT(1);
  auto time2 = base::Time::FromDoubleT(2);

  {
    DIPSState state = storage_.Read(url);
    state.update_site_storage_time(time1);
    state.update_user_interaction_time(time2);

    // Before flushing `state`, reads for the same URL won't include its
    // changes.
    DIPSState state2 = storage_.Read(url);
    EXPECT_FALSE(state2.site_storage_times().has_value());
    EXPECT_FALSE(state2.user_interaction_times().has_value());
  }

  DIPSState state = storage_.Read(url);
  EXPECT_TRUE(state.was_loaded());
  EXPECT_EQ(state.site_storage_times()->first, absl::make_optional(time1));
  EXPECT_EQ(state.user_interaction_times()->first, absl::make_optional(time2));
}

TEST_F(DIPSStorageTest, SameSiteSameState) {
  // The two urls use different subdomains of example.com; and one is HTTPS
  // while the other is HTTP.
  GURL url1("https://subdomain1.example.com");
  GURL url2("http://subdomain2.example.com");
  auto time = base::Time::FromDoubleT(1);

  storage_.Read(url1).update_site_storage_time(time);

  DIPSState state = storage_.Read(url2);
  // State was recorded for url1, but can be read for url2.
  EXPECT_EQ(time, state.site_storage_times()->first);
  EXPECT_FALSE(state.user_interaction_times().has_value());
}

TEST_F(DIPSStorageTest, DifferentSiteDifferentState) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  auto time1 = base::Time::FromDoubleT(1);
  auto time2 = base::Time::FromDoubleT(2);

  storage_.Read(url1).update_site_storage_time(time1);
  storage_.Read(url2).update_site_storage_time(time2);

  // Verify that url1 and url2 have independent state:
  EXPECT_EQ(storage_.Read(url1).site_storage_times()->first,
            absl::make_optional(time1));
  EXPECT_EQ(storage_.Read(url2).site_storage_times()->first,
            absl::make_optional(time2));
}

TEST_F(DIPSStorageTest, RemoveByTimeWithNullRangeEndTime) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromDoubleT(2);
  base::Time delete_end = base::Time();

  storage_.WriteForTesting(
      url1, {/*site_storage_times= */ {
                 {base::Time::FromDoubleT(1), base::Time::FromDoubleT(3)}},
             /*user_interaction_times= */ {
                 {base::Time::FromDoubleT(5), base::Time::FromDoubleT(8)}}});
  storage_.WriteForTesting(
      url2, {/*site_storage_times= */ TimestampRange(),
             /*user_interaction_times= */ {
                 {base::Time::FromDoubleT(3), base::Time::FromDoubleT(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        DIPSEventRemovalType::kAll);

  DIPSState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            absl::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.user_interaction_times(),
            absl::nullopt);  // removed

  DIPSState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(DIPSStorageTest, RemoveByTimeWithNullRangeBeginTime) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::Min();
  base::Time delete_end = base::Time::FromDoubleT(6);

  storage_.WriteForTesting(
      url1, {/*site_storage_times= */ {
                 {base::Time::FromDoubleT(1), base::Time::FromDoubleT(3)}},
             /*user_interaction_times= */ {
                 {base::Time::FromDoubleT(5), base::Time::FromDoubleT(8)}}});
  storage_.WriteForTesting(
      url2, {/*site_storage_times= */ TimestampRange(),
             /*user_interaction_times= */ {
                 {base::Time::FromDoubleT(3), base::Time::FromDoubleT(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        DIPSEventRemovalType::kAll);

  DIPSState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times(), absl::nullopt);  // removed
  EXPECT_EQ(state1.user_interaction_times()->first,
            absl::make_optional(delete_end));  // adjusted
  EXPECT_EQ(state1.user_interaction_times()->second,
            absl::make_optional(base::Time::FromDoubleT(8)));  // no change

  DIPSState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(DIPSStorageTest, RemoveByTimeAdjustsOverlappingTimes) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromDoubleT(2);
  base::Time delete_end = base::Time::FromDoubleT(6);

  storage_.WriteForTesting(
      url1, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(5), base::Time::FromDoubleT(8)}}});
  storage_.WriteForTesting(
      url2, {TimestampRange(),
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        DIPSEventRemovalType::kAll);

  DIPSState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            absl::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.user_interaction_times()->first,
            absl::make_optional(delete_end));  // adjusted
  EXPECT_EQ(state1.user_interaction_times()->second,
            absl::make_optional(base::Time::FromDoubleT(8)));  // no change

  DIPSState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(DIPSStorageTest, RemoveByTimeDoesNotAffectTouchingWindowEndpoints) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromDoubleT(3);
  base::Time delete_end = base::Time::FromDoubleT(5);

  storage_.WriteForTesting(
      url1, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(5), base::Time::FromDoubleT(8)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        DIPSEventRemovalType::kAll);

  DIPSState state = storage_.Read(url1);
  EXPECT_EQ(state.site_storage_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state.site_storage_times()->second,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
  EXPECT_EQ(state.user_interaction_times()->first,
            absl::make_optional(base::Time::FromDoubleT(5)));  // no change
  EXPECT_EQ(state.user_interaction_times()->second,
            absl::make_optional(base::Time::FromDoubleT(8)));  // no change
}

TEST_F(DIPSStorageTest, RemoveByTimeStorageOnly) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromDoubleT(2);
  base::Time delete_end = base::Time::FromDoubleT(6);

  storage_.WriteForTesting(
      url1, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(5), base::Time::FromDoubleT(8)}}});
  storage_.WriteForTesting(
      url2, {TimestampRange(),
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        DIPSEventRemovalType::kStorage);

  DIPSState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            absl::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.user_interaction_times()->first,
            absl::make_optional(base::Time::FromDoubleT(5)));  // no change
  EXPECT_EQ(state1.user_interaction_times()->second,
            absl::make_optional(base::Time::FromDoubleT(8)));  // no change

  DIPSState state2 = storage_.Read(url2);
  EXPECT_EQ(state2.user_interaction_times()->first,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
  EXPECT_EQ(state2.user_interaction_times()->second,
            absl::make_optional(base::Time::FromDoubleT(5)));  // no change
}

TEST_F(DIPSStorageTest, RemoveByTimeInteractionOnly) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromDoubleT(2);
  base::Time delete_end = base::Time::FromDoubleT(6);

  storage_.WriteForTesting(
      url1, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(5), base::Time::FromDoubleT(8)}}});
  storage_.WriteForTesting(
      url2, {TimestampRange(),
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        DIPSEventRemovalType::kHistory);

  DIPSState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
  EXPECT_EQ(state1.user_interaction_times()->first,
            absl::make_optional(delete_end));  // adjusted
  EXPECT_EQ(state1.user_interaction_times()->second,
            absl::make_optional(base::Time::FromDoubleT(8)));  // no change

  DIPSState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(DIPSStorageTest, RemoveByTimeBounces) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromDoubleT(2);
  base::Time delete_end = base::Time::FromDoubleT(6);

  storage_.WriteForTesting(
      url1, {TimestampRange(),
             TimestampRange(),
             {{base::Time::FromDoubleT(1), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(1), base::Time::FromDoubleT(8)}}});
  storage_.WriteForTesting(
      url2, {TimestampRange(),
             TimestampRange(),
             TimestampRange(),
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        DIPSEventRemovalType::kStorage);

  DIPSState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.stateful_bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state1.stateful_bounce_times()->second,
            absl::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state1.bounce_times()->second,
            absl::make_optional(base::Time::FromDoubleT(8)));  // no change

  DIPSState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(DIPSStorageTest, RemoveBySite) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  GURL url3("https://example3.com");
  GURL url4("https://example4.com");

  storage_.WriteForTesting(
      url1, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)}},
             {{base::Time::FromDoubleT(2), base::Time::FromDoubleT(2)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}}});
  storage_.WriteForTesting(
      url2, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)}},
             {{base::Time::FromDoubleT(2), base::Time::FromDoubleT(2)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}}});
  storage_.WriteForTesting(
      url3, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(2)}},
             TimestampRange(),
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}}});
  storage_.WriteForTesting(
      url4, {TimestampRange(),
             {{base::Time::FromDoubleT(2), base::Time::FromDoubleT(2)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}}});

  std::unique_ptr<content::BrowsingDataFilterBuilder> builder =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete);
  builder->AddRegisterableDomain(GetSiteForDIPS(url1));
  builder->AddRegisterableDomain(GetSiteForDIPS(url3));
  storage_.RemoveEvents(base::Time(), base::Time::Max(),
                        builder->BuildNetworkServiceFilter(),
                        DIPSEventRemovalType::kStorage);

  DIPSState state1 = storage_.Read(url1);
  EXPECT_FALSE(state1.site_storage_times().has_value());  // removed
  EXPECT_EQ(state1.user_interaction_times()->first,
            absl::make_optional(base::Time::FromDoubleT(2)));  // no change
  EXPECT_FALSE(state1.stateful_bounce_times().has_value());    // removed
  EXPECT_FALSE(state1.bounce_times().has_value());             // removed

  DIPSState state2 = storage_.Read(url2);
  EXPECT_EQ(state2.site_storage_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state2.user_interaction_times()->first,
            absl::make_optional(base::Time::FromDoubleT(2)));  // no change
  EXPECT_EQ(state2.stateful_bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
  EXPECT_EQ(state2.bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change

  DIPSState state3 = storage_.Read(url3);
  EXPECT_FALSE(state3.was_loaded());  // removed

  DIPSState state4 = storage_.Read(url2);
  EXPECT_FALSE(state1.site_storage_times().has_value());  // no change
  EXPECT_EQ(state4.user_interaction_times()->first,
            absl::make_optional(base::Time::FromDoubleT(2)));  // no change
  EXPECT_EQ(state4.stateful_bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
  EXPECT_EQ(state4.bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
}

TEST_F(DIPSStorageTest, RemoveBySiteIgnoresDeletionWithTimeRange) {
  GURL url1("https://example1.com");
  base::Time delete_begin = base::Time::FromDoubleT(2);
  base::Time delete_end = base::Time::FromDoubleT(6);

  storage_.WriteForTesting(
      url1, {{{base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)}},
             {{base::Time::FromDoubleT(2), base::Time::FromDoubleT(2)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(3)}},
             {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}}});

  std::unique_ptr<content::BrowsingDataFilterBuilder> builder =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete);
  builder->AddRegisterableDomain(GetSiteForDIPS(url1));
  storage_.RemoveEvents(delete_begin, delete_end,
                        builder->BuildNetworkServiceFilter(),
                        DIPSEventRemovalType::kStorage);

  // Removing events by site (i.e. by using a non-null filter) with a time-range
  // (other than base::Time() to base::Time::Max()), is currently unsupported.
  // So url1's DIPS Storage entry should be unaffected.
  DIPSState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            absl::make_optional(base::Time::FromDoubleT(1)));  // no change
  EXPECT_EQ(state1.user_interaction_times()->first,
            absl::make_optional(base::Time::FromDoubleT(2)));  // no change
  EXPECT_EQ(state1.stateful_bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
  EXPECT_EQ(state1.bounce_times()->first,
            absl::make_optional(base::Time::FromDoubleT(3)));  // no change
}

TEST_F(DIPSStorageTest, RemoveRows) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  ASSERT_TRUE(url1.is_valid());
  ASSERT_TRUE(url2.is_valid());

  StateValue test_value = {
      {{base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)}},
      {{base::Time::FromDoubleT(2), base::Time::FromDoubleT(2)}},
      {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(3)}},
      {{base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}}};

  storage_.WriteForTesting(url1, test_value);
  storage_.WriteForTesting(url2, test_value);

  ASSERT_EQ(storage_.Read(url1).ToStateValue(), test_value);
  ASSERT_EQ(storage_.Read(url2).ToStateValue(), test_value);

  storage_.RemoveRows({GetSiteForDIPS(url1), GetSiteForDIPS(url2)});

  EXPECT_FALSE(storage_.Read(url1).was_loaded());
  EXPECT_FALSE(storage_.Read(url2).was_loaded());
}

class DIPSStoragePrepopulateTest : public testing::Test {
 public:
  DIPSStoragePrepopulateTest()
      : task_environment_(base::test::TaskEnvironment(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED)),
        storage_(base::SequenceBound<DIPSStorage>(CreateTaskRunner(),
                                                  absl::nullopt)) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  ScopedDIPSFeatureEnabledWithParams feature{{{"interaction_ttl", "inf"}}};
  base::SequenceBound<DIPSStorage> storage_;
};

TEST_F(DIPSStoragePrepopulateTest, NoExistingTime) {
  base::Time time = base::Time::FromDoubleT(1);

  storage_.AsyncCall(&DIPSStorage::Prepopulate)
      .WithArgs(time, std::vector<std::string>{"site"}, base::DoNothing());
  absl::optional<StateValue> state;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(GURL("http://site"))
      .Then(base::BindOnce(StoreState, &state));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->user_interaction_times->first, time);  // written
  EXPECT_EQ(state->site_storage_times->first, time);      // written
}

TEST_F(DIPSStoragePrepopulateTest, ExistingStorageAndInteractionTimes) {
  base::Time interaction_time = base::Time::FromDoubleT(1);
  base::Time storage_time = base::Time::FromDoubleT(2);
  base::Time prepopulate_time = base::Time::FromDoubleT(3);

  // First record interaction and storage for the site, then call Prepopulate().
  storage_.AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(GURL("http://site"), interaction_time,
                DIPSCookieMode::kStandard);
  storage_.AsyncCall(&DIPSStorage::RecordStorage)
      .WithArgs(GURL("http://site"), storage_time, DIPSCookieMode::kStandard);
  storage_.AsyncCall(&DIPSStorage::Prepopulate)
      .WithArgs(prepopulate_time, std::vector<std::string>{"site"},
                base::DoNothing());
  absl::optional<StateValue> state;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(GURL("http://site"))
      .Then(base::BindOnce(StoreState, &state));
  task_environment_.RunUntilIdle();

  // Prepopulate() didn't overwrite the previous timestamps.
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->user_interaction_times->first,
            interaction_time);  // no change
  EXPECT_EQ(state->site_storage_times->first,
            storage_time);  // no change
}

TEST_F(DIPSStoragePrepopulateTest, ExistingStorageTime) {
  base::Time storage_time = base::Time::FromDoubleT(1);
  base::Time prepopulate_time = base::Time::FromDoubleT(2);

  // Record only storage for the site, then call Prepopulate().
  storage_.AsyncCall(&DIPSStorage::RecordStorage)
      .WithArgs(GURL("http://site"), storage_time, DIPSCookieMode::kStandard);
  storage_.AsyncCall(&DIPSStorage::Prepopulate)
      .WithArgs(prepopulate_time, std::vector<std::string>{"site"},
                base::DoNothing());
  absl::optional<StateValue> state;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(GURL("http://site"))
      .Then(base::BindOnce(StoreState, &state));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->site_storage_times->first,
            storage_time);  // no change
  EXPECT_EQ(state->user_interaction_times->first,
            prepopulate_time);  // written
}

TEST_F(DIPSStoragePrepopulateTest, ExistingInteractionTime) {
  base::Time interaction_time = base::Time::FromDoubleT(1);
  base::Time prepopulate_time = base::Time::FromDoubleT(2);

  // Record only storage for the site, then call Prepopulate().
  storage_.AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(GURL("http://site"), interaction_time,
                DIPSCookieMode::kStandard);
  storage_.AsyncCall(&DIPSStorage::Prepopulate)
      .WithArgs(prepopulate_time, std::vector<std::string>{"site"},
                base::DoNothing());
  absl::optional<StateValue> state;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(GURL("http://site"))
      .Then(base::BindOnce(StoreState, &state));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->user_interaction_times->first,
            interaction_time);                          // no change
  EXPECT_EQ(state->site_storage_times, absl::nullopt);  // no change
}

TEST_F(DIPSStoragePrepopulateTest, WorksOnChunks) {
  base::Time time = base::Time::FromDoubleT(1);
  std::vector<std::string> sites = {"site1", "site2", "site3"};
  DIPSStorage::SetPrepopulateChunkSizeForTesting(2);

  absl::optional<StateValue> state1, state2, state3;
  auto queue_state_reads = [&]() {
    storage_.AsyncCall(&DIPSStorage::Read)
        .WithArgs(GURL("http://site1"))
        .Then(base::BindOnce(StoreState, &state1));
    storage_.AsyncCall(&DIPSStorage::Read)
        .WithArgs(GURL("http://site2"))
        .Then(base::BindOnce(StoreState, &state2));
    storage_.AsyncCall(&DIPSStorage::Read)
        .WithArgs(GURL("http://site3"))
        .Then(base::BindOnce(StoreState, &state3));
  };

  storage_.AsyncCall(&DIPSStorage::Prepopulate)
      .WithArgs(time, std::move(sites), base::DoNothing());
  queue_state_reads();
  task_environment_.RunUntilIdle();

  // At this point, the entire |sites| vector has been processed. But we made
  // async calls to read the state for each site before Prepopulate()
  // actually ran, so the reads were performed after only the first chunk of
  // |sites| was processed.

  // The first two sites were prepopulated.
  EXPECT_TRUE(state1.has_value());
  EXPECT_TRUE(state2.has_value());
  // The last wasn't.
  ASSERT_FALSE(state3.has_value());

  queue_state_reads();
  task_environment_.RunUntilIdle();

  // Now we've read the final state for all sites.
  EXPECT_TRUE(state1.has_value());
  EXPECT_TRUE(state2.has_value());
  EXPECT_TRUE(state3.has_value());
}
