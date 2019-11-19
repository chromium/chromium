// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config_provider.h"

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Does sanity check on apps grid item tile dimensions in config. On error, it
// causes test failure with additional |scoped_trace| message.
// |error_margin|: Allowed error margin when comparing vertical dimensions. Used
//     for scaled shared app list config (to account for possible rounding
//     errors).
void SanityCheckGridTileDimensions(AppListConfig* config,
                                   int error_margin,
                                   const std::string& scoped_trace) {
  const int icon_top =
      (config->grid_tile_height() - config->grid_icon_bottom_padding() -
       config->grid_icon_dimension()) /
      2;
  // The app list item icon top should be within the tile bounds.
  EXPECT_GE(icon_top, 0) << scoped_trace;

  const int icon_bottom = icon_top + config->grid_icon_dimension();
  const int title_top = config->grid_tile_height() -
                        config->grid_title_bottom_padding() -
                        config->app_title_font().GetHeight();
  // Icon should not overlap with title.
  EXPECT_LE(icon_bottom, title_top + error_margin) << scoped_trace;

  // Icon should fit within available height.
  EXPECT_LE(icon_bottom,
            config->grid_tile_height() - config->grid_icon_bottom_padding())
      << scoped_trace;

  // Icon should fit in the tile width.
  EXPECT_LE(config->grid_icon_dimension(), config->grid_tile_width())
      << scoped_trace;

  const int folder_unclipped_icon_top =
      (config->grid_tile_height() - config->grid_icon_bottom_padding() -
       config->folder_unclipped_icon_dimension()) /
      2;
  // The app list folder icon top should be within the tile bounds.
  EXPECT_GE(folder_unclipped_icon_top, 0) << scoped_trace;

  // Unclipped folder icon should not overlap with title.
  const int folder_unclipped_icon_bottom =
      folder_unclipped_icon_top + config->folder_unclipped_icon_dimension();
  EXPECT_LE(folder_unclipped_icon_bottom, title_top + error_margin)
      << scoped_trace;

  // Unclipped folder icon should fit within available height.
  EXPECT_LE(folder_unclipped_icon_bottom,
            config->grid_tile_height() - config->grid_icon_bottom_padding())
      << scoped_trace;

  // Unclipped folder icon should fit into tile width.
  EXPECT_LE(config->folder_unclipped_icon_dimension(),
            config->grid_tile_width())
      << scoped_trace;
}

class TestAppListConfigProviderObserver
    : public AppListConfigProvider::Observer {
 public:
  TestAppListConfigProviderObserver() = default;
  ~TestAppListConfigProviderObserver() override = default;

  // AppListConfigProvider::Observer:
  void OnAppListConfigCreated(ash::AppListConfigType config_type) override {
    ASSERT_FALSE(base::Contains(created_types_, config_type));

    created_types_.push_back(config_type);
  }

  const std::vector<ash::AppListConfigType>& created_types() const {
    return created_types_;
  }

  void ClearCreatedTypes() { created_types_.clear(); }

 private:
  std::vector<ash::AppListConfigType> created_types_;

  DISALLOW_COPY_AND_ASSIGN(TestAppListConfigProviderObserver);
};

}  // namespace

class AppListConfigProviderTest : public testing::Test {
 public:
  AppListConfigProviderTest() = default;
  ~AppListConfigProviderTest() override = default;

  void SetUp() override {
    // AppListConfigProvider is base::NoDestruct, which means it may not be
    // cleared between test runs - clear the registry to ensure this test starts
    // with the clean app list config registry.
    AppListConfigProvider::Get().ResetForTesting();
    AppListConfigProvider::Get().AddObserver(&registry_observer_);
  }

  void TearDown() override {
    AppListConfigProvider::Get().RemoveObserver(&registry_observer_);
    AppListConfigProvider::Get().ResetForTesting();
  }

  TestAppListConfigProviderObserver registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(AppListConfigProviderTest);
};

