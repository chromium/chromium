// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_backend_model.h"

#include <memory>
#include <string>

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/test/ambient_test_util.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/scoped_observation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

namespace ash {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Not;

namespace {
class MockAmbientBackendModelObserver : public AmbientBackendModelObserver {
 public:
  MockAmbientBackendModelObserver() = default;
  ~MockAmbientBackendModelObserver() override = default;

  MOCK_METHOD(void, OnImagesFailed, (), (override));
  MOCK_METHOD(void, OnImagesReady, (), (override));
  MOCK_METHOD(void, OnImageAdded, (), (override));
};

MATCHER_P2(HasPrimaryElements, url, details, "") {
  return arg.url == url && arg.details == details;
}

MATCHER_P(MatchesPhotosInTopic, expected_topic, "") {
  if (arg.photo.isNull() != expected_topic.photo.isNull() ||
      arg.related_photo.isNull() != expected_topic.related_photo.isNull()) {
    return false;
  }

  if (!arg.photo.isNull() &&
      !gfx::test::AreBitmapsEqual(*arg.photo.bitmap(),
                                  *expected_topic.photo.bitmap())) {
    return false;
  }

  if (!arg.related_photo.isNull() &&
      !gfx::test::AreBitmapsEqual(*arg.related_photo.bitmap(),
                                  *expected_topic.related_photo.bitmap())) {
    return false;
  }
  return true;
}

}  // namespace

class AmbientBackendModelTest : public AshTestBase {
 protected:
  AmbientBackendModelTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  AmbientBackendModelTest(const AmbientBackendModelTest&) = delete;
  AmbientBackendModelTest& operator=(AmbientBackendModelTest&) = delete;
  ~AmbientBackendModelTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ambient_backend_model_ = std::make_unique<AmbientBackendModel>(
        CreateAmbientSlideshowPhotoConfig());
  }

  void TearDown() override {
    ambient_backend_model_.reset();
    AshTestBase::TearDown();
  }

  PhotoWithDetails CreateTestImage() {
    PhotoWithDetails test_detailed_image;
    test_detailed_image.photo =
        gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
    test_detailed_image.details = std::string("fake-photo-attribution");
    return test_detailed_image;
  }

  // Adds n test images to the model.
  void AddNTestImages(int n) {
    while (n > 0) {
      ambient_backend_model()->AddNextImage(CreateTestImage());
      n--;
    }
  }

  // Returns whether the image and its details are equivalent to the test
  // detailed image.
  bool EqualsToTestImage(const PhotoWithDetails& detailed_image) {
    return !detailed_image.IsNull() &&
           gfx::test::AreBitmapsEqual(*(detailed_image.photo).bitmap(),
                                      *CreateTestImage().photo.bitmap()) &&
           (detailed_image.details == std::string("fake-photo-attribution"));
  }

  // Returns whether the image is null.
  bool IsNullImage(const gfx::ImageSkia& image) { return image.isNull(); }

  base::TimeDelta GetPhotoRefreshInterval() const {
    return ambient_backend_model()->GetPhotoRefreshInterval();
  }

  void SetPhotoRefreshInterval(const base::TimeDelta& interval) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    prefs->SetInteger(ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
                      interval.InSeconds());
  }

  AmbientBackendModel* ambient_backend_model() const {
    return ambient_backend_model_.get();
  }

  PhotoWithDetails GetNextImage() {
    PhotoWithDetails next_image;
    ambient_backend_model_->GetCurrentAndNextImages(/*current_image=*/nullptr,
                                                    &next_image);
    return next_image;
  }

  PhotoWithDetails GetCurrentImage() {
    PhotoWithDetails current_image;
    ambient_backend_model_->GetCurrentAndNextImages(&current_image,
                                                    /*next_image=*/nullptr);
    return current_image;
  }

  int failure_count() { return ambient_backend_model_->failures_; }

  std::unique_ptr<AmbientBackendModel> ambient_backend_model_;
};

// For simplicity and consistency, the AmbientAnimationPhotoConfig has 2 assets
// in all test cases.
class AmbientBackendModelTestWithAnimationConfig
    : public AmbientBackendModelTest {
 protected:
  static constexpr int kNumAssetsInAnimation = 2;

  void SetUp() override {
    AshTestBase::SetUp();
    ambient_backend_model_ = std::make_unique<AmbientBackendModel>(
        GenerateAnimationConfigWithNAssets(kNumAssetsInAnimation));
  }
};

class AmbientBackendModelTestWithEmptyConfig : public AmbientBackendModelTest {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();
    ambient_backend_model_ =
        std::make_unique<AmbientBackendModel>(AmbientPhotoConfig());
  }
};

// Test adding the first image.
TEST_F(AmbientBackendModelTest, AddFirstImage) {
  AddNTestImages(1);

  EXPECT_TRUE(EqualsToTestImage(GetCurrentImage()));
  EXPECT_TRUE(GetNextImage().IsNull());
}

