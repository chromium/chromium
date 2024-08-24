// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/show_notification_action_performer.h"

#include <memory>
#include <optional>

#include "ash/test/ash_test_base.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/growth/mock_ui_performer_observer.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "url/gurl.h"

namespace {

constexpr char kShowNotificationParamTemplate[] = R"(
    {
      "title": "%s",
      "message": "%s",
      "sourceIcon": {
        "builtInVectorIcon": 0
      },
      "image": {
        "builtInImage": 2
      },
      "shouldLogCrOSEvents": true
    }
)";
constexpr char kTestTitle[] = "test title";
constexpr char kTestMessage[] = "test message";
constexpr int kTestCampaignId = 10;
constexpr char kNotificationIdTemplate[] = "growth_campaign_%d";

}  // namespace

class ShowNotificationActionPerformerTest : public ash::AshTestBase {
 public:
  ShowNotificationActionPerformerTest() = default;
  ShowNotificationActionPerformerTest(
      const ShowNotificationActionPerformerTest&) = delete;
  ShowNotificationActionPerformerTest& operator=(
      const ShowNotificationActionPerformerTest&) = delete;
  ~ShowNotificationActionPerformerTest() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    action_ = std::make_unique<ShowNotificationActionPerformer>();
    scoped_observation_.Observe(action_.get());
    message_center_ = message_center::MessageCenter::Get();

    // No notifications should have been posted yet.
    ASSERT_EQ(0u, message_center_->NotificationCount());
  }

  void TearDown() override {
    message_center_ = nullptr;
    AshTestBase::TearDown();
    scoped_observation_.Reset();
  }

  ShowNotificationActionPerformer& action() { return *action_; }

  void RunShowNotificationActionPerformerCallback(
      growth::ActionResult result,
      std::optional<growth::ActionResultReason> reason) {
    if (result == growth::ActionResult::kSuccess) {
      std::move(action_success_closure_).Run();
    } else {
      std::move(action_failed_closure_).Run();
    }
  }

  bool VerifyActionResult(bool success) {
    if (success) {
      action_success_run_loop_.Run();
    } else {
      action_failed_run_loop_.Run();
    }
    return true;
  }

 protected:
  raw_ptr<message_center::MessageCenter> message_center_;
  MockUiPerformerObserver mock_observer_;

 private:
  base::RunLoop action_success_run_loop_;
  base::RunLoop action_failed_run_loop_;

  base::OnceClosure action_success_closure_ =
      action_success_run_loop_.QuitClosure();
  base::OnceClosure action_failed_closure_ =
      action_failed_run_loop_.QuitClosure();

  std::unique_ptr<ShowNotificationActionPerformer> action_;
  growth::CampaignsLogger logger_;
  base::ScopedObservation<UiActionPerformer, UiActionPerformer::Observer>
      scoped_observation_{&mock_observer_};
};

TEST_F(ShowNotificationActionPerformerTest, TestValidParams) {
  const auto valid_params = base::StringPrintf(kShowNotificationParamTemplate,
                                               kTestTitle, kTestMessage);
  auto value = base::JSONReader::Read(valid_params);
  ASSERT_TRUE(value.has_value());
  EXPECT_CALL(mock_observer_, OnReadyToLogImpression(
                                  testing::Eq(kTestCampaignId),
                                  testing::Eq(std::nullopt), testing::Eq(true)))
      .Times(1);

  action().Run(
      /*campaign_id=*/kTestCampaignId, /*group_id=*/std::nullopt,
      &value->GetDict(),
      base::BindOnce(&ShowNotificationActionPerformerTest::
                         RunShowNotificationActionPerformerCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));

  const auto notification_id =
      base::StringPrintf(kNotificationIdTemplate, kTestCampaignId);
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(notification->title(), base::UTF8ToUTF16(std::string(kTestTitle)));
  EXPECT_EQ(notification->message(),
            base::UTF8ToUTF16(std::string(kTestMessage)));
  EXPECT_STREQ(chromeos::kRedeemIcon.name,
               notification->vector_small_image().name);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto& expected_image =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_GROWTH_FRAMEWORK_SPARK_REBUY_PNG);
  EXPECT_EQ(expected_image, notification->rich_notification_data().image);
#else
  EXPECT_EQ(gfx::Image(), notification->rich_notification_data().image);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

TEST_F(ShowNotificationActionPerformerTest, TestUnrecognizedImage) {
  auto* valid_params = R"(
    {
      "title": "test title",
      "message": "test message",
      "sourceIcon": {
        "builtInVectorIcon": 0
      },
      "image": {
        "builtInImage": 20000
      }
    }
)";
  auto value = base::JSONReader::Read(valid_params);
  ASSERT_TRUE(value.has_value());
  EXPECT_CALL(
      mock_observer_,
      OnReadyToLogImpression(testing::Eq(kTestCampaignId),
                             testing::Eq(std::nullopt), testing::Eq(false)))
      .Times(1);

  action().Run(
      /*campaign_id=*/kTestCampaignId, /*group_id=*/std::nullopt,
      &value->GetDict(),
      base::BindOnce(&ShowNotificationActionPerformerTest::
                         RunShowNotificationActionPerformerCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));

  const auto notification_id =
      base::StringPrintf(kNotificationIdTemplate, kTestCampaignId);
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(notification->title(), base::UTF8ToUTF16(std::string(kTestTitle)));
  EXPECT_EQ(notification->message(),
            base::UTF8ToUTF16(std::string(kTestMessage)));
  EXPECT_STREQ(chromeos::kRedeemIcon.name,
               notification->vector_small_image().name);
  EXPECT_EQ(gfx::Image(), notification->rich_notification_data().image);
}

TEST_F(ShowNotificationActionPerformerTest, TestInvalidParams) {
  auto* const invalid_params = "{}";
  auto value = base::JSONReader::Read(invalid_params);
  ASSERT_TRUE(value.has_value());
  EXPECT_CALL(mock_observer_, OnReadyToLogImpression(
                                  testing::Eq(kTestCampaignId),
                                  testing::Eq(std::nullopt), testing::Eq(true)))
      .Times(0);

  action().Run(
      /*campaign_id=*/kTestCampaignId, /*group_id=*/std::nullopt,
      &value->GetDict(),
      base::BindOnce(&ShowNotificationActionPerformerTest::
                         RunShowNotificationActionPerformerCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      base::StringPrintf(kNotificationIdTemplate, kTestCampaignId)));
}
