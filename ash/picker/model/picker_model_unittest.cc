// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/picker/model/picker_category.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

TEST(PickerModel, AvailableCategories) {
  PickerModel model;
  EXPECT_THAT(model.GetAvailableCategories(),
              ElementsAre(PickerCategory::kEmojis, PickerCategory::kSymbols,
                          PickerCategory::kEmoticons, PickerCategory::kGifs));
}

}  // namespace
}  // namespace ash
