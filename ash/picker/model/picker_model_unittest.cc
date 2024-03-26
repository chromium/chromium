// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/public/cpp/picker/picker_category.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

TEST(PickerModel, AvailableCategoriesWithNoFocus) {
  PickerModel model(nullptr);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kEmojis, PickerCategory::kSymbols,
                  PickerCategory::kEmoticons, PickerCategory::kGifs,
                  PickerCategory::kOpenTabs, PickerCategory::kBrowsingHistory,
                  PickerCategory::kBookmarks, PickerCategory::kDriveFiles,
                  PickerCategory::kLocalFiles, PickerCategory::kEditor,
                  PickerCategory::kDatesTimes, PickerCategory::kUnitsMaths,
                  PickerCategory::kClipboard));
}

TEST(PickerModel, AvailableCategoriesWithNoSelectedText) {
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0));

  PickerModel model(&client);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kEmojis, PickerCategory::kSymbols,
                  PickerCategory::kEmoticons, PickerCategory::kGifs,
                  PickerCategory::kOpenTabs, PickerCategory::kBrowsingHistory,
                  PickerCategory::kBookmarks, PickerCategory::kDriveFiles,
                  PickerCategory::kLocalFiles, PickerCategory::kEditor,
                  PickerCategory::kDatesTimes, PickerCategory::kUnitsMaths,
                  PickerCategory::kClipboard));
}

TEST(PickerModel, AvailableCategoriesWithSelectedText) {
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  PickerModel model(&client);
  EXPECT_THAT(model.GetAvailableCategories(),
              ElementsAre(PickerCategory::kEditor));
}

}  // namespace
}  // namespace ash
