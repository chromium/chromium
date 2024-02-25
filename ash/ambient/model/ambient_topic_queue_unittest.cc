// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_topic_queue.h"

#include <memory>
#include <string>

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/test/ambient_topic_queue_test_delegate.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr base::TimeDelta kDefaultTopicFetchInterval = base::Seconds(10);

AmbientTopicQueue::WaitResult WaitForTopicsAvailable(AmbientTopicQueue& queue) {
  static constexpr base::TimeDelta kTimeout = base::Seconds(3);
  base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE, kTimeout);
  AmbientTopicQueue::WaitResult result_out;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  queue.WaitForTopicsAvailable(base::BindLambdaForTesting(
      [&result_out, quit_closure](AmbientTopicQueue::WaitResult result_in) {
        result_out = result_in;
        quit_closure.Run();
      }));
  run_loop.Run();
  return result_out;
}

std::vector<AmbientModeTopic> PopUntilEmpty(AmbientTopicQueue& queue) {
  std::vector<AmbientModeTopic> fetched_topics;
  while (!queue.IsEmpty()) {
    fetched_topics.push_back(queue.Pop());
  }
  return fetched_topics;
}

AmbientModeTopic CreateTopic(const std::string& url,
                             const std::string& details,
                             bool is_portrait,
                             const std::string& related_url,
                             const std::string& related_details,
                             ::ambient::TopicType topic_type) {
  AmbientModeTopic topic;
  topic.url = url;
  topic.details = details;
  topic.is_portrait = is_portrait;
  topic.topic_type = topic_type;

  topic.related_image_url = related_url;
  topic.related_details = related_details;
  return topic;
}

MATCHER_P(TopicUrlContainsSize, size, "") {
  return base::Contains(arg.url, size.ToString());
}

class AmbientTopicQueueTest : public AmbientAshTestBase {
 protected:
  static constexpr ::ambient::TopicType kDefaultTopicType =
      ::ambient::TopicType::kGeo;

  void SetUp() override {
    AmbientAshTestBase::SetUp();
    // Disable any sort of pairing on the client by default. Makes it easier to
    // reason about how many topics are in the queue.
    backend_controller()->SetPhotoTopicType(kDefaultTopicType);
  }

  AmbientTopicQueueTestDelegate delegate_;
};

// By default, FakeAmbientBackendControllerImpl returns paired topics.
class AmbientTopicQueuePairedTopicTest : public AmbientAshTestBase {
 protected:
  AmbientTopicQueueTestDelegate delegate_;
};

TEST_F(AmbientTopicQueueTest, WaitForTopicsAvailable) {
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/5,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
  AmbientModeTopic topic = queue.Pop();
  EXPECT_THAT(topic.topic_type, Eq(kDefaultTopicType));

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
  topic = queue.Pop();
  EXPECT_THAT(topic.topic_type, Eq(kDefaultTopicType));
}

TEST_F(AmbientTopicQueueTest, RefillsWhenEmpty) {
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
  queue.Pop();
  ASSERT_TRUE(queue.IsEmpty());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
}

TEST_F(AmbientTopicQueueTest, RefillsOnSchedule) {
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  // Wait for first topic to be pushed.
  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  // Second topic should be pushed.
  task_environment()->FastForwardBy(kDefaultTopicFetchInterval);
  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  EXPECT_THAT(PopUntilEmpty(queue), SizeIs(2u));
}

TEST_F(AmbientTopicQueueTest, StopsRefillingAtLimit) {
  AmbientTopicQueue queue(/*topic_fetch_limit=*/2, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
  queue.Pop();

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
  queue.Pop();

  EXPECT_TRUE(queue.IsEmpty());
  EXPECT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicFetchLimitReached));
}

