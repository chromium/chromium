// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "media/base/picture_in_picture_events_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"

using testing::_;
using testing::AtLeast;
using testing::Return;

using UiResult = AutoPipSettingView::UiResult;
using AutoPipReason = media::PictureInPictureEventsInfo::AutoPipReason;

namespace {

const char kVideoConferencingHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "VideoConferencing.PromptResultV2";
const char kMediaPlaybackHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "MediaPlayback.PromptResultV2";
const char kBrowserInitiatedHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
    "BrowserInitiated.PromptResultV2";

struct TestParams {
  AutoPipReason auto_pip_reason;
};

}  // anonymous namespace

class MockAutoBlocker : public permissions::PermissionDecisionAutoBlockerBase {
 public:
  MOCK_METHOD(bool,
              IsEmbargoed,
              (const GURL& request_origin, ContentSettingsType permission),
              (override));
  MOCK_METHOD(bool,
              RecordDismissAndEmbargo,
              (const GURL& url,
               ContentSettingsType permission,
               bool dismissed_prompt_was_quiet),
              (override));
  MOCK_METHOD(bool,
              RecordIgnoreAndEmbargo,
              (const GURL& url,
               ContentSettingsType permission,
               bool ignored_prompt_was_quiet),
              (override));
};

class AutoPipSettingHelperTest
    : public views::ViewsTestBase,
      public testing::WithParamInterface<TestParams> {
 public:
  AutoPipSettingHelperTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->Show();

    // Create parent Widget for AutoPiP setting view.
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    parent_widget_->Show();

    // Create the anchor Widget for AutoPiP setting view.
    anchor_view_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    anchor_view_widget_->Show();

    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, true /* should_record_metrics */);

    setting_helper_ = std::make_unique<AutoPipSettingHelper>(
        origin_, settings_map_.get(), &auto_blocker());
  }

  void TearDown() override {
    anchor_view_widget_.reset();
    parent_widget_.reset();
    setting_overlay_ = nullptr;
    widget_.reset();
    setting_helper_.reset();
    ViewsTestBase::TearDown();
    settings_map_->ShutdownOnUIThread();
  }

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }

  AutoPipSettingHelper* setting_helper() { return setting_helper_.get(); }
  AutoPipSettingOverlayView* setting_overlay() const {
    return setting_overlay_;
  }

  AutoPipSettingView* setting_view() const {
    return setting_overlay()->get_view_for_testing();
  }

  void clear_setting_helper() { setting_helper_.reset(); }

  views::Widget* widget() const { return widget_.get(); }

  base::MockOnceCallback<void()>& close_cb() { return close_cb_; }

  // Ask the helper for the overlay view, and return whatever it gives us.  This
  // may be null if it decides that one shouldn't be shown.
  AutoPipSettingOverlayView* AttachOverlayView(
      AutoPipReason auto_pip_reason = AutoPipReason::kUnknown) {
    auto* anchor_view =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());
    auto setting_overlay = setting_helper_->CreateOverlayViewIfNeeded(
        close_cb_.Get(), auto_pip_reason, std::nullopt, anchor_view,
        views::BubbleBorder::TOP_CENTER);
    if (setting_overlay) {
      setting_overlay_ = static_cast<AutoPipSettingOverlayView*>(
          widget_->SetContentsView(std::move(setting_overlay)));
    } else {
      setting_overlay_ = nullptr;
    }

    return setting_overlay_;
  }

  void set_content_setting(ContentSetting new_setting) {
    settings_map_->SetContentSettingDefaultScope(
        origin_, GURL(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
        new_setting);
  }

  ContentSetting get_content_setting() {
    return settings_map_->GetContentSetting(
        origin_, GURL(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
  }

  MockAutoBlocker& auto_blocker() { return auto_blocker_; }

  // Set up expectations that there will be no interaction with the permissions
  // auto blocker.
  void ExpectEmbargoWillNotBeChecked() {
    EXPECT_CALL(auto_blocker(), IsEmbargoed(_, _)).Times(0);
  }

  // Expect that there will be an embargo check, and that there isn't currently
  // an embargo.
  void SetupNoEmbargo() {
    // Expect `auto_blocker()` will be called at least once for the correct
    // origin and content setting, but never for anything else.
    EXPECT_CALL(auto_blocker(), IsEmbargoed(_, _)).Times(0);
    EXPECT_CALL(
        auto_blocker(),
        IsEmbargoed(origin_, ContentSettingsType::AUTO_PICTURE_IN_PICTURE))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));
  }

  const GURL& origin() const { return origin_; }

 private:
  base::MockOnceCallback<void()> close_cb_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AutoPipSettingOverlayView> setting_overlay_ = nullptr;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<views::Widget> anchor_view_widget_;

  const GURL origin_{"https://example.com"};

  // Used by the HostContentSettingsMap instance.
  sync_preferences::TestingPrefServiceSyncable prefs_;

  // Used by the AutoPipSettingHelper instance.
  scoped_refptr<HostContentSettingsMap> settings_map_;
  MockAutoBlocker auto_blocker_;

  std::unique_ptr<AutoPipSettingHelper> setting_helper_;
};

