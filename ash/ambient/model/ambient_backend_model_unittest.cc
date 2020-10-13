// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_backend_model.h"

#include <memory>
#include <string>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/test/ash_test_base.h"
#include "base/scoped_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {
class MockAmbientBackendModelObserver : public AmbientBackendModelObserver {
 public:
  MockAmbientBackendModelObserver() = default;
  ~MockAmbientBackendModelObserver() override = default;

  MOCK_METHOD(void, OnImagesFailed, (), (override));
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

  base::TimeDelta GetPhotoRefreshInterval() {
    return ambient_backend_model()->GetPhotoRefreshInterval();
  }

  void SetPhotoRefreshInterval(const base::TimeDelta& interval) {
    ambient_backend_model()->SetPhotoRefreshInterval(interval);
  }

  AmbientBackendModel* ambient_backend_model() {
    return ambient_backend_model_.get();
  }

  PhotoWithDetails GetNextImage() {
    return ambient_backend_model_->GetNextImage();
  }

  int failure_count() { return ambient_backend_model_->failures_; }

 private:
  std::unique_ptr<AmbientBackendModel> ambient_backend_model_;
};

// Test adding the first image.
TEST_F(AmbientBackendModelTest, AddFirstImage) {
  AddNTestImages(1);

  EXPECT_TRUE(EqualsToTestImage(GetNextImage()));
}

// Test adding the second image.
TEST_F(AmbientBackendModelTest, AddSecondImage) {
  AddNTestImages(1);
  EXPECT_TRUE(EqualsToTestImage(GetNextImage()));

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
  ScopedObserver<AmbientBackendModel, AmbientBackendModelObserver> scoped_obs{
      &observer};

  scoped_obs.Add(ambient_backend_model());

  EXPECT_CALL(observer, OnImagesFailed).Times(1);

  for (int i = 0; i < kMaxConsecutiveReadPhotoFailures; i++) {
    ambient_backend_model()->AddImageFailure();
  }
}

TEST_F(AmbientBackendModelTest, ShouldResetFailuresOnAddImage) {
  testing::NiceMock<MockAmbientBackendModelObserver> observer;
  ScopedObserver<AmbientBackendModel, AmbientBackendModelObserver> scoped_obs{
      &observer};

  scoped_obs.Add(ambient_backend_model());

  EXPECT_CALL(observer, OnImagesFailed).Times(0);

  for (int i = 0; i < kMaxConsecutiveReadPhotoFailures - 1; i++) {
    ambient_backend_model()->AddImageFailure();
  }

  EXPECT_EQ(failure_count(), kMaxConsecutiveReadPhotoFailures - 1);

  AddNTestImages(1);

  EXPECT_EQ(failure_count(), 0);
}

}  // namespace ash