TEST_F(AmbientTopicQueueTest, StopsRefillingAtLimitForScheduledRefills) {
  AmbientTopicQueue queue(/*topic_fetch_limit=*/2, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  // Fast forward some huge amount so that the topic limit should be reached
  // via scheduled refills.
  task_environment()->FastForwardBy(10 * kDefaultTopicFetchInterval);

  EXPECT_THAT(PopUntilEmpty(queue), SizeIs(2u));
  EXPECT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicFetchLimitReached));
}

TEST_F(AmbientTopicQueueTest, NotifiesWhenFetchFailed) {
  backend_controller()->SetFetchScreenUpdateInfoResponseSize(0);
  AmbientTopicQueue queue(/*topic_fetch_limit=*/2, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  EXPECT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicFetchBackingOff));
}

TEST_F(AmbientTopicQueueTest, NotifiesWhenBackingOff) {
  backend_controller()->SetFetchScreenUpdateInfoResponseSize(0);
  AmbientTopicQueue queue(/*topic_fetch_limit=*/2, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  // Fast forward some huge amount to enter backoff state.
  task_environment()->FastForwardBy(10 * kDefaultTopicFetchInterval);
  EXPECT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicFetchBackingOff));
}

TEST_F(AmbientTopicQueueTest, RetriesAfterFailedFetch) {
  backend_controller()->SetFetchScreenUpdateInfoResponseSize(0);
  AmbientTopicQueue queue(/*topic_fetch_limit=*/2, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  // Fast forward by some huge amount of time to simulate a series of failures
  // with backoff.
  task_environment()->FastForwardBy(10 * kDefaultTopicFetchInterval);

  // Now it should start succeeding again.
  backend_controller()->SetFetchScreenUpdateInfoResponseSize(1);
  // Fast forward by some huge amount of time to fill the queue.
  task_environment()->FastForwardBy(10 * kDefaultTopicFetchInterval);
  EXPECT_THAT(PopUntilEmpty(queue), SizeIs(2u));
  EXPECT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicFetchLimitReached));
}

TEST_F(AmbientTopicQueueTest, DoesNotPairTopicsWhenSplitIsSet) {
  backend_controller()->SetPhotoTopicType(
      ::ambient::TopicType::kCulturalInstitute);
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/2,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/true, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));

  auto fetched_topics = PopUntilEmpty(queue);
  ASSERT_THAT(fetched_topics, SizeIs(2));
  EXPECT_THAT(fetched_topics[0].related_image_url, IsEmpty());
  EXPECT_THAT(fetched_topics[1].related_image_url, IsEmpty());
}

TEST_F(AmbientTopicQueuePairedTopicTest, SplitsIncomingTopics) {
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/2,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/true, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));

  auto fetched_topics = PopUntilEmpty(queue);
  ASSERT_THAT(fetched_topics, SizeIs(4));
  EXPECT_THAT(fetched_topics,
              Each(Field(&AmbientModeTopic::related_image_url, IsEmpty())));
}

TEST_F(AmbientTopicQueueTest, RequestsPairedPersonalPortraits) {
  backend_controller()->SetPhotoTopicType(::ambient::TopicType::kPersonal);
  backend_controller()->SetPhotoOrientation(/*portrait=*/true);
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
  AmbientModeTopic topic = queue.Pop();
  EXPECT_THAT(topic.url, Not(IsEmpty()));
  EXPECT_THAT(topic.related_image_url, Not(IsEmpty()));
}

TEST_F(AmbientTopicQueueTest, DoesNotRequestPairedPersonalPortraits) {
  backend_controller()->SetPhotoTopicType(::ambient::TopicType::kPersonal);
  backend_controller()->SetPhotoOrientation(/*portrait=*/true);
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/1,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/true, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  ASSERT_FALSE(queue.IsEmpty());
  AmbientModeTopic topic = queue.Pop();
  EXPECT_THAT(topic.url, Not(IsEmpty()));
  EXPECT_THAT(topic.related_image_url, IsEmpty());
}