// Tests that shared AppListConfig type is considered available by default, and
// that AppListConfig::instance() can be used to access the default, unscaled
// ash::AppListConfigType::kShared app list config.
TEST_F(AppListConfigProviderTest, SharedInstance) {
  AppListConfig* shared_config = AppListConfigProvider::Get().GetConfigForType(
      ash::AppListConfigType::kShared, false);
  ASSERT_TRUE(shared_config);
  EXPECT_EQ(&AppListConfig::instance(), shared_config);
  // Observer not expected to trigger, as the shared config is considered
  // created by default (even though it's created lazily on first access).
  EXPECT_EQ(std::vector<ash::AppListConfigType>(),
            registry_observer_.created_types());

  EXPECT_EQ(ash::AppListConfigType::kShared, shared_config->type());
  EXPECT_EQ(1., shared_config->scale_x());
  EXPECT_EQ(1., shared_config->scale_y());
}

// Tests GetConfigForType behavior for app list configs associated with
// kScalableAppList feature.
TEST_F(AppListConfigProviderTest, NonSharedConfigGetters) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({app_list_features::kScalableAppList},
                                       {});

  std::vector<ash::AppListConfigType> test_cases = {
      ash::AppListConfigType::kSmall, ash::AppListConfigType::kMedium,
      ash::AppListConfigType::kLarge};
  for (const auto& config_type : test_cases) {
    SCOPED_TRACE(static_cast<int>(config_type));

    // Calling GetConfigForType with false |can_create| will not create a new
    // config.
    EXPECT_FALSE(AppListConfigProvider::Get().GetConfigForType(
        config_type, false /*can_create*/));
    EXPECT_EQ(std::vector<ash::AppListConfigType>(),
              registry_observer_.created_types());

    // Calling GetConfigForType with true |can_create| will create a new config
    // (if not previously created), and it will notify observers a config was
    // created.
    const AppListConfig* config = AppListConfigProvider::Get().GetConfigForType(
        config_type, true /*can_create*/);
    ASSERT_TRUE(config);
    EXPECT_EQ(config_type, config->type());
    const std::vector<ash::AppListConfigType> expected_created_types = {
        config_type};
    EXPECT_EQ(expected_created_types, registry_observer_.created_types());
    EXPECT_NE(&AppListConfig::instance(), config);

    // Subsequent calls to GetConfigForType will return previously created
    // config, and will not notify observers of config creation.
    EXPECT_EQ(config, AppListConfigProvider::Get().GetConfigForType(config_type,
                                                                    false));
    EXPECT_EQ(config,
              AppListConfigProvider::Get().GetConfigForType(config_type, true));
    EXPECT_EQ(expected_created_types, registry_observer_.created_types());

    registry_observer_.ClearCreatedTypes();
  }
}

// Tests calling CreateConfigByDisplayWorkArea creates the appropriate app list
// configuration depending on display size.
TEST_F(AppListConfigProviderTest, CreateConfigByDisplayWorkArea) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({app_list_features::kScalableAppList},
                                       {});

  const struct TestCase {
    gfx::Size work_area_size;
    ash::AppListConfigType config_type;
  } test_cases[] = {{gfx::Size(900, 500), ash::AppListConfigType::kSmall},
                    {gfx::Size(500, 900), ash::AppListConfigType::kSmall},
                    {gfx::Size(960, 600), ash::AppListConfigType::kMedium},
                    {gfx::Size(1100, 700), ash::AppListConfigType::kMedium},
                    {gfx::Size(600, 960), ash::AppListConfigType::kMedium},
                    {gfx::Size(700, 1100), ash::AppListConfigType::kMedium},
                    {gfx::Size(1200, 768), ash::AppListConfigType::kLarge},
                    {gfx::Size(768, 1200), ash::AppListConfigType::kLarge}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Size: " << test_case.work_area_size.ToString()
                 << ", expected config type: "
                 << static_cast<int>(test_case.config_type));

    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            test_case.work_area_size, 32 /*min_horizontal_margin*/,
            56 /*shelf_height*/, nullptr);

    ASSERT_TRUE(config.get());
    EXPECT_EQ(test_case.config_type, config->type());
    EXPECT_EQ(1, config->scale_x());
    EXPECT_EQ(1, config->scale_y());
    SanityCheckGridTileDimensions(config.get(), 0, "");

    // Verify that AppListConfigProvider now provides the created config type.
    EXPECT_TRUE(AppListConfigProvider::Get().GetConfigForType(
        test_case.config_type, false));

    // NOTE: While a specific config might be expected in more than one test
    // case, it should only get reported as created once - given that the
    // observed created types are not cleared for |registry_observer_| between
    // test cases, the "observed" count for |test_case.config_type| should
    // always be 1.
    EXPECT_EQ(1, base::STLCount(registry_observer_.created_types(),
                                test_case.config_type));

    // Verify CreateForAppListWidget returns nullptr if the created config would
    // be the same as |config|.
    EXPECT_FALSE(AppListConfigProvider::Get().CreateForAppListWidget(
        test_case.work_area_size, 32 /*min_horizontal_margin*/,
        56 /*shelf_height*/, config.get()));
  }
}