TEST_F(AutoPipSettingHelperTest, NoUiIfContentSettingIsAllow) {
  set_content_setting(CONTENT_SETTING_ALLOW);
  ExpectEmbargoWillNotBeChecked();

  EXPECT_CALL(close_cb(), Run()).Times(0);
  AttachOverlayView();
  EXPECT_FALSE(setting_overlay());
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ALLOW);
}

TEST_F(AutoPipSettingHelperTest, UiShownIfContentSettingIsAsk) {
  set_content_setting(CONTENT_SETTING_ASK);
  SetupNoEmbargo();

  EXPECT_CALL(close_cb(), Run()).Times(0);
  AttachOverlayView();
  EXPECT_TRUE(setting_overlay());
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);
}

TEST_F(AutoPipSettingHelperTest,
       NoUiButCallbackIsCalledIfContentSettingIsBlock) {
  set_content_setting(CONTENT_SETTING_BLOCK);
  ExpectEmbargoWillNotBeChecked();

  EXPECT_CALL(close_cb(), Run()).Times(1);
  AttachOverlayView();
  EXPECT_FALSE(setting_overlay());
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_BLOCK);
}

TEST_F(AutoPipSettingHelperTest, AllowOnceDoesNotCallCloseCb) {
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  ASSERT_TRUE(AttachOverlayView());
  setting_overlay()->ShowBubble(widget()->GetNativeView());

  // Run result callback with "allow once" UiResult.  Nothing should happen.
  EXPECT_CALL(close_cb(), Run()).Times(0);
  setting_view()->simulate_button_press_for_testing(UiResult::kAllowOnce);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);

  // Also verify that asking again does not create the view again, since 'allow
  // once' persists for the lifetime of the setting helper.
  ASSERT_FALSE(AttachOverlayView());
}

TEST_F(AutoPipSettingHelperTest, AllowOnEveryVisitDoesNotCallCloseCb) {
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  ASSERT_TRUE(AttachOverlayView());
  setting_overlay()->ShowBubble(widget()->GetNativeView());

  // Run result callback with "allow on every visit" UiResult.  Nothing should
  // happen.
  EXPECT_CALL(close_cb(), Run()).Times(0);
  setting_view()->simulate_button_press_for_testing(
      UiResult::kAllowOnEveryVisit);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ALLOW);
}

// Verify AUTO_PICTURE_IN_PICTURE permission granted through AutoPipSettingView
// bubble is correctly marked as eligible (i.e. `last_visited` timestamp
// is tracked) for Safety Hub auto-revocation when the
// kSafetyHubUnusedPermissionRevocationForAllSurfaces flag is enabled.
TEST_F(AutoPipSettingHelperTest, UpdateContentSetting_LastVisited_Tracked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);

  // Show AutoPipSettingView bubble for the user to choose a setting for
  // Automatic Picture in Picture.
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  ASSERT_TRUE(AttachOverlayView());
  setting_overlay()->ShowBubble(widget()->GetNativeView());

  // Simulate the user choosing "ALLOW on every visit".
  setting_view()->simulate_button_press_for_testing(
      UiResult::kAllowOnEveryVisit);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ALLOW);

  // Verify that `last_visited` was recorded and lies within the past 7 days.
  //
  // The `last_visited` is coarsed by `GetCoarseVisitedTime` [1] due to privacy.
  // It rounds given timestamp down to the nearest multiple of 7 in the past.
  // [1] components/content_settings/core/browser/content_settings_utils.cc
  base::Time now = base::Time::Now();
  content_settings::SettingInfo info;
  settings_map()->GetWebsiteSetting(
      origin(), GURL(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE, &info);
  EXPECT_GE(info.metadata.last_visited(), now - base::Days(7));
  EXPECT_LE(info.metadata.last_visited(), now);
}

