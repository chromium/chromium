// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

class AutoPipSettingHelperTest : public views::ViewsTestBase {
 public:
  AutoPipSettingHelperTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    widget_->Show();

    // Create parent Widget for AutoPiP setting view.
    parent_widget_ = CreateTestWidget();
    parent_widget_->Show();

    // Create the anchor Widget for AutoPiP setting view.
    anchor_view_widget_ = CreateTestWidget();
    anchor_view_widget_->Show();

    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, true /* should_record_metrics */);

    setting_helper_ = std::make_unique<AutoPipSettingHelper>(
        origin_, settings_map_.get(), close_cb_.Get());
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

  AutoPipSettingHelper* setting_helper() { return setting_helper_.get(); }
  const AutoPipSettingOverlayView* setting_overlay() const {
    return setting_overlay_;
  }

  base::MockOnceCallback<void()>& close_cb() { return close_cb_; }

  void AttachOverlayView() {
    auto* anchor_view =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());
    auto setting_overlay = setting_helper_->CreateOverlayViewIfNeeded(
        gfx::Rect(), anchor_view, views::BubbleBorder::TOP_CENTER);
    if (setting_overlay) {
      setting_overlay_ = static_cast<AutoPipSettingOverlayView*>(
          widget_->SetContentsView(std::move(setting_overlay)));
    }
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

  std::unique_ptr<AutoPipSettingHelper> setting_helper_;
};

TEST_F(AutoPipSettingHelperTest, NoUiIfContentSettingIsAllow) {
  set_content_setting(CONTENT_SETTING_ALLOW);

  EXPECT_CALL(close_cb(), Run()).Times(0);
  AttachOverlayView();
  EXPECT_FALSE(setting_overlay());
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ALLOW);
}

TEST_F(AutoPipSettingHelperTest, UiShownIfContentSettingIsAsk) {
  set_content_setting(CONTENT_SETTING_ASK);

  EXPECT_CALL(close_cb(), Run()).Times(0);
  AttachOverlayView();
  EXPECT_TRUE(setting_overlay());
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);
}

TEST_F(AutoPipSettingHelperTest,
       NoUiButCallbackIsCalledIfContentSettingIsBlock) {
  set_content_setting(CONTENT_SETTING_BLOCK);

  EXPECT_CALL(close_cb(), Run()).Times(1);
  AttachOverlayView();
  EXPECT_FALSE(setting_overlay());
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_BLOCK);
}

TEST_F(AutoPipSettingHelperTest, AllowOnceDoesNotCallCloseCb) {
  set_content_setting(CONTENT_SETTING_DEFAULT);

  // Run result callback with "allow once" UiResult.  Nothing should happen.
  EXPECT_CALL(close_cb(), Run()).Times(0);
  std::move(setting_helper()->take_result_cb_for_testing())
      .Run(AutoPipSettingView::UiResult::kAllowOnce);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ASK);
}

TEST_F(AutoPipSettingHelperTest, AllowOnEveryVisitDoesNotCallCloseCb) {
  set_content_setting(CONTENT_SETTING_DEFAULT);

  // Run result callback with "allow on every visit" UiResult.  Nothing should
  // happen.
  EXPECT_CALL(close_cb(), Run()).Times(0);
  std::move(setting_helper()->take_result_cb_for_testing())
      .Run(AutoPipSettingView::UiResult::kAllowOnEveryVisit);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_ALLOW);
}

TEST_F(AutoPipSettingHelperTest, BlockDoesCallCloseCb) {
  set_content_setting(CONTENT_SETTING_DEFAULT);

  // Run result callback with "block" UiResult.  The close cb should be called.
  EXPECT_CALL(close_cb(), Run()).Times(1);
  std::move(setting_helper()->take_result_cb_for_testing())
      .Run(AutoPipSettingView::UiResult::kBlock);
  EXPECT_EQ(get_content_setting(), CONTENT_SETTING_BLOCK);
}