// Tests whether CreateForAppListWidget returns a new config depending on the
// value of the old config passed to the method.
TEST_F(AppListConfigProviderTest,
       CreateConfigByDisplayWorkAreaWithNonNullConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({app_list_features::kScalableAppList},
                                       {});

  // Create initial configuration.
  std::unique_ptr<AppListConfig> config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(1200, 768), 32 /*min_horizontal_margin*/,
          56 /*shelf_height*/, nullptr);
  ASSERT_TRUE(config);
  EXPECT_EQ(ash::AppListConfigType::kLarge, config->type());

  // Verify CreateForAppListWidget returns nullptr if the created config would
  // be the same as |config|.
  EXPECT_FALSE(AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(768, 1200), 32 /*min_horizontal_margin*/, 56 /*shelf_height*/,
      config.get()));

  // Create different config.
  std::unique_ptr<AppListConfig> updated_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(960, 600), 32 /*min_horizontal_margin*/,
          56 /*shelf_height*/, config.get());
  ASSERT_TRUE(updated_config);
  EXPECT_EQ(ash::AppListConfigType::kMedium, updated_config->type());
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaLandscape) {
  // Explicitly disable kScalableAppList feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({},
                                       {app_list_features::kScalableAppList});

  const int kMinGridWidth = 560;   // 112 * 5
  const int kMinGridHeight = 480;  // 120 * 4;

  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(1200, 768) /*display_work_area_size*/,
          32 /*min_horizontal_margin*/, 56 /*shelf_height*/, nullptr);
  ASSERT_TRUE(config.get());
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(1, config->scale_x());
  EXPECT_EQ(1, config->scale_y());
  EXPECT_EQ(112, config->grid_tile_width());
  EXPECT_EQ(120, config->grid_tile_height());

  auto get_grid_title_height = [](AppListConfig* config) {
    return config->grid_tile_height() - config->grid_title_top_padding() -
           config->grid_title_bottom_padding();
  };
  const int kDefaultGridTileTitleHeight = get_grid_title_height(config.get());

  // The available grid size still fits the grid - verify that
  // CreateForAppListWidget does not create a new config identical to the
  // previous one.
  EXPECT_FALSE(AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(1200, 768) /*display_work_area_size*/,
      32 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get()));

  // The app list has to be scaled down horizontally.
  config = AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(800, 700) /*display_work_area_size*/,
      150 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get());
  ASSERT_TRUE(config);
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(500.f / kMinGridWidth, config->scale_x());
  EXPECT_EQ(1, config->scale_y());
  // 100 == std::round(scale_x * 112)
  EXPECT_EQ(100, config->grid_tile_width());
  EXPECT_EQ(120, config->grid_tile_height());
  // Grid title height should not be scaled down.
  EXPECT_EQ(kDefaultGridTileTitleHeight, get_grid_title_height(config.get()));
  SanityCheckGridTileDimensions(config.get(), 1, "Horizontal scaling");

  // The app list has to be scaled down vertically.
  config = AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(800, 624) /*display_work_area_size*/,
      32 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get());
  ASSERT_TRUE(config);
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(1, config->scale_x());
  // Available height includes fadeout zones, which should not be included in
  // scale calculation.
  EXPECT_EQ(400.f / kMinGridHeight, config->scale_y());
  EXPECT_EQ(112, config->grid_tile_width());
  // 100 == std::round(scale_y * 120)
  EXPECT_EQ(100, config->grid_tile_height());
  // Grid title height should not be scaled down.
  EXPECT_EQ(kDefaultGridTileTitleHeight, get_grid_title_height(config.get()));
  SanityCheckGridTileDimensions(config.get(), 1, "Vertical scaling");

  // Both vertical and horizontal scaling required.
  config = AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(800, 624) /*display_work_area_size*/,
      150 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get());
  ASSERT_TRUE(config);
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(500.f / kMinGridWidth, config->scale_x());
  // Available height includes fadeout zones, which should not be included in
  // scale calculation.
  EXPECT_EQ(400.f / kMinGridHeight, config->scale_y());
  // 100 == std::round(scale_x * 112)
  EXPECT_EQ(100, config->grid_tile_width());
  // 100 == std::round(scale_y * 120)
  EXPECT_EQ(100, config->grid_tile_height());
  // Grid title height should not be scaled down.
  EXPECT_EQ(kDefaultGridTileTitleHeight, get_grid_title_height(config.get()));
  SanityCheckGridTileDimensions(config.get(), 1,
                                "Horizontal and vertical scaling");
}