// Verify AUTO_PICTURE_IN_PICTURE permission blocked through AutoPipSettingView
// bubble is not marked as eligible (i.e. `last_visited` timestamp
// is tracked) for Safety Hub auto-revocation even when the
// kSafetyHubUnusedPermissionRevocationForAllSurfaces flag is enabled.
TEST_F(AutoPipSettingHelperTest,
       UpdateContentSetting_LastVisited_NotTracked_WrongValue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);

  // Show AutoPipSettingView bubble for the user to choose a setting for
  // Automatic Picture in Picture.
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  ASSERT_TRUE(AttachOverlayView());
  setting_overlay()->ShowBubble(widget()->GetNativeView());

  // Simulate the user choosing "BLOCK".
  setting_view()->simulate_button_press_for_testing(UiResult::kBlock);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_BLOCK);

  // Verify that `last_visited` is not recorded unless the value is ALLOW.
  content_settings::SettingInfo info;
  settings_map()->GetWebsiteSetting(
      origin(), GURL(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE, &info);
  EXPECT_EQ(base::Time(), info.metadata.last_visited());
}

// Verify AUTO_PICTURE_IN_PICTURE permission granted through AutoPipSettingView
// bubble is not marked as eligible (i.e. `last_visited` timestamp
// is tracked) for Safety Hub auto-revocation because the
// kSafetyHubUnusedPermissionRevocationForAllSurfaces flag is disabled.
TEST_F(AutoPipSettingHelperTest,
       UpdateContentSetting_LastVisited_NotTracked_FeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);

  // Show AutoPipSettingView bubble for the user to choose a setting for
  // Automatic Picture in Picture.
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  ASSERT_TRUE(AttachOverlayView());
  setting_overlay()->ShowBubble(widget()->GetNativeView());

  // Simulate the user choosing "ALLOW on every visit".
  setting_view()->simulate_button_press_for_testing(
      UiResult::kAllowOnEveryVisit);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ALLOW);

  // Verify that `last_visited` is not recorded when the feature is off.
  content_settings::SettingInfo info;
  settings_map()->GetWebsiteSetting(
      origin(), GURL(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE, &info);
  EXPECT_EQ(base::Time(), info.metadata.last_visited());
}

TEST_F(AutoPipSettingHelperTest, BlockDoesCallCloseCb) {
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  ASSERT_TRUE(AttachOverlayView());
  setting_overlay()->ShowBubble(widget()->GetNativeView());

  // Run result callback with "block" UiResult.  The close cb should be called.
  EXPECT_CALL(close_cb(), Run()).Times(1);
  setting_view()->simulate_button_press_for_testing(UiResult::kBlock);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_BLOCK);
}

TEST_F(AutoPipSettingHelperTest,
       DestructionDoesNotNotifyEmbargoEvenIfUiIsCreated) {
  // If the UI is created and not used, and the window is destroyed without
  // notifying the helper that the user explicitly closed it, then the embargo
  // should not be updated.  For example, switching back to the opener tab or
  // the site closing the pip window should not count against the embargo.
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  EXPECT_CALL(auto_blocker(), RecordDismissAndEmbargo(_, _, _)).Times(0);

  // The close cb shouldn't be called because the setting helper shouldn't want
  // to close the window.
  EXPECT_CALL(close_cb(), Run()).Times(0);
  AttachOverlayView();
  EXPECT_TRUE(setting_overlay());
  // Destroy the setting helper without calling `OnUserClosedWindow()`.
  clear_setting_helper();
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);
}

TEST_F(AutoPipSettingHelperTest, DismissNotifiesEmbargoIfUiIsCreated) {
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  EXPECT_CALL(
      auto_blocker(),
      RecordDismissAndEmbargo(
          origin(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE, false))
      .Times(1)
      .WillOnce(Return(true));

  // Notify the setting helper that the user closed the window manually.  The
  // close cb should not be called, because the user closed it already.
  EXPECT_CALL(close_cb(), Run()).Times(0);
  AttachOverlayView();
  EXPECT_TRUE(setting_overlay());
  setting_helper()->OnUserClosedWindow(AutoPipReason::kUnknown, std::nullopt);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);
}