// Test adding the second image.
TEST_F(AmbientBackendModelTest, AddSecondImage) {
  AddNTestImages(1);
  EXPECT_TRUE(EqualsToTestImage(GetCurrentImage()));
  EXPECT_TRUE(GetNextImage().IsNull());

  AddNTestImages(1);
  EXPECT_TRUE(EqualsToTestImage(GetNextImage()));
}

// Test the photo refresh interval is expected.
TEST_F(AmbientBackendModelTest, ShouldReturnExpectedPhotoRefreshInterval) {
  // Should fetch image immediately.
  EXPECT_EQ(GetPhotoRefreshInterval(), base::TimeDelta());

  AddNTestImages(1);
  // Should fetch image immediately.
  EXPECT_EQ(GetPhotoRefreshInterval(), base::TimeDelta());

  AddNTestImages(1);
  // Has enough images. Will fetch more image at the |photo_refresh_interval_|,
  // which is |kPhotoRefreshInterval| by default.
  EXPECT_EQ(GetPhotoRefreshInterval(), kPhotoRefreshInterval);

  // Change the photo refresh interval.
  const base::TimeDelta interval = base::Minutes(1);
  SetPhotoRefreshInterval(interval);
  // The refresh interval will be the set value.
  EXPECT_EQ(GetPhotoRefreshInterval(), interval);
}

TEST_F(AmbientBackendModelTest, ShouldNotifyObserversIfImagesFailed) {
  ambient_backend_model()->Clear();
  testing::NiceMock<MockAmbientBackendModelObserver> observer;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_obs{&observer};

  scoped_obs.Observe(ambient_backend_model());

  EXPECT_CALL(observer, OnImagesFailed).Times(1);

  for (int i = 0; i < kMaxConsecutiveReadPhotoFailures; i++) {
    ambient_backend_model()->AddImageFailure();
  }
}

TEST_F(AmbientBackendModelTest, ShouldResetFailuresOnAddImage) {
  testing::NiceMock<MockAmbientBackendModelObserver> observer;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_obs{&observer};

  scoped_obs.Observe(ambient_backend_model());

  EXPECT_NO_CALLS(observer, OnImagesFailed);

  for (int i = 0; i < kMaxConsecutiveReadPhotoFailures - 1; i++) {
    ambient_backend_model()->AddImageFailure();
  }

  EXPECT_EQ(failure_count(), kMaxConsecutiveReadPhotoFailures - 1);

  AddNTestImages(1);

  EXPECT_EQ(failure_count(), 0);
}

TEST_F(AmbientBackendModelTest, ShouldNotifyObserversOnImagesReady) {
  testing::NiceMock<MockAmbientBackendModelObserver> observer;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_obs{&observer};

  scoped_obs.Observe(ambient_backend_model());

  EXPECT_CALL(observer, OnImageAdded).Times(1);
  EXPECT_NO_CALLS(observer, OnImagesReady);
  AddNTestImages(1);

  EXPECT_CALL(observer, OnImageAdded).Times(1);
  EXPECT_CALL(observer, OnImagesReady).Times(1);
  AddNTestImages(1);
}

TEST_F(AmbientBackendModelTest, ShouldNotifyObserversOnImageAdded) {
  testing::NiceMock<MockAmbientBackendModelObserver> observer;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_obs{&observer};

  scoped_obs.Observe(ambient_backend_model());

  EXPECT_CALL(observer, OnImagesReady).Times(1);
  EXPECT_CALL(observer, OnImageAdded).Times(2);
  AddNTestImages(2);

  EXPECT_CALL(observer, OnImageAdded).Times(3);
  AddNTestImages(3);
}

TEST_F(AmbientBackendModelTest, DoesNotTimeoutIfMinimumTopicsUnavailable) {
  testing::NiceMock<MockAmbientBackendModelObserver> observer;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_obs{&observer};
  scoped_obs.Observe(ambient_backend_model());

  AddNTestImages(1);
  ASSERT_FALSE(ambient_backend_model_->ImagesReady());

  EXPECT_CALL(observer, OnImagesReady).Times(0);
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(ambient_backend_model_->ImagesReady());
}

TEST_F(AmbientBackendModelTest, SwitchesBetweenConfigs) {
  // Initially in slideshow config (requires 2 images to be "ready").
  AddNTestImages(1);
  AddNTestImages(1);
  ASSERT_TRUE(ambient_backend_model_->ImagesReady());

  // Switch to animation config that requires 4 photos to be "ready" instead of
  // 2.
  ambient_backend_model_->SetPhotoConfig(GenerateAnimationConfigWithNAssets(4));
  ASSERT_FALSE(ambient_backend_model_->ImagesReady());
  AddNTestImages(1);
  AddNTestImages(1);
  ASSERT_FALSE(ambient_backend_model_->ImagesReady());
  AddNTestImages(1);
  AddNTestImages(1);
  EXPECT_TRUE(ambient_backend_model_->ImagesReady());

  // Switch back to slideshow again.
  ambient_backend_model_->SetPhotoConfig(CreateAmbientSlideshowPhotoConfig());
  ASSERT_FALSE(ambient_backend_model_->ImagesReady());
  AddNTestImages(1);
  AddNTestImages(1);
  EXPECT_TRUE(ambient_backend_model_->ImagesReady());
}

