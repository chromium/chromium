// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/toast_data.h"

#include <string>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(ToastDataTest, InitializedWithProvidedValues) {
  ToastData data = ToastData(
      /*id=*/"some_id", ToastCatalogName::kDebugCommand, /*text=*/u"some_text",
      base::Seconds(1),
      /*visible_on_lock_screen=*/true,
      /*has_dismiss_button=*/true,
      /*custom_dismiss_text=*/u"Dismiss now");

  EXPECT_EQ(data.id, "some_id");
  EXPECT_EQ(data.catalog_name, ash::ToastCatalogName::kDebugCommand);
  EXPECT_EQ(data.text, u"some_text");
  EXPECT_EQ(data.duration, base::Seconds(1));
  EXPECT_EQ(data.visible_on_lock_screen, true);
  EXPECT_EQ(data.dismiss_text, u"Dismiss now");
}

TEST(ToastDataTest, InitializedWithDefaultValues) {
  ToastData data = ToastData(
      /*id=*/"some_id", ToastCatalogName::kDebugCommand, /*text=*/u"some_text");

  EXPECT_EQ(data.duration, ToastData::kDefaultToastDuration);
  EXPECT_EQ(data.visible_on_lock_screen, false);
  EXPECT_EQ(data.dismiss_text, std::u16string());
}

TEST(ToastDataTest, InitializedWithInfiniteDuration) {
  ToastData data = ToastData(
      /*id=*/"some_id", ToastCatalogName::kDebugCommand, /*text=*/u"some_text",
      ToastData::kInfiniteDuration);

  EXPECT_EQ(data.duration, ToastData::kInfiniteDuration);
}

TEST(ToastDataTest, EnforcesMinimumDuration) {
  ToastData data = ToastData(
      /*id=*/"some_id", ToastCatalogName::kDebugCommand, /*text=*/u"some_text",
      base::Milliseconds(1));

  EXPECT_EQ(data.duration, ToastData::kMinimumDuration);
}

}  // namespace ash