TEST_F(AmbientTopicQueueTest, ShouldPairLandscapeImages) {
  backend_controller()->set_custom_topic_generator(
      base::BindLambdaForTesting([](int, const gfx::Size&) {
        // Set up 3 featured landscape photos and 3 personal landscape photos.
        // Will output 2 paired topics, having one in featured and personal
        // category.
        std::vector<AmbientModeTopic> topics;
        topics.emplace_back(CreateTopic(
            /*url=*/"topic1_url", /*details=*/"topic1_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic2_url", /*details=*/"topic2_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic3_url", /*details=*/"topic3_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"topic3_related_details",
            ::ambient::TopicType::kPersonal));

        topics.emplace_back(CreateTopic(
            /*url=*/"topic4_url", /*details=*/"topic4_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kFeatured));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic5_url", /*details=*/"topic5_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kFeatured));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic6_url", /*details=*/"topic6_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kFeatured));
        return topics;
      }));

  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/10,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  ASSERT_THAT(fetched_topics, SizeIs(2u));

  const int index =
      (fetched_topics[0].topic_type == ::ambient::TopicType::kPersonal) ? 0 : 1;
  EXPECT_EQ(fetched_topics[index].url, "topic1_url");
  EXPECT_EQ(fetched_topics[index].details, "topic1_details");
  EXPECT_FALSE(fetched_topics[index].is_portrait);
  EXPECT_EQ(fetched_topics[index].topic_type, ::ambient::TopicType::kPersonal);
  EXPECT_EQ(fetched_topics[index].related_image_url, "topic2_url");
  EXPECT_EQ(fetched_topics[index].related_details, "topic2_details");

  EXPECT_EQ(fetched_topics[1 - index].url, "topic4_url");
  EXPECT_EQ(fetched_topics[1 - index].details, "topic4_details");
  EXPECT_FALSE(fetched_topics[1 - index].is_portrait);
  EXPECT_EQ(fetched_topics[1 - index].topic_type,
            ::ambient::TopicType::kFeatured);
  EXPECT_EQ(fetched_topics[1 - index].related_image_url, "topic5_url");
  EXPECT_EQ(fetched_topics[1 - index].related_details, "topic5_details");
}

TEST_F(AmbientTopicQueueTest, ShouldNotPairPortraitImages) {
  backend_controller()->set_custom_topic_generator(
      base::BindLambdaForTesting([](int, const gfx::Size&) {
        // Test that topics with portrait photos will not be re-paired. Only
        // topics with landscape photos will be paired. Set up 3 landscape
        // topics and 3 portrait topics. Will output 4 topics, having one paired
        // landscape photo. The 3 portrait topics will not change.
        std::vector<AmbientModeTopic> topics;
        topics.emplace_back(CreateTopic(
            /*url=*/"topic1_url", /*details=*/"topic1_details",
            /*is_portrait=*/true,
            /*related_url=*/"topic1_related_url",
            /*related_details=*/"topic1_related_details",
            ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic2_url", /*details=*/"topic2_details",
            /*is_portrait=*/true,
            /*related_url=*/"topic2_related_url",
            /*related_details=*/"topic2_related_details",
            ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic3_url", /*details=*/"topic3_details",
            /*is_portrait=*/true,
            /*related_url=*/"topic3_related_url",
            /*related_details=*/"topic3_related_details",
            ::ambient::TopicType::kPersonal));

        topics.emplace_back(CreateTopic(
            /*url=*/"topic4_url", /*details=*/"topic4_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic5_url", /*details=*/"topic5_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic6_url", /*details=*/"topic6_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kPersonal));
        return topics;
      }));

  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/10,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  ASSERT_THAT(fetched_topics, SizeIs(4u));

  for (size_t index = 0; index < fetched_topics.size(); ++index) {
    if (fetched_topics[index].url == "topic1_url") {
      EXPECT_EQ(fetched_topics[index].url, "topic1_url");
      EXPECT_EQ(fetched_topics[index].details, "topic1_details");
      EXPECT_TRUE(fetched_topics[index].is_portrait);
      EXPECT_EQ(fetched_topics[index].topic_type,
                ::ambient::TopicType::kPersonal);
      EXPECT_EQ(fetched_topics[index].related_image_url, "topic1_related_url");
      EXPECT_EQ(fetched_topics[index].related_details,
                "topic1_related_details");
    } else if (fetched_topics[index].url == "topic2_url") {
      EXPECT_EQ(fetched_topics[index].url, "topic2_url");
      EXPECT_EQ(fetched_topics[index].details, "topic2_details");
      EXPECT_TRUE(fetched_topics[index].is_portrait);
      EXPECT_EQ(fetched_topics[index].topic_type,
                ::ambient::TopicType::kPersonal);
      EXPECT_EQ(fetched_topics[index].related_image_url, "topic2_related_url");
      EXPECT_EQ(fetched_topics[index].related_details,
                "topic2_related_details");
    } else if (fetched_topics[index].url == "topic3_url") {
      EXPECT_EQ(fetched_topics[index].url, "topic3_url");
      EXPECT_EQ(fetched_topics[index].details, "topic3_details");
      EXPECT_TRUE(fetched_topics[index].is_portrait);
      EXPECT_EQ(fetched_topics[index].topic_type,
                ::ambient::TopicType::kPersonal);
      EXPECT_EQ(fetched_topics[index].related_image_url, "topic3_related_url");
      EXPECT_EQ(fetched_topics[index].related_details,
                "topic3_related_details");
    } else if (fetched_topics[index].url == "topic4_url") {
      EXPECT_EQ(fetched_topics[index].url, "topic4_url");
      EXPECT_EQ(fetched_topics[index].details, "topic4_details");
      EXPECT_FALSE(fetched_topics[index].is_portrait);
      EXPECT_EQ(fetched_topics[index].topic_type,
                ::ambient::TopicType::kPersonal);
      EXPECT_EQ(fetched_topics[index].related_image_url, "topic5_url");
      EXPECT_EQ(fetched_topics[index].related_details, "topic5_details");
    } else {
      // Not reached.
      EXPECT_FALSE(true);
    }
  }
}

TEST_F(AmbientTopicQueueTest,
       ShouldNotPairIfNoTwoLandscapeImagesInOneCategory) {
  backend_controller()->set_custom_topic_generator(
      base::BindLambdaForTesting([](int, const gfx::Size&) {
        // Set up 1 personal landscape photo, 1 personal portrait photo, and 1
        // featured landscape photos. Will output 1 topic of 1 personal portrait
        // photo.
        std::vector<AmbientModeTopic> topics;
        topics.emplace_back(CreateTopic(
            /*url=*/"topic1_url", /*details=*/"topic1_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic2_url", /*details=*/"topic2_details",
            /*is_portrait=*/true,
            /*related_url=*/"topic2_related_url",
            /*related_details=*/"topic2_related_details",
            ::ambient::TopicType::kPersonal));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic3_url", /*details=*/"topic3_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kFeatured));
        return topics;
      }));

  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/10,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  ASSERT_THAT(fetched_topics, SizeIs(1u));

  EXPECT_EQ(fetched_topics.size(), 1u);
  EXPECT_EQ(fetched_topics[0].url, "topic2_url");
  EXPECT_EQ(fetched_topics[0].details, "topic2_details");
  EXPECT_TRUE(fetched_topics[0].is_portrait);
  EXPECT_EQ(fetched_topics[0].topic_type, ::ambient::TopicType::kPersonal);
  EXPECT_EQ(fetched_topics[0].related_image_url, "topic2_related_url");
  EXPECT_EQ(fetched_topics[0].related_details, "topic2_related_details");
}

TEST_F(AmbientTopicQueueTest, ShouldNotPairTwoLandscapeImagesInGeoCategory) {
  backend_controller()->set_custom_topic_generator(
      base::BindLambdaForTesting([](int, const gfx::Size&) {
        // Set up 2 Geo landscape photos. Will output 2 topics of Geo photos.
        std::vector<AmbientModeTopic> topics;
        topics.emplace_back(CreateTopic(
            /*url=*/"topic1_url", /*details=*/"topic1_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kGeo));
        topics.emplace_back(CreateTopic(
            /*url=*/"topic2_url", /*details=*/"topic2_details",
            /*is_portrait=*/false,
            /*related_url=*/"",
            /*related_details=*/"", ::ambient::TopicType::kGeo));
        return topics;
      }));

  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/10,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  ASSERT_THAT(fetched_topics, SizeIs(2u));

  const int index = (fetched_topics[0].url == "topic1_url") ? 0 : 1;
  EXPECT_EQ(fetched_topics[index].url, "topic1_url");
  EXPECT_EQ(fetched_topics[index].details, "topic1_details");
  EXPECT_FALSE(fetched_topics[index].is_portrait);
  EXPECT_EQ(fetched_topics[index].topic_type, ::ambient::TopicType::kGeo);
  EXPECT_EQ(fetched_topics[index].related_image_url, "");
  EXPECT_EQ(fetched_topics[index].related_details, "");

  EXPECT_EQ(fetched_topics[1 - index].url, "topic2_url");
  EXPECT_EQ(fetched_topics[1 - index].details, "topic2_details");
  EXPECT_FALSE(fetched_topics[1 - index].is_portrait);
  EXPECT_EQ(fetched_topics[1 - index].topic_type, ::ambient::TopicType::kGeo);
  EXPECT_EQ(fetched_topics[1 - index].related_image_url, "");
  EXPECT_EQ(fetched_topics[1 - index].related_details, "");
}

TEST_F(AmbientTopicQueueTest, UniformTopicSizeDistribution) {
  static constexpr gfx::Size kSize1 = gfx::Size(100, 200);
  static constexpr gfx::Size kSize2 = gfx::Size(200, 100);

  backend_controller()->set_custom_topic_generator(base::BindLambdaForTesting(
      [](int num_topics_requested, const gfx::Size& topic_size) {
        std::vector<AmbientModeTopic> topics;
        for (int i = 0; i < num_topics_requested; ++i) {
          topics.push_back(CreateTopic(
              /*url=*/base::StringPrintf("http://test-url-%s.com",
                                         topic_size.ToString().c_str()),
              /*details=*/"",
              /*is_portrait=*/false,
              /*related_url=*/"",
              /*related_details=*/"", kDefaultTopicType));
        }
        return topics;
      }));

  delegate_.SetTopicSizes({kSize1, kSize2});
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/4,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  // Fast forward some huge amount so that the topic limit should be reached
  // via scheduled refills.
  task_environment()->FastForwardBy(10 * kDefaultTopicFetchInterval);
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  EXPECT_THAT(
      fetched_topics,
      ElementsAre(TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2),
                  TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2),
                  TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2),
                  TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2),
                  TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2)));
}

