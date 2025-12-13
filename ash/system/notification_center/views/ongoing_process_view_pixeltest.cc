// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {
constexpr char16_t kEmptyString[] = u"";
constexpr char16_t kShortTitleString[] = u"Short Title";
constexpr char16_t kLongTitleString[] =
    u"Test Notification's Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Long Multiline Title";

constexpr char16_t kLongMessageString[] =
    u"Test Notification's Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Long Message";

enum ButtonType {
  kNone,
  kIconButton,
  kPillButton,
};

}  // namespace

class OngoingProcessViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::
              tuple<const char16_t*, const char16_t*, ButtonType, bool, bool>> {
 public:
  const std::u16string GetTitle() const { return std::get<0>(GetParam()); }
  const std::u16string GetMessage() const { return std::get<1>(GetParam()); }
  ButtonType GetButtonType() const { return std::get<2>(GetParam()); }
  bool IsNotificationWidthIncreaseEnabled() const {
    return std::get<3>(GetParam());
  }
  bool IsSystemBlurEnabled() const { return std::get<4>(GetParam()); }

  std::string GenerateScreenshotName(const std::string& title) override {
    std::string test_name = title;

    test_name +=
        (GetTitle() == kShortTitleString) ? "ShortTitle_" : "LongTitle_";

    test_name += (GetMessage().empty()) ? "EmptyMessage_" : "LongMessage_";

    switch (GetButtonType()) {
      case ButtonType::kNone:
        test_name += "_NoButton";
        break;
      case ButtonType::kIconButton:
        test_name += "_IconButton";
        break;
      case ButtonType::kPillButton:
        test_name += "_PillButton";
        break;
    }

    test_name += IsNotificationWidthIncreaseEnabled() ? "_WidthIncreased"
                                                      : "_NormalWidth";

    return test_name;
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  // AshPixelTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = IsSystemBlurEnabled();
    return init_params;
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kOngoingProcesses};

    if (IsNotificationWidthIncreaseEnabled()) {
      enabled_features.push_back(
          chromeos::features::kNotificationWidthIncrease);
    }

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kOngoingProcesses, true},
         {chromeos::features::kNotificationWidthIncrease,
          IsNotificationWidthIncreaseEnabled() ? true : false}});

    AshTestBase::SetUp();

    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

  std::unique_ptr<NotificationCenterTestApi> test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OngoingProcessViewPixelTest,
    testing::Combine(
        /*GetTitle()=*/testing::ValuesIn({kShortTitleString, kLongTitleString}),
        /*GetMessage()=*/
        testing::ValuesIn({kEmptyString, kLongMessageString}),
        /*GetButtonType()=*/
        testing::ValuesIn({ButtonType::kNone, ButtonType::kIconButton,
                           ButtonType::kPillButton}),
        /*IsNotificationWidthIncreaseEnabled()=*/testing::Bool(),
        /*IsSystemBlurEnabled()=*/testing::Bool()));

TEST_P(OngoingProcessViewPixelTest, MultilineLabels) {
  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  switch (GetButtonType()) {
    case ButtonType::kNone:
      // No buttons to add
      break;
    case ButtonType::kIconButton:
      optional_fields.buttons.emplace_back();
      optional_fields.buttons.back().vector_icon = &kChevronDownSmallIcon;
      optional_fields.buttons.back().accessible_name = u"Icon Button";
      break;
    case ButtonType::kPillButton:
      optional_fields.buttons.emplace_back((u"Pill Button"));
      break;
  }

  const std::string id = test_api()->AddCustomNotification(
      GetTitle(), GetMessage(), ui::ImageModel(), u"", GURL(),
      message_center::NotifierId(), optional_fields);
  test_api()->ToggleBubble();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("OngoingProcessView"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 4 : 0,
      test_api()->GetNotificationCenterView()));
}

}  // namespace ash
