// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/folder_image.h"

#include <string>
#include <utility>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace ash {

namespace {

gfx::ImageSkia CreateSquareBitmapWithColor(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

bool ImagesAreEqual(const gfx::ImageSkia& image1,
                    const gfx::ImageSkia& image2) {
  return gfx::BitmapsAreEqual(*image1.bitmap(), *image2.bitmap());
}

// Listens for OnFolderImageUpdated and sets a flag upon receiving the signal.
class TestFolderImageObserver : public FolderImageObserver {
 public:
  TestFolderImageObserver() : updated_flag_(false) {}

  TestFolderImageObserver(const TestFolderImageObserver&) = delete;
  TestFolderImageObserver& operator=(const TestFolderImageObserver&) = delete;

  bool updated() const { return updated_flag_; }

  void Reset() { updated_flag_ = false; }

  // FolderImageObserver overrides:
  void OnFolderImageUpdated(ash::AppListConfigType config_type) override {
    updated_flag_ = true;
  }

 private:
  bool updated_flag_;
};

}  // namespace

class FolderImageTest : public testing::Test,
                        public ::testing::WithParamInterface<
                            std::tuple<AppListConfigType, bool>> {
 public:
  FolderImageTest() : scoped_locale_(std::get<1>(GetParam()) ? "he" : "") {}

  FolderImageTest(const FolderImageTest&) = delete;
  FolderImageTest& operator=(const FolderImageTest&) = delete;

  ~FolderImageTest() override = default;

  void SetUp() override {
    app_list_model_ = std::make_unique<test::AppListTestModel>();
    folder_image_ =
        std::make_unique<FolderImage>(GetAppListConfig(/*can_create=*/true),
                                      app_list_model_->top_level_item_list());

    // Populate the AppListModel with three items (to test that the FolderImage
    // correctly supports having fewer than four icons).
    AddAppWithColoredIcon("app1", SK_ColorRED);
    AddAppWithColoredIcon("app2", SK_ColorGREEN);
    AddAppWithColoredIcon("app3", SK_ColorBLUE);

    observer_.Reset();
    folder_image_->AddObserver(&observer_);
  }

  void TearDown() override {
    folder_image_->RemoveObserver(&observer_);
    AppListConfigProvider::Get().ResetForTesting();
  }

  AppListConfig* GetAppListConfig(bool can_create) {
    return AppListConfigProvider::Get().GetConfigForType(
        std::get<0>(GetParam()), can_create);
  }

  bool is_rtl() { return std::get<1>(GetParam()); }

 protected:
  void AddAppWithColoredIcon(const std::string& id, SkColor icon_color) {
    std::unique_ptr<AppListItem> item(new AppListItem(id));
    item->SetDefaultIconAndColor(
        CreateSquareBitmapWithColor(
            SharedAppListConfig::instance().default_grid_icon_dimension(),
            icon_color),
        IconColor(), /*is_placeholder_icon=*/false);
    static_cast<AppListModel*>(app_list_model_.get())->AddItem(std::move(item));
  }

  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;

  std::unique_ptr<test::AppListTestModel> app_list_model_;

  std::unique_ptr<FolderImage> folder_image_;

  TestFolderImageObserver observer_;
};
INSTANTIATE_TEST_SUITE_P(
    All,
    FolderImageTest,
    ::testing::Combine(::testing::Values(AppListConfigType::kRegular,
                                         AppListConfigType::kDense),
                       ::testing::Bool()));

TEST_P(FolderImageTest, UpdateListTest) {
  gfx::ImageSkia icon1 = folder_image_->icon();

  // Call UpdateIcon and ensure that the observer event fired.
  folder_image_->UpdateIcon();
  EXPECT_TRUE(observer_.updated());
  observer_.Reset();
  // The icon should not have changed.
  EXPECT_TRUE(ImagesAreEqual(icon1, folder_image_->icon()));

  // Swap two items. Ensure that the observer fired and the icon changed.
  app_list_model_->top_level_item_list()->MoveItem(2, 1);
  EXPECT_TRUE(observer_.updated());
  observer_.Reset();
  gfx::ImageSkia icon2 = folder_image_->icon();
  EXPECT_FALSE(ImagesAreEqual(icon1, icon2));

  // Swap back items. Ensure that the observer fired and the icon changed back.
  app_list_model_->top_level_item_list()->MoveItem(2, 1);
  EXPECT_TRUE(observer_.updated());
  observer_.Reset();
  EXPECT_TRUE(ImagesAreEqual(icon1, folder_image_->icon()));

  // Add a new item. Ensure that the observer fired and the icon changed.
  AddAppWithColoredIcon("app4", SK_ColorYELLOW);
  EXPECT_TRUE(observer_.updated());
  observer_.Reset();
  gfx::ImageSkia icon3 = folder_image_->icon();
  EXPECT_FALSE(ImagesAreEqual(icon1, icon3));

  // Add a new item. The observer should not fire, nor should the icon change
  // (because it does not affect the first four icons).
  AddAppWithColoredIcon("app5", SK_ColorCYAN);
  EXPECT_FALSE(observer_.updated());
  observer_.Reset();
  EXPECT_TRUE(ImagesAreEqual(icon3, folder_image_->icon()));

  // Delete an item. Ensure that the observer fired and the icon changed.
  app_list_model_->DeleteItem("app2");
  EXPECT_TRUE(observer_.updated());
  observer_.Reset();
  gfx::ImageSkia icon4 = folder_image_->icon();
  EXPECT_FALSE(ImagesAreEqual(icon3, icon4));
}

TEST_P(FolderImageTest, UpdateItemTest) {
  gfx::ImageSkia icon1 = folder_image_->icon();

  // Change an item's icon. Ensure that the observer fired and the icon changed.
  app_list_model_->FindItem("app2")->SetDefaultIconAndColor(
      CreateSquareBitmapWithColor(
          SharedAppListConfig::instance().default_grid_icon_dimension(),
          SK_ColorMAGENTA),
      IconColor(), /*is_placeholder_icon=*/false);
  EXPECT_TRUE(observer_.updated());
  observer_.Reset();
  EXPECT_FALSE(ImagesAreEqual(icon1, folder_image_->icon()));
}

TEST_P(FolderImageTest, GetTargetIconRectInFolderWithSingleItem) {
  app_list_model_->DeleteItem("app2");
  app_list_model_->DeleteItem("app3");
  const AppListConfig* config = GetAppListConfig(/*can_create=*/false);
  ASSERT_TRUE(config);

  const gfx::Rect test_rects[] = {
      gfx::Rect(config->icon_visible_size()),
      gfx::Rect(gfx::Point(10, 10), config->icon_visible_size()),
      gfx::Rect(config->folder_icon_size()),
      gfx::Rect(gfx::Point(10, 10), config->folder_icon_size()),
  };

  for (const auto& test_rect : test_rects) {
    SCOPED_TRACE(::testing::Message()
                 << "Target folder icon bounds: " << test_rect.ToString());

    const gfx::Point test_rect_center = test_rect.CenterPoint();

    const gfx::Size expected_icon_rect_size(
        config->item_icon_in_folder_icon_dimension(),
        config->item_icon_in_folder_icon_dimension());

    gfx::Rect item_1_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app1"), test_rect);
    EXPECT_EQ(expected_icon_rect_size, item_1_bounds.size());
    EXPECT_EQ(test_rect_center, item_1_bounds.CenterPoint());
  }
}

TEST_P(FolderImageTest, GetTargetIconRectInFolderWithTwoItems) {
  app_list_model_->DeleteItem("app3");
  const AppListConfig* config = GetAppListConfig(/*can_create=*/false);
  ASSERT_TRUE(config);

  const gfx::Rect test_rects[] = {
      gfx::Rect(config->icon_visible_size()),
      gfx::Rect(gfx::Point(10, 10), config->icon_visible_size()),
      gfx::Rect(config->folder_icon_size()),
      gfx::Rect(gfx::Point(10, 10), config->folder_icon_size()),
  };

  for (const auto& test_rect : test_rects) {
    SCOPED_TRACE(::testing::Message()
                 << "Target folder icon bounds: " << test_rect.ToString());

    const gfx::Point test_rect_center = test_rect.CenterPoint();

    const gfx::Size expected_icon_rect_size(
        config->item_icon_in_folder_icon_dimension(),
        config->item_icon_in_folder_icon_dimension());

    gfx::Rect item_1_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app1"), test_rect);
    gfx::Rect item_2_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app2"), test_rect);

    if (is_rtl())
      std::swap(item_1_bounds, item_2_bounds);

    EXPECT_EQ(expected_icon_rect_size, item_1_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() - config->item_icon_in_folder_icon_margin() / 2,
        item_1_bounds.right());
    EXPECT_EQ(test_rect_center.y(), item_1_bounds.CenterPoint().y());

    EXPECT_EQ(expected_icon_rect_size, item_2_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() + config->item_icon_in_folder_icon_margin() / 2,
        item_2_bounds.x());
    EXPECT_EQ(test_rect_center.y(), item_2_bounds.CenterPoint().y());
  }
}

TEST_P(FolderImageTest, GetTargetIconRectInFolderWithThreeItems) {
  const AppListConfig* config = GetAppListConfig(/*can_create=*/false);
  ASSERT_TRUE(config);

  const gfx::Rect test_rects[] = {
      gfx::Rect(config->icon_visible_size()),
      gfx::Rect(gfx::Point(10, 10), config->icon_visible_size()),
      gfx::Rect(config->folder_icon_size()),
      gfx::Rect(gfx::Point(10, 10), config->folder_icon_size()),
  };

  for (const auto& test_rect : test_rects) {
    SCOPED_TRACE(::testing::Message()
                 << "Target folder icon bounds: " << test_rect.ToString());

    const gfx::Point test_rect_center = test_rect.CenterPoint();

    const gfx::Size expected_icon_rect_size(
        config->item_icon_in_folder_icon_dimension(),
        config->item_icon_in_folder_icon_dimension());

    gfx::Rect item_1_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app1"), test_rect);
    gfx::Rect item_2_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app2"), test_rect);
    gfx::Rect item_3_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app3"), test_rect);

    if (is_rtl())
      std::swap(item_2_bounds, item_3_bounds);

    EXPECT_EQ(expected_icon_rect_size, item_1_bounds.size());
    EXPECT_EQ(test_rect_center.x(), item_1_bounds.CenterPoint().x());
    EXPECT_EQ(
        test_rect_center.y() - config->item_icon_in_folder_icon_margin() / 2,
        item_1_bounds.bottom());
    EXPECT_EQ(expected_icon_rect_size, item_2_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() - config->item_icon_in_folder_icon_margin() / 2,
        item_2_bounds.right());
    EXPECT_EQ(
        test_rect_center.y() + config->item_icon_in_folder_icon_margin() / 2,
        item_2_bounds.y());

    EXPECT_EQ(expected_icon_rect_size, item_3_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() + config->item_icon_in_folder_icon_margin() / 2,
        item_3_bounds.x());
    EXPECT_EQ(
        test_rect_center.y() + config->item_icon_in_folder_icon_margin() / 2,
        item_3_bounds.y());
  }
}

TEST_P(FolderImageTest, GetTargetIconRectInFolderWithFourItems) {
  AddAppWithColoredIcon("app4", SK_ColorYELLOW);

  const AppListConfig* config = GetAppListConfig(/*can_create=*/false);
  ASSERT_TRUE(config);

  const gfx::Rect test_rects[] = {
      gfx::Rect(config->icon_visible_size()),
      gfx::Rect(gfx::Point(10, 10), config->icon_visible_size()),
      gfx::Rect(config->folder_icon_size()),
      gfx::Rect(gfx::Point(10, 10), config->folder_icon_size()),
  };

  for (const auto& test_rect : test_rects) {
    SCOPED_TRACE(::testing::Message()
                 << "Target folder icon bounds: " << test_rect.ToString());

    const gfx::Point test_rect_center = test_rect.CenterPoint();

    const gfx::Size expected_icon_rect_size(
        config->item_icon_in_folder_icon_dimension(),
        config->item_icon_in_folder_icon_dimension());

    gfx::Rect item_1_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app1"), test_rect);
    gfx::Rect item_2_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app2"), test_rect);
    gfx::Rect item_3_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app3"), test_rect);
    gfx::Rect item_4_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app4"), test_rect);

    if (is_rtl()) {
      std::swap(item_1_bounds, item_2_bounds);
      std::swap(item_3_bounds, item_4_bounds);
    }

    EXPECT_EQ(expected_icon_rect_size, item_1_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() - config->item_icon_in_folder_icon_margin() / 2,
        item_1_bounds.right());
    EXPECT_EQ(
        test_rect_center.y() - config->item_icon_in_folder_icon_margin() / 2,
        item_1_bounds.bottom());

    EXPECT_EQ(expected_icon_rect_size, item_2_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() + config->item_icon_in_folder_icon_margin() / 2,
        item_2_bounds.x());
    EXPECT_EQ(
        test_rect_center.y() - config->item_icon_in_folder_icon_margin() / 2,
        item_2_bounds.bottom());

    EXPECT_EQ(
        test_rect_center.x() - config->item_icon_in_folder_icon_margin() / 2,
        item_3_bounds.right());
    EXPECT_EQ(
        test_rect_center.y() + config->item_icon_in_folder_icon_margin() / 2,
        item_3_bounds.y());

    EXPECT_EQ(expected_icon_rect_size, item_4_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() + config->item_icon_in_folder_icon_margin() / 2,
        item_4_bounds.x());
    EXPECT_EQ(
        test_rect_center.y() + config->item_icon_in_folder_icon_margin() / 2,
        item_4_bounds.y());
  }
}

TEST_P(FolderImageTest, GetTargetIconRectInFolderWithFiveItems) {
  AddAppWithColoredIcon("app4", SK_ColorYELLOW);
  AddAppWithColoredIcon("app5", SK_ColorYELLOW);

  const AppListConfig* config = GetAppListConfig(/*can_create=*/false);
  ASSERT_TRUE(config);

  const gfx::Rect test_rects[] = {
      gfx::Rect(config->icon_visible_size()),
      gfx::Rect(gfx::Point(10, 10), config->icon_visible_size()),
      gfx::Rect(config->folder_icon_size()),
      gfx::Rect(gfx::Point(10, 10), config->folder_icon_size()),
  };

  for (const auto& test_rect : test_rects) {
    SCOPED_TRACE(::testing::Message()
                 << "Target folder icon bounds: " << test_rect.ToString());

    const gfx::Point test_rect_center = test_rect.CenterPoint();

    const gfx::Size expected_icon_rect_size(
        config->item_icon_in_folder_icon_dimension(),
        config->item_icon_in_folder_icon_dimension());

    gfx::Rect item_1_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app1"), test_rect);
    gfx::Rect item_2_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app2"), test_rect);
    gfx::Rect item_3_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app3"), test_rect);
    gfx::Rect item_4_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app4"), test_rect);
    gfx::Rect item_5_bounds = folder_image_->GetTargetIconRectInFolderForItem(
        *config, app_list_model_->FindItem("app5"), test_rect);

    if (is_rtl()) {
      std::swap(item_1_bounds, item_2_bounds);
      std::swap(item_3_bounds, item_4_bounds);
    }

    EXPECT_EQ(expected_icon_rect_size, item_1_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() - config->item_icon_in_folder_icon_margin() / 2,
        item_1_bounds.right());
    EXPECT_EQ(
        test_rect_center.y() - config->item_icon_in_folder_icon_margin() / 2,
        item_1_bounds.bottom());

    EXPECT_EQ(expected_icon_rect_size, item_2_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() + config->item_icon_in_folder_icon_margin() / 2,
        item_2_bounds.x());
    EXPECT_EQ(
        test_rect_center.y() - config->item_icon_in_folder_icon_margin() / 2,
        item_2_bounds.bottom());

    EXPECT_EQ(
        test_rect_center.x() - config->item_icon_in_folder_icon_margin() / 2,
        item_3_bounds.right());
    EXPECT_EQ(
        test_rect_center.y() + config->item_icon_in_folder_icon_margin() / 2,
        item_3_bounds.y());

    EXPECT_EQ(expected_icon_rect_size, item_4_bounds.size());
    EXPECT_EQ(
        test_rect_center.x() + config->item_icon_in_folder_icon_margin() / 2,
        item_4_bounds.x());
    EXPECT_EQ(
        test_rect_center.y() + config->item_icon_in_folder_icon_margin() / 2,
        item_4_bounds.y());

    EXPECT_EQ(expected_icon_rect_size, item_5_bounds.size());
    EXPECT_EQ(test_rect_center, item_5_bounds.CenterPoint());
  }
}

}  // namespace ash
