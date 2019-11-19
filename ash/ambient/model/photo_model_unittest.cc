// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/photo_model.h"

#include "ash/ambient/model/photo_model_observer.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// This class has a local in memory cache of downloaded photos. This is the max
// number of photos before and after currently shown image.
constexpr int kImageBufferLength = 3;

}  // namespace

class PhotoModelTest : public AshTestBase {
 public:
  PhotoModelTest() = default;
  ~PhotoModelTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    model_ = std::make_unique<PhotoModel>();
    model_->set_buffer_length_for_testing(kImageBufferLength);
  }

  void TearDown() override {
    model_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<PhotoModel> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhotoModelTest);
};

// Test adding the first image.
TEST_F(PhotoModelTest, AddFirstImage) {
  gfx::ImageSkia first_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  model_->AddNextImage(first_image);
  EXPECT_TRUE(model_->GetPrevImage().isNull());
  EXPECT_TRUE(model_->GetCurrImage().BackedBySameObjectAs(first_image));
  EXPECT_TRUE(model_->GetNextImage().isNull());
}

// Test adding the second image.
TEST_F(PhotoModelTest, AddSecondImage) {
  gfx::ImageSkia first_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  gfx::ImageSkia second_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

  // First |AddNextImage| will set |current_image_index_| to 0.
  model_->AddNextImage(first_image);
  model_->AddNextImage(second_image);
  EXPECT_TRUE(model_->GetPrevImage().isNull());
  EXPECT_TRUE(model_->GetCurrImage().BackedBySameObjectAs(first_image));
  EXPECT_TRUE(model_->GetNextImage().BackedBySameObjectAs(second_image));

  // Increment the |current_image_index_| to 1.
  model_->ShowNextImage();
  EXPECT_TRUE(model_->GetPrevImage().BackedBySameObjectAs(first_image));
  EXPECT_TRUE(model_->GetCurrImage().BackedBySameObjectAs(second_image));
  EXPECT_TRUE(model_->GetNextImage().isNull());
}

// Test adding the third image.
TEST_F(PhotoModelTest, AddThirdImage) {
  gfx::ImageSkia first_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  gfx::ImageSkia second_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  gfx::ImageSkia third_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

  // The default |current_image_index_| is 0.
  model_->AddNextImage(first_image);
  model_->AddNextImage(second_image);
  model_->AddNextImage(third_image);
  EXPECT_TRUE(model_->GetPrevImage().isNull());
  EXPECT_TRUE(model_->GetCurrImage().BackedBySameObjectAs(first_image));
  EXPECT_TRUE(model_->GetNextImage().BackedBySameObjectAs(second_image));

  // Increment the |current_image_index_| to 1.
  model_->ShowNextImage();
  EXPECT_TRUE(model_->GetPrevImage().BackedBySameObjectAs(first_image));
  EXPECT_TRUE(model_->GetCurrImage().BackedBySameObjectAs(second_image));
  EXPECT_TRUE(model_->GetNextImage().BackedBySameObjectAs(third_image));

  // Pop the |images_| front and keep the |current_image_index_| to 1.
  model_->ShowNextImage();
  EXPECT_TRUE(model_->GetPrevImage().BackedBySameObjectAs(second_image));
  EXPECT_TRUE(model_->GetCurrImage().BackedBySameObjectAs(third_image));
  EXPECT_TRUE(model_->GetNextImage().isNull());

  // ShowNextImage() will early return.
  model_->ShowNextImage();
  EXPECT_TRUE(model_->GetPrevImage().BackedBySameObjectAs(second_image));
  EXPECT_TRUE(model_->GetCurrImage().BackedBySameObjectAs(third_image));
  EXPECT_TRUE(model_->GetNextImage().isNull());
}

}  // namespace ash