TEST_F(AmbientTopicQueueTest,
       TrimsIMAXResponseForUniformTopicSizeDistribution) {
  static constexpr gfx::Size kSize1 = gfx::Size(100, 200);
  static constexpr gfx::Size kSize2 = gfx::Size(200, 100);

  backend_controller()->set_custom_topic_generator(base::BindLambdaForTesting(
      [](int num_topics_requested, const gfx::Size& topic_size) {
        int num_topics_to_return = topic_size == kSize1
                                       ? num_topics_requested / 2
                                       : num_topics_requested;
        std::vector<AmbientModeTopic> topics;
        for (int i = 0; i < num_topics_to_return; ++i) {
          topics.push_back(CreateTopic(
              /*url=*/base::StringPrintf("http://test-url-%s.com",
                                         topic_size.ToString().c_str()),
              /*details=*/"",
              /*is_portrait=*/false,
              /*related_url=*/"",
              /*related_details=*/"", kDefaultTopicType));
        }
        return topics;
      }));

  delegate_.SetTopicSizes({kSize1, kSize2});
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/4,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  EXPECT_THAT(fetched_topics, ElementsAre(TopicUrlContainsSize(kSize1),
                                          TopicUrlContainsSize(kSize2)));
}

TEST_F(AmbientTopicQueueTest, HandlesEmptyResponseForSingleTopicSize) {
  static constexpr gfx::Size kSize1 = gfx::Size(100, 200);
  static constexpr gfx::Size kSize2 = gfx::Size(200, 100);

  backend_controller()->set_custom_topic_generator(base::BindLambdaForTesting(
      [](int num_topics_requested, const gfx::Size& topic_size) {
        std::vector<AmbientModeTopic> topics;
        if (topic_size == kSize1)
          return topics;

        for (int i = 0; i < num_topics_requested; ++i) {
          topics.push_back(CreateTopic(
              /*url=*/base::StringPrintf("http://test-url-%s.com",
                                         topic_size.ToString().c_str()),
              /*details=*/"",
              /*is_portrait=*/false,
              /*related_url=*/"",
              /*related_details=*/"", kDefaultTopicType));
        }
        return topics;
      }));

  delegate_.SetTopicSizes({kSize1, kSize2});
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/4,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  ASSERT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicsAvailable));
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  EXPECT_THAT(fetched_topics, ElementsAre(TopicUrlContainsSize(kSize2)));
}

