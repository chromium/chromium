// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_backend_model.h"

#include <memory>
#include <string>

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/scoped_observation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

namespace ash {

namespace {
class MockAmbientBackendModelObserver : public AmbientBackendModelObserver {
 public:
  MockAmbientBackendModelObserver() = default;
  ~MockAmbientBackendModelObserver() override = default;

  MOCK_METHOD(void, OnImagesFailed, (), (override));
  MOCK_METHOD(void, OnImagesReady, (), (override));
  MOCK_METHOD(void, OnImageAdded, (), (override));
};

}  // namespace

class AmbientBackendModelTest : public AshTestBase {
 public:
  AmbientBackendModelTest() = default;
  AmbientBackendModelTest(const AmbientBackendModelTest&) = delete;
  AmbientBackendModelTest& operator=(AmbientBackendModelTest&) = delete;
  ~AmbientBackendModelTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ambient_backend_model_ = std::make_unique<AmbientBackendModel>();
  }

  void TearDown() override {
    ambient_backend_model_.reset();
    AshTestBase::TearDown();
  }

  // Adds n test images to the model.
  void AddNTestImages(int n) {
    while (n > 0) {
      PhotoWithDetails test_detailed_image;
      test_detailed_image.photo =
          gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
      test_detailed_image.details = std::string("fake-photo-attribution");
      ambient_backend_model()->AddNextImage(std::move(test_detailed_image));
      n--;
    }
  }

  // Returns whether the image and its details are equivalent to the test
  // detailed image.
  bool EqualsToTestImage(const PhotoWithDetails& detailed_image) {
    gfx::ImageSkia test_image =
        gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
    return !detailed_image.IsNull() &&
           gfx::test::AreBitmapsEqual(*(detailed_image.photo).bitmap(),
                                      *test_image.bitmap()) &&
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
    return ambient_backend_model_->GetNextImage();
  }

  PhotoWithDetails GetCurrentImage() {
    return ambient_backend_model_->GetCurrentImage();
  }

  int failure_count() { return ambient_backend_model_->failures_; }

 private:
  std::unique_ptr<AmbientBackendModel> ambient_backend_model_;
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
  const base::TimeDelta interval = base::TimeDelta::FromMinutes(1);
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

}  // namespace ash