TEST_F(AmbientBackendModelTestWithAnimationConfig, ImagesReady) {
  EXPECT_FALSE(ambient_backend_model_->ImagesReady());
  AddNTestImages(1);
  EXPECT_FALSE(ambient_backend_model_->ImagesReady());
  AddNTestImages(1);
  EXPECT_TRUE(ambient_backend_model_->ImagesReady());
  AddNTestImages(1);
  EXPECT_TRUE(ambient_backend_model_->ImagesReady());
}

TEST_F(AmbientBackendModelTestWithAnimationConfig,
       TimesOutWithMinimumTopicsAvailable) {
  testing::NiceMock<MockAmbientBackendModelObserver> observer;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_obs{&observer};
  scoped_obs.Observe(ambient_backend_model());

  AddNTestImages(1);
  ASSERT_FALSE(ambient_backend_model_->ImagesReady());

  EXPECT_CALL(observer, OnImagesReady);
  task_environment()->FastForwardBy(base::Seconds(15));
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(ambient_backend_model_->ImagesReady());

  EXPECT_CALL(observer, OnImagesReady).Times(0);
  AddNTestImages(1);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(ambient_backend_model_->ImagesReady());
}

TEST_F(AmbientBackendModelTestWithAnimationConfig, IsHashDuplicate) {
  EXPECT_FALSE(ambient_backend_model_->IsHashDuplicate("dummy-hash"));
  PhotoWithDetails topic;
  topic.photo = gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  topic.hash = "topic-0-hash";
  ambient_backend_model_->AddNextImage(topic);
  EXPECT_FALSE(ambient_backend_model_->IsHashDuplicate("dummy-hash"));
  EXPECT_TRUE(ambient_backend_model_->IsHashDuplicate("topic-0-hash"));
  topic.hash = "topic-1-hash";
  ambient_backend_model_->AddNextImage(topic);
  EXPECT_FALSE(ambient_backend_model_->IsHashDuplicate("topic-0-hash"));
  EXPECT_TRUE(ambient_backend_model_->IsHashDuplicate("topic-1-hash"));
}

TEST_F(AmbientBackendModelTestWithAnimationConfig, Clear) {
  AddNTestImages(2);
  ASSERT_THAT(ambient_backend_model_->all_decoded_topics(), Not(IsEmpty()));
  ASSERT_TRUE(ambient_backend_model_->ImagesReady());
  ambient_backend_model_->Clear();
  EXPECT_THAT(ambient_backend_model_->all_decoded_topics(), IsEmpty());
  EXPECT_FALSE(ambient_backend_model_->ImagesReady());
}

TEST_F(AmbientBackendModelTestWithAnimationConfig,
       GetAllDecodedTopicsBeforeImagesReady) {
  AddNTestImages(1);
  EXPECT_THAT(ambient_backend_model_->all_decoded_topics(),
              ElementsAre(MatchesPhotosInTopic(CreateTestImage())));
}

TEST_F(AmbientBackendModelTestWithAnimationConfig,
       GetAllDecodedTopicsAfterImagesReady) {
  AddNTestImages(2);
  EXPECT_THAT(ambient_backend_model_->all_decoded_topics(),
              ElementsAre(MatchesPhotosInTopic(CreateTestImage()),
                          MatchesPhotosInTopic(CreateTestImage())));
}

TEST_F(AmbientBackendModelTestWithAnimationConfig, RotatesTopics) {
  PhotoWithDetails topic_0;
  topic_0.photo = gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  ambient_backend_model_->AddNextImage(topic_0);

  PhotoWithDetails topic_1;
  topic_1.photo = gfx::test::CreateImageSkia(/*width=*/20, /*height=*/20);
  ambient_backend_model_->AddNextImage(topic_1);
  EXPECT_THAT(ambient_backend_model_->all_decoded_topics(),
              ElementsAre(MatchesPhotosInTopic(topic_0),
                          MatchesPhotosInTopic(topic_1)));

  PhotoWithDetails topic_2;
  topic_2.photo = gfx::test::CreateImageSkia(/*width=*/30, /*height=*/30);
  ambient_backend_model_->AddNextImage(topic_2);
  EXPECT_THAT(ambient_backend_model_->all_decoded_topics(),
              ElementsAre(MatchesPhotosInTopic(topic_1),
                          MatchesPhotosInTopic(topic_2)));

  PhotoWithDetails topic_3;
  topic_3.photo = gfx::test::CreateImageSkia(/*width=*/40, /*height=*/40);
  ambient_backend_model_->AddNextImage(topic_3);
  EXPECT_THAT(ambient_backend_model_->all_decoded_topics(),
              ElementsAre(MatchesPhotosInTopic(topic_2),
                          MatchesPhotosInTopic(topic_3)));
}

TEST_F(AmbientBackendModelTestWithEmptyConfig, ImagesReadyImmediately) {
  EXPECT_TRUE(ambient_backend_model_->ImagesReady());
}

}  // namespace ash