TEST_F(AmbientTopicQueueTest, HandlesManyRequestedTopicSizes) {
  static constexpr gfx::Size kSize1 = gfx::Size(100, 200);
  static constexpr gfx::Size kSize2 = gfx::Size(200, 100);
  static constexpr gfx::Size kSize3 = gfx::Size(300, 400);
  static constexpr gfx::Size kSize4 = gfx::Size(400, 300);

  backend_controller()->set_custom_topic_generator(base::BindLambdaForTesting(
      [](int num_topics_requested, const gfx::Size& topic_size) {
        std::vector<AmbientModeTopic> topics;
        for (int i = 0; i < num_topics_requested; ++i) {
          topics.push_back(CreateTopic(
              /*url=*/base::StringPrintf("http://test-url-%s.com",
                                         topic_size.ToString().c_str()),
              /*details=*/"",
              /*is_portrait=*/false,
              /*related_url=*/"",
              /*related_details=*/"", kDefaultTopicType));
        }
        return topics;
      }));

  delegate_.SetTopicSizes({kSize1, kSize2, kSize3, kSize4});
  AmbientTopicQueue queue(/*topic_fetch_limit=*/10, /*topic_fetch_size=*/2,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  // Fast forward some huge amount so that the topic limit should be reached
  // via scheduled refills.
  task_environment()->FastForwardBy(10 * kDefaultTopicFetchInterval);
  std::vector<AmbientModeTopic> fetched_topics = PopUntilEmpty(queue);
  EXPECT_THAT(
      fetched_topics,
      ElementsAre(TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2),
                  TopicUrlContainsSize(kSize3), TopicUrlContainsSize(kSize4),
                  TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2),
                  TopicUrlContainsSize(kSize3), TopicUrlContainsSize(kSize4),
                  TopicUrlContainsSize(kSize1), TopicUrlContainsSize(kSize2)));
}

TEST_F(AmbientTopicQueueTest, ZeroTopicFetchLimit) {
  AmbientTopicQueue queue(/*topic_fetch_limit=*/0, /*topic_fetch_size=*/5,
                          kDefaultTopicFetchInterval,
                          /*should_split_topics=*/false, &delegate_,
                          backend_controller());

  EXPECT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicFetchLimitReached));
  EXPECT_TRUE(queue.IsEmpty());
  task_environment()->FastForwardBy(10 * kDefaultTopicFetchInterval);
  EXPECT_THAT(WaitForTopicsAvailable(queue),
              Eq(AmbientTopicQueue::WaitResult::kTopicFetchLimitReached));
  EXPECT_TRUE(queue.IsEmpty());
}

}  // namespace
}  // namespace ash
