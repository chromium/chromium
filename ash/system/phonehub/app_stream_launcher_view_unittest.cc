// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_launcher_view.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/app_stream_launcher_item.h"
#include "ash/system/phonehub/app_stream_launcher_list_item.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

class AppStreamLauncherViewTest : public views::ViewsTestBase {
 public:
  AppStreamLauncherViewTest() = default;
  ~AppStreamLauncherViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    CreateWidget();
    generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));

    // All unit tests related to ListView will enable the feature themselves.
    // Currently keeping the grid view unit tests in the event the design goes
    // back to it.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA},
        /*disabled_features=*/{features::kEcheLauncherListView});
  }

  // AshTestBase:
  void TearDown() override {
    app_stream_launcher_view_.reset();
    generator_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  views::Widget* widget() { return widget_; }
  ui::test::EventGenerator* generator() { return generator_.get(); }
  AppStreamLauncherView* app_stream_launcher_view() {
    return app_stream_launcher_view_.get();
  }

  void GenerateLauncherView() {
    app_stream_launcher_view_ =
        std::make_unique<AppStreamLauncherView>(&fake_phone_hub_manager_);
    widget_->SetContentsView(app_stream_launcher_view_.get());
    widget_->Show();
    widget_->LayoutRootViewIfNecessary();
  }

  AppStreamLauncherItem* GetItemView(int index) {
    return static_cast<AppStreamLauncherItem*>(
        app_stream_launcher_view()->items_container_for_test()->children().at(
            index));
  }

  AppStreamLauncherListItem* GetListItemView(int index) {
    return static_cast<AppStreamLauncherListItem*>(
        app_stream_launcher_view()->items_container_for_test()->children().at(
            index));
  }

  const gfx::Image CreateTestImage() {
    gfx::ImageSkia image_skia = gfx::test::CreateImageSkia(/*size=*/60);
    image_skia.MakeThreadSafe();
    return gfx::Image(image_skia);
  }

  void CreateWidget() {
    DCHECK(!widget_);
    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 600, 800);
    widget_->Init(std::move(params));
  }

  phonehub::FakePhoneHubManager* fake_phone_hub_manager() {
    return &fake_phone_hub_manager_;
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  // This is required in order for the context to find color provider
  AshColorProvider color_provider_;
  std::unique_ptr<AppStreamLauncherView> app_stream_launcher_view_;
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  raw_ptr<views::Widget, DanglingUntriaged> widget_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

TEST_F(AppStreamLauncherViewTest, OpenView) {
  fake_phone_hub_manager()
      ->fake_app_stream_launcher_data_model()
      ->SetLauncherSize(1000, 1000);
  GenerateLauncherView();
  EXPECT_TRUE(app_stream_launcher_view()->GetVisible());
}

TEST_F(AppStreamLauncherViewTest, AddItems) {
  GenerateLauncherView();
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";

  EXPECT_EQ(0U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  EXPECT_EQ(u"Fake App", GetItemView(0)->GetLabelForTest()->GetText());
}

TEST_F(AppStreamLauncherViewTest, AddItemsListView) {
  GenerateLauncherView();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA,
                            features::kEcheLauncherListView},
      /*disabled_features=*/{});

  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  EXPECT_EQ(u"Fake App", GetListItemView(0)->GetText());
}

TEST_F(AppStreamLauncherViewTest, RemoveItem) {
  GenerateLauncherView();
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";

  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  EXPECT_EQ(u"Fake App", GetItemView(0)->GetLabelForTest()->GetText());

  apps.clear();
  data_model->SetAppList(apps);

  EXPECT_EQ(0U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());
}

TEST_F(AppStreamLauncherViewTest, RemoveItemListView) {
  GenerateLauncherView();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA,
                            features::kEcheLauncherListView},
      /*disabled_features=*/{});

  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";

  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  EXPECT_EQ(u"Fake App", GetListItemView(0)->GetText());

  apps.clear();
  data_model->SetAppList(apps);

  EXPECT_EQ(0U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());
}

TEST_F(AppStreamLauncherViewTest, ClickOnItem) {
  GenerateLauncherView();
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";

  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);
  widget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  ui::test::EventGenerator generator(
      GetRootWindow(app_stream_launcher_view()->GetWidget()));

  EXPECT_TRUE(GetItemView(0)->GetVisible());
  EXPECT_TRUE(GetItemView(0)->GetIconForTest()->GetEnabled());
  EXPECT_TRUE(GetItemView(0)->GetLabelForTest()->GetEnabled());

  gfx::Point cursor_location =
      GetItemView(0)->GetIconForTest()->GetBoundsInScreen().CenterPoint();
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  EXPECT_EQ(1U, fake_phone_hub_manager()
                    ->fake_recent_apps_interaction_handler()
                    ->HandledRecentAppsCount(package_name));
}

TEST_F(AppStreamLauncherViewTest, ClickOnItemListView) {
  GenerateLauncherView();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA,
                            features::kEcheLauncherListView},
      /*disabled_features=*/{});

  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";

  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);
  widget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  ui::test::EventGenerator generator(
      GetRootWindow(app_stream_launcher_view()->GetWidget()));

  EXPECT_TRUE(GetListItemView(0)->GetVisible());
  EXPECT_TRUE(GetListItemView(0)->GetEnabled());

  gfx::Point cursor_location =
      GetListItemView(0)->GetBoundsInScreen().CenterPoint();
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  EXPECT_EQ(1U, fake_phone_hub_manager()
                    ->fake_recent_apps_interaction_handler()
                    ->HandledRecentAppsCount(package_name));
}

TEST_F(AppStreamLauncherViewTest, DisabledItem) {
  GenerateLauncherView();
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";

  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::BLOCK_LISTED);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);
  widget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  ui::test::EventGenerator generator(
      GetRootWindow(app_stream_launcher_view()->GetWidget()));

  EXPECT_TRUE(GetItemView(0)->GetVisible());
  EXPECT_FALSE(GetItemView(0)->GetIconForTest()->GetEnabled());
  EXPECT_FALSE(GetItemView(0)->GetLabelForTest()->GetEnabled());
  EXPECT_EQ(u"Not supported",
            GetItemView(0)->GetIconForTest()->GetTooltipText());
}

TEST_F(AppStreamLauncherViewTest, DisabledItemListView) {
  GenerateLauncherView();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA,
                            features::kEcheLauncherListView},
      /*disabled_features=*/{});

  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";

  auto app1 = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/CreateTestImage(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, user_id,
      phonehub::proto::AppStreamabilityStatus::BLOCK_LISTED);
  std::vector<phonehub::Notification::AppMetadata> apps;
  apps.push_back(app1);

  phonehub::AppStreamLauncherDataModel* data_model =
      fake_phone_hub_manager()->fake_app_stream_launcher_data_model();
  data_model->SetAppList(apps);
  widget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(1U, app_stream_launcher_view()
                    ->items_container_for_test()
                    ->children()
                    .size());

  ui::test::EventGenerator generator(
      GetRootWindow(app_stream_launcher_view()->GetWidget()));

  EXPECT_TRUE(GetListItemView(0)->GetVisible());
  EXPECT_FALSE(GetListItemView(0)->GetEnabled());
  EXPECT_EQ(u"Not supported", GetListItemView(0)->GetTooltipText());
}

}  // namespace ash