TEST_F(AppListConfigProviderTest, CreateScaledConfigByDisplayWorkAreaPortrait) {
  // Explicitly disable kScalableAppList feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({},
                                       {app_list_features::kScalableAppList});

  const int kMinGridWidth = 448;   // 112 * 4
  const int kMinGridHeight = 600;  // 120 * 5;

  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(768, 1200) /*display_work_area_size*/,
          32 /*min_horizontal_margin*/, 56 /*shelf_height*/, nullptr);
  ASSERT_TRUE(config.get());
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(1, config->scale_x());
  EXPECT_EQ(1, config->scale_y());
  EXPECT_EQ(112, config->grid_tile_width());
  EXPECT_EQ(120, config->grid_tile_height());

  auto get_grid_title_height = [](AppListConfig* config) {
    return config->grid_tile_height() - config->grid_title_top_padding() -
           config->grid_title_bottom_padding();
  };
  const int kDefaultGridTileTitleHeight = get_grid_title_height(config.get());

  // The available grid size still fits the grid - verify that
  // CreateForAppListWidget does not create a new config identical to the
  // previous one.
  EXPECT_FALSE(AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(768, 1200) /*display_work_area_size*/,
      32 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get()));

  // The app list has to be scaled down horizontally.
  config = AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(600, 800) /*display_work_area_size*/,
      100 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get());
  ASSERT_TRUE(config);
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(400.f / kMinGridWidth, config->scale_x());
  EXPECT_EQ(1, config->scale_y());
  // 100 == std::round(scale_x * 112)
  EXPECT_EQ(100, config->grid_tile_width());
  EXPECT_EQ(120, config->grid_tile_height());
  // Grid title height should not be scaled down.
  EXPECT_EQ(kDefaultGridTileTitleHeight, get_grid_title_height(config.get()));
  SanityCheckGridTileDimensions(config.get(), 1, "Horizontal scaling");

  // The app list has to be scaled down vertically.
  config = AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(600, 624) /*display_work_area_size*/,
      32 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get());
  ASSERT_TRUE(config);
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(1, config->scale_x());
  // Available height includes fadeout zones, which should not be included in
  // scale calculation.
  EXPECT_EQ(400.f / kMinGridHeight, config->scale_y());
  EXPECT_EQ(112, config->grid_tile_width());
  // 80 == std::round(scale_y * 120)
  EXPECT_EQ(80, config->grid_tile_height());
  // Grid title height should not be scaled down.
  EXPECT_EQ(kDefaultGridTileTitleHeight, get_grid_title_height(config.get()));
  SanityCheckGridTileDimensions(config.get(), 1, "Vertical scaling");

  // Both vertical and horizontal scaling required.
  config = AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(600, 732) /*display_work_area_size*/,
      150 /*min_horizontal_margin*/, 56 /*shelf_height*/, config.get());
  ASSERT_TRUE(config);
  EXPECT_EQ(ash::AppListConfigType::kShared, config->type());
  EXPECT_EQ(300.f / kMinGridWidth, config->scale_x());
  // Available height includes fadeout zones, which should not be included in
  // scale calculation.
  EXPECT_EQ(500.f / kMinGridHeight, config->scale_y());
  // 75 == std::round(scale_x * 112)
  EXPECT_EQ(75, config->grid_tile_width());
  // 100 == std::round(scale_y * 120)
  EXPECT_EQ(100, config->grid_tile_height());
  // Grid title height should not be scaled down.
  EXPECT_EQ(kDefaultGridTileTitleHeight, get_grid_title_height(config.get()));
  SanityCheckGridTileDimensions(config.get(), 1,
                                "Horizontal and vertical scaling");
}

}  // namespace ash