TEST_F(AutoPipSettingHelperTest,
       DismissDoesNotNotifyEmbargoIfUiIsNotRequested) {
  set_content_setting(CONTENT_SETTING_DEFAULT);
  ExpectEmbargoWillNotBeChecked();
  // We don't ask for the UI, so there should not be an embargo check.  There
  // should also not be an embargo update.  That's the point of this test --
  // creating the setting helper but not using it should not cause any update to
  // the embargo.
  EXPECT_CALL(auto_blocker(), RecordDismissAndEmbargo(_, _, _)).Times(0);

  EXPECT_CALL(close_cb(), Run()).Times(0);
  // Do not attach the overlay view, which should prevent a callback since the
  // user wasn't presented with any UI.
  setting_helper()->OnUserClosedWindow(AutoPipReason::kUnknown, std::nullopt);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);
}

TEST_F(AutoPipSettingHelperTest,
       UiIsNotShownIfContentSettingIsAskButUnderEmbargo) {
  set_content_setting(CONTENT_SETTING_ASK);
  EXPECT_CALL(
      auto_blocker(),
      IsEmbargoed(origin(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  // Since there's no UI shown, there also should not be an embargo update.
  EXPECT_CALL(auto_blocker(), RecordDismissAndEmbargo(_, _, _)).Times(0);

  // Since there's an embargo, we expect that there should not be an overlay
  // view shown.  However, the close cb should be called.
  EXPECT_CALL(close_cb(), Run()).Times(1);
  AttachOverlayView();
  EXPECT_FALSE(setting_overlay());
  // Should not change the content setting as a result of the embargo.
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);
}

const struct TestParams kTestHistogramNameParams[] = {
    {AutoPipReason::kUnknown},
    {AutoPipReason::kVideoConferencing},
    {AutoPipReason::kMediaPlayback},
    {AutoPipReason::kBrowserInitiated}};

INSTANTIATE_TEST_SUITE_P(AllHistogramNames,
                         AutoPipSettingHelperTest,
                         testing::ValuesIn(kTestHistogramNameParams));

TEST_P(AutoPipSettingHelperTest, HistogramExpectedCounts) {
  set_content_setting(CONTENT_SETTING_DEFAULT);
  SetupNoEmbargo();
  ASSERT_TRUE(AttachOverlayView(GetParam().auto_pip_reason));
  setting_overlay()->ShowBubble(widget()->GetNativeView());

  // Run result callback with "block" UiResult.  The close cb should be called,
  // and the corresponding histogram count recorded.
  base::HistogramTester histograms;
  EXPECT_CALL(close_cb(), Run()).Times(1);
  setting_view()->simulate_button_press_for_testing(UiResult::kBlock);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_BLOCK);

  auto video_conferencing_samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);
  auto media_playback_samples =
      histograms.GetHistogramSamplesSinceCreation(kMediaPlaybackHistogram);
  auto browser_initiated_samples =
      histograms.GetHistogramSamplesSinceCreation(kBrowserInitiatedHistogram);

  const auto auto_pip_reason = GetParam().auto_pip_reason;
  if (auto_pip_reason == AutoPipReason::kUnknown) {
    EXPECT_EQ(0, video_conferencing_samples->TotalCount());
    EXPECT_EQ(0, media_playback_samples->TotalCount());
    EXPECT_EQ(0, browser_initiated_samples->TotalCount());
  } else if (auto_pip_reason == AutoPipReason::kVideoConferencing) {
    EXPECT_EQ(1, video_conferencing_samples->TotalCount());
    EXPECT_EQ(0, media_playback_samples->TotalCount());
    EXPECT_EQ(0, browser_initiated_samples->TotalCount());
  } else if (auto_pip_reason == AutoPipReason::kMediaPlayback) {
    EXPECT_EQ(0, video_conferencing_samples->TotalCount());
    EXPECT_EQ(1, media_playback_samples->TotalCount());
    EXPECT_EQ(0, browser_initiated_samples->TotalCount());
  } else if (auto_pip_reason == AutoPipReason::kBrowserInitiated) {
    EXPECT_EQ(0, video_conferencing_samples->TotalCount());
    EXPECT_EQ(0, media_playback_samples->TotalCount());
    EXPECT_EQ(1, browser_initiated_samples->TotalCount());
  } else {
    FAIL() << "Unhandled auto picture in picture reason: "
           << static_cast<int>(auto_pip_reason);
  }
}
