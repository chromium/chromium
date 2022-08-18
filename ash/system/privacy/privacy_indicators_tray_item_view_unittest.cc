// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_tray_item_view.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class PrivacyIndicatorsTrayItemViewTest : public AshTestBase {
 public:
  PrivacyIndicatorsTrayItemViewTest() = default;
  PrivacyIndicatorsTrayItemViewTest(const PrivacyIndicatorsTrayItemViewTest&) =
      delete;
  PrivacyIndicatorsTrayItemViewTest& operator=(
      const PrivacyIndicatorsTrayItemViewTest&) = delete;
  ~PrivacyIndicatorsTrayItemViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    privacy_indicators_view_ =
        std::make_unique<PrivacyIndicatorsTrayItemView>(GetPrimaryShelf());
  }

  std::u16string GetTooltipText() {
    return privacy_indicators_view_->GetTooltipText(gfx::Point());
  }

 protected:
  PrivacyIndicatorsTrayItemView* privacy_indicators_view() {
    return privacy_indicators_view_.get();
  }

  views::ImageView* camera_icon() {
    return privacy_indicators_view_->camera_icon_;
  }
  views::ImageView* microphone_icon() {
    return privacy_indicators_view_->microphone_icon_;
  }

 private:
  std::unique_ptr<PrivacyIndicatorsTrayItemView> privacy_indicators_view_;
};

TEST_F(PrivacyIndicatorsTrayItemViewTest, IconsVisibility) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  privacy_indicators_view()->Update(/*camera_is_used=*/true,
                                    /*microphone_is_used=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  privacy_indicators_view()->Update(/*camera_is_used=*/false,
                                    /*microphone_is_used=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  privacy_indicators_view()->Update(/*camera_is_used=*/true,
                                    /*microphone_is_used=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  privacy_indicators_view()->Update(/*camera_is_used=*/false,
                                    /*microphone_is_used=*/false);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, TooltipText) {
  EXPECT_EQ(std::u16string(), GetTooltipText());

  privacy_indicators_view()->Update(/*camera_is_used=*/true,
                                    /*microphone_is_used=*/false);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA),
            GetTooltipText());

  privacy_indicators_view()->Update(/*camera_is_used=*/false,
                                    /*microphone_is_used=*/true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC),
            GetTooltipText());

  privacy_indicators_view()->Update(/*camera_is_used=*/true,
                                    /*microphone_is_used=*/true);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC),
      GetTooltipText());

  privacy_indicators_view()->Update(/*camera_is_used=*/false,
                                    /*microphone_is_used=*/false);
  EXPECT_EQ(std::u16string(), GetTooltipText());
}

}  // namespace ash
