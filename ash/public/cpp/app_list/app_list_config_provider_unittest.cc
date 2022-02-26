// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config_provider.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Returns expected number of rows in the fullscreen app list apps grid
// depending on the display work area (when ProductivityLauncher is not
// enabled).
int GetPreferredGridRowsForWorkArea(const gfx::Size& work_area_size) {
  return work_area_size.width() > work_area_size.height() ? 4 : 5;
}

// Returns expected number of columns in the fullscreen app list apps grid
// depending on the display work area (when ProductivityLauncher is not
// enabled).
int GetPreferredGridColumnsForWorkArea(const gfx::Size& work_area_size) {
  if (ash::features::IsProductivityLauncherEnabled())
    return 5;
  return work_area_size.width() > work_area_size.height() ? 5 : 4;
}

// Does sanity check on apps grid item tile dimensions in config. On error, it
// causes test failure with additional |scoped_trace| message.
// |error_margin|: Allowed error margin when comparing vertical dimensions. Used
//     for scaled app list config (to account for possible rounding
//     errors).
void SanityCheckGridTileDimensions(AppListConfig* config, int error_margin) {
  const int icon_top =
      (config->grid_tile_height() - config->grid_icon_bottom_padding() -
       config->grid_icon_dimension()) /
      2;
  // The app list item icon top should be within the tile bounds.
  EXPECT_GE(icon_top, 0);

  const int icon_bottom = icon_top + config->grid_icon_dimension();
  const int title_top = config->grid_tile_height() -
                        config->grid_title_bottom_padding() -
                        config->app_title_font().GetHeight();
  // Icon should not overlap with title.
  EXPECT_LE(icon_bottom, title_top + error_margin);

  // Icon should fit within available height.
  EXPECT_LE(icon_bottom,
            config->grid_tile_height() - config->grid_icon_bottom_padding());

  // Icon should fit in the tile width.
  EXPECT_LE(config->grid_icon_dimension(), config->grid_tile_width());

  const int folder_unclipped_icon_top =
      (config->grid_tile_height() - config->grid_icon_bottom_padding() -
       config->folder_unclipped_icon_dimension()) /
      2;
  // The app list folder icon top should be within the tile bounds.
  EXPECT_GE(folder_unclipped_icon_top, 0);

  // Unclipped folder icon should not overlap with title.
  const int folder_unclipped_icon_bottom =
      folder_unclipped_icon_top + config->folder_unclipped_icon_dimension();
  EXPECT_LE(folder_unclipped_icon_bottom, title_top + error_margin);

  // Unclipped folder icon should fit within available height.
  EXPECT_LE(folder_unclipped_icon_bottom,
            config->grid_tile_height() - config->grid_icon_bottom_padding());

  // Unclipped folder icon should fit into tile width.
  EXPECT_LE(config->folder_unclipped_icon_dimension(),
            config->grid_tile_width());
}

class TestAppListConfigProviderObserver
    : public AppListConfigProvider::Observer {
 public:
  TestAppListConfigProviderObserver() = default;

  TestAppListConfigProviderObserver(const TestAppListConfigProviderObserver&) =
      delete;
  TestAppListConfigProviderObserver& operator=(
      const TestAppListConfigProviderObserver&) = delete;

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

  void VerifyScaledConfig(const AppListConfig& base_config,
                          AppListConfig* config,
                          float scale_x,
                          float scale_y) {
    ASSERT_TRUE(config);
    EXPECT_EQ(base_config.type(), config->type());

    EXPECT_EQ(scale_x, config->scale_x());
    EXPECT_EQ(scale_y, config->scale_y());

    EXPECT_EQ(std::round(base_config.grid_tile_width() * scale_x),
              config->grid_tile_width());
    EXPECT_EQ(std::round(base_config.grid_tile_height() * scale_y),
              config->grid_tile_height());

    auto get_grid_title_height = [](const AppListConfig* config) {
      return config->grid_tile_height() - config->grid_title_top_padding() -
             config->grid_title_bottom_padding();
    };
    EXPECT_EQ(get_grid_title_height(&base_config),
              get_grid_title_height(config));

    SanityCheckGridTileDimensions(config, 1);
  }

 protected:
  TestAppListConfigProviderObserver registry_observer_;
};

// Tests GetConfigForType behavior.
TEST_F(AppListConfigProviderTest, ConfigGetters) {
  // The configs tested here are only used by ProductivityLauncher.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kProductivityLauncher);

  std::vector<AppListConfigType> test_cases = {AppListConfigType::kRegular,
                                               AppListConfigType::kDense};
  std::set<AppListConfigType> created_types;
  for (const auto& config_type : test_cases) {
    SCOPED_TRACE(static_cast<int>(config_type));

    // Calling GetConfigForType with false |can_create| will not create a new
    // config.
    EXPECT_FALSE(AppListConfigProvider::Get().GetConfigForType(
        config_type, false /*can_create*/));
    EXPECT_EQ(std::vector<AppListConfigType>(),
              registry_observer_.created_types());

    // Calling GetConfigForType with true |can_create| will create a new config
    // (if not previously created), and it will notify observers a config was
    // created.
    const AppListConfig* config = AppListConfigProvider::Get().GetConfigForType(
        config_type, true /*can_create*/);
    ASSERT_TRUE(config);
    created_types.insert(config_type);
    EXPECT_EQ(config_type, config->type());
    const std::vector<AppListConfigType> expected_created_types = {config_type};
    EXPECT_EQ(expected_created_types, registry_observer_.created_types());

    // Subsequent calls to GetConfigForType will return previously created
    // config, and will not notify observers of config creation.
    EXPECT_EQ(config, AppListConfigProvider::Get().GetConfigForType(config_type,
                                                                    false));
    EXPECT_EQ(config,
              AppListConfigProvider::Get().GetConfigForType(config_type, true));
    EXPECT_EQ(expected_created_types, registry_observer_.created_types());

    EXPECT_EQ(created_types,
              AppListConfigProvider::Get().GetAvailableConfigTypes());

    registry_observer_.ClearCreatedTypes();
  }
}

// Tests GetConfigForType behavior for pre-productivity launcher configs.
TEST_F(AppListConfigProviderTest, LegacyConfigGetters) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  std::vector<AppListConfigType> test_cases = {AppListConfigType::kSmall,
                                               AppListConfigType::kMedium,
                                               AppListConfigType::kLarge};
  std::set<AppListConfigType> created_types;
  for (const auto& config_type : test_cases) {
    SCOPED_TRACE(static_cast<int>(config_type));

    // Calling GetConfigForType with false |can_create| will not create a new
    // config.
    EXPECT_FALSE(AppListConfigProvider::Get().GetConfigForType(
        config_type, false /*can_create*/));
    EXPECT_EQ(std::vector<AppListConfigType>(),
              registry_observer_.created_types());

    // Calling GetConfigForType with true |can_create| will create a new config
    // (if not previously created), and it will notify observers a config was
    // created.
    const AppListConfig* config = AppListConfigProvider::Get().GetConfigForType(
        config_type, true /*can_create*/);
    ASSERT_TRUE(config);
    created_types.insert(config_type);
    EXPECT_EQ(config_type, config->type());
    const std::vector<AppListConfigType> expected_created_types = {config_type};
    EXPECT_EQ(expected_created_types, registry_observer_.created_types());

    // Subsequent calls to GetConfigForType will return previously created
    // config, and will not notify observers of config creation.
    EXPECT_EQ(config, AppListConfigProvider::Get().GetConfigForType(config_type,
                                                                    false));
    EXPECT_EQ(config,
              AppListConfigProvider::Get().GetConfigForType(config_type, true));
    EXPECT_EQ(expected_created_types, registry_observer_.created_types());

    EXPECT_EQ(created_types,
              AppListConfigProvider::Get().GetAvailableConfigTypes());

    registry_observer_.ClearCreatedTypes();
  }
}

// Tests calling CreateForFullscreenAppList creates the appropriate app list
// configuration depending on display size.
TEST_F(AppListConfigProviderTest, CreateConfigByDisplayWorkArea) {
  // The configs tested here are only used by ProductivityLauncher.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kProductivityLauncher);

  // NOTE: The `available_size` are arbitrary values large enough so the
  // returned app list config does not get scaled down (i.e. large enough so
  // they can fit an apps grid with default sized-items).
  const struct TestCase {
    gfx::Size work_area_size;
    gfx::Size available_size;
    AppListConfigType config_type;
  } test_cases[] = {
      {gfx::Size(900, 500), gfx::Size(788, 321), AppListConfigType::kDense},
      {gfx::Size(540, 900), gfx::Size(428, 704), AppListConfigType::kDense},
      {gfx::Size(960, 600), gfx::Size(848, 412), AppListConfigType::kDense},
      {gfx::Size(1100, 700), gfx::Size(988, 504), AppListConfigType::kRegular},
      {gfx::Size(600, 960), gfx::Size(488, 764), AppListConfigType::kDense},
      {gfx::Size(700, 1100), gfx::Size(588, 904), AppListConfigType::kRegular},
      {gfx::Size(1200, 768), gfx::Size(1088, 572), AppListConfigType::kRegular},
      {gfx::Size(768, 1200), gfx::Size(656, 1004),
       AppListConfigType::kRegular}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Size: " << test_case.work_area_size.ToString()
                 << ", expected config type: "
                 << static_cast<int>(test_case.config_type));

    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            test_case.work_area_size,
            -1 /*row count is ignored when productivity launcher is enabled*/,
            GetPreferredGridColumnsForWorkArea(test_case.work_area_size),
            test_case.available_size, nullptr);

    ASSERT_TRUE(config.get());
    EXPECT_EQ(test_case.config_type, config->type());
    EXPECT_EQ(1, config->scale_x());
    EXPECT_EQ(1, config->scale_y());
    SanityCheckGridTileDimensions(config.get(), 0);

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
    EXPECT_FALSE(AppListConfigProvider::Get().CreateForFullscreenAppList(
        test_case.work_area_size,
        GetPreferredGridRowsForWorkArea(test_case.work_area_size),
        GetPreferredGridColumnsForWorkArea(test_case.work_area_size),
        test_case.available_size, config.get()));
  }
}

// Tests calling CreateForFullscreenAppList creates the appropriate app list
// configuration depending on display size with productivity launcher disabled.
TEST_F(AppListConfigProviderTest, CreateLegacyConfigByDisplayWorkArea) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // NOTE: The `available_size` are arbitrary values large enough so the
  // returned app list config does not get scaled down (i.e. large enough so
  // they can fit an apps grid with default sized-items).
  const struct TestCase {
    gfx::Size work_area_size;
    gfx::Size available_size;
    AppListConfigType config_type;
  } test_cases[] = {
      {gfx::Size(900, 500), gfx::Size(788, 321), AppListConfigType::kSmall},
      {gfx::Size(500, 900), gfx::Size(388, 704), AppListConfigType::kSmall},
      {gfx::Size(960, 600), gfx::Size(848, 412), AppListConfigType::kMedium},
      {gfx::Size(1100, 700), gfx::Size(988, 504), AppListConfigType::kMedium},
      {gfx::Size(600, 960), gfx::Size(488, 764), AppListConfigType::kMedium},
      {gfx::Size(700, 1100), gfx::Size(588, 904), AppListConfigType::kMedium},
      {gfx::Size(1200, 768), gfx::Size(1088, 572), AppListConfigType::kLarge},
      {gfx::Size(768, 1200), gfx::Size(656, 1004), AppListConfigType::kLarge}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Size: " << test_case.work_area_size.ToString()
                 << ", expected config type: "
                 << static_cast<int>(test_case.config_type));

    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            test_case.work_area_size,
            GetPreferredGridRowsForWorkArea(test_case.work_area_size),
            GetPreferredGridColumnsForWorkArea(test_case.work_area_size),
            test_case.available_size, nullptr);

    ASSERT_TRUE(config.get());
    EXPECT_EQ(test_case.config_type, config->type());
    EXPECT_EQ(1, config->scale_x());
    EXPECT_EQ(1, config->scale_y());
    SanityCheckGridTileDimensions(config.get(), 0);

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
    EXPECT_FALSE(AppListConfigProvider::Get().CreateForFullscreenAppList(
        test_case.work_area_size,
        GetPreferredGridRowsForWorkArea(test_case.work_area_size),
        GetPreferredGridColumnsForWorkArea(test_case.work_area_size),
        test_case.available_size, config.get()));
  }
}

// Tests whether CreateForAppListWidget returns a new config depending on the
// value of the old config passed to the method.
TEST_F(AppListConfigProviderTest,
       CreateConfigByDisplayWorkAreaWithNonNullConfig) {
  // The configs tested here are only used by ProductivityLauncher.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kProductivityLauncher);

  // Create initial configuration.
  gfx::Size work_area(1200, 768);
  gfx::Size available_size(1088, 572);
  std::unique_ptr<AppListConfig> config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, -1 /*rows not used for productivity launcher configs*/,
          GetPreferredGridColumnsForWorkArea(work_area), available_size,
          nullptr);
  ASSERT_TRUE(config);
  EXPECT_EQ(AppListConfigType::kRegular, config->type());

  // Verify CreateForAppListWidget returns nullptr if the created config would
  // be the same as `config`.
  work_area = gfx::Size(768, 1200);
  available_size = gfx::Size(656, 1004);
  EXPECT_FALSE(AppListConfigProvider::Get().CreateForFullscreenAppList(
      work_area, -1 /*rows not used for productivity launcher configs*/,
      GetPreferredGridColumnsForWorkArea(work_area), available_size,
      config.get()));

  // Create different config.
  work_area = gfx::Size(960, 600);
  available_size = gfx::Size(848, 412);
  std::unique_ptr<AppListConfig> updated_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, GetPreferredGridRowsForWorkArea(work_area),
          GetPreferredGridColumnsForWorkArea(work_area), available_size,
          config.get());
  ASSERT_TRUE(updated_config);
  EXPECT_EQ(AppListConfigType::kDense, updated_config->type());
}

// Tests whether CreateForAppListWidget returns a new config depending on the
// value of the old config passed to the method.
TEST_F(AppListConfigProviderTest,
       CreateLegacyConfigByDisplayWorkAreaWithNonNullConfig) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // Create initial configuration.
  gfx::Size work_area(1200, 768);
  gfx::Size available_size(1088, 572);
  std::unique_ptr<AppListConfig> config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, GetPreferredGridRowsForWorkArea(work_area),
          GetPreferredGridColumnsForWorkArea(work_area), available_size,
          nullptr);
  ASSERT_TRUE(config);
  EXPECT_EQ(AppListConfigType::kLarge, config->type());

  // Verify CreateForAppListWidget returns nullptr if the created config would
  // be the same as `config`.
  work_area = gfx::Size(768, 1200);
  available_size = gfx::Size(656, 1004);
  EXPECT_FALSE(AppListConfigProvider::Get().CreateForFullscreenAppList(
      work_area, GetPreferredGridRowsForWorkArea(work_area),
      GetPreferredGridColumnsForWorkArea(work_area), available_size,
      config.get()));

  // Create different config.
  work_area = gfx::Size(960, 600);
  available_size = gfx::Size(848, 412);
  std::unique_ptr<AppListConfig> updated_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, GetPreferredGridRowsForWorkArea(work_area),
          GetPreferredGridColumnsForWorkArea(work_area), available_size,
          config.get());
  ASSERT_TRUE(updated_config);
  EXPECT_EQ(AppListConfigType::kMedium, updated_config->type());
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaRegularLandscape) {
  // The configs tested here are only used by ProductivityLauncher.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(1200, 768);
  const gfx::Size initial_available_size(1088, 572);
  // Rows are not used for calculating productivity launcher configs - select a
  // reasonable arbitrary value for number of rows.
  const int preferred_rows = 4;
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kRegular, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(480, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 480.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid doesn't fit `preferred_rows` - the
    // config should not be scaled, as apps grid is expected to reduce the
    // number of visible rows in this case.
    const gfx::Size available_size(initial_available_size.width(), 400);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 1);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width and height, and expect the grid to scale down
    // horizontally only.
    const gfx::Size available_size(480, 400);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 480.0f / kMinGridWidth, 1);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaDenseLandscape) {
  // The configs tested here are only used by ProductivityLauncher.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(960, 600);
  const gfx::Size initial_available_size(848, 412);
  // Rows are not used for calculating productivity launcher configs - select a
  // reasonable arbitrary value for number of rows.
  const int preferred_rows = 4;
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);
  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kDense, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(300, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid doesn't fit `preferred_rows` - the
    // config should not be scaled, as apps grid is expected to reduce the
    // number of visible rows in this case.
    const gfx::Size available_size(initial_available_size.width(), 200);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 1);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width and height, and expect the grid to scale down
    // horizontally only.
    const gfx::Size available_size(300, 200);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth, 1);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaLargeLandscape) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(1200, 768);
  const gfx::Size initial_available_size(1088, 572);
  const int preferred_rows = GetPreferredGridRowsForWorkArea(work_area);
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kLarge, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;
  const int kMinGridHeight = base_config->grid_tile_height() * preferred_rows;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(480, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 480.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid scales down vertically.
    const gfx::Size available_size(initial_available_size.width(), 400);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 400.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width and height so the grid scales down
    // horizontally and vertically.
    const gfx::Size available_size(480, 400);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 480.0f / kMinGridWidth,
                       400.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaMediumLandscape) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(960, 600);
  const gfx::Size initial_available_size(848, 412);
  const int preferred_rows = GetPreferredGridRowsForWorkArea(work_area);
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);
  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kMedium, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;
  const int kMinGridHeight = base_config->grid_tile_height() * preferred_rows;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(400, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 400.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid scales down vertically.
    const gfx::Size available_size(initial_available_size.width(), 300);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 300.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width height so the grid scales down horizontally
    // and vertically.
    const gfx::Size available_size(400, 300);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 400.0f / kMinGridWidth,
                       300.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaSmallLandscape) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(900, 500);
  const gfx::Size initial_available_size(788, 321);
  const int preferred_rows = GetPreferredGridRowsForWorkArea(work_area);
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kSmall, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;
  const int kMinGridHeight = base_config->grid_tile_height() * preferred_rows;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(340, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 340.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid scales down vertically.
    const gfx::Size available_size(initial_available_size.width(), 260);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 260.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width height so the grid scales down horizontally
    // and vertically.
    const gfx::Size available_size(340, 260);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 340.0f / kMinGridWidth,
                       260.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaRegularPortrait) {
  // The configs tested here are only used by ProductivityLauncher.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(768, 1200);
  const gfx::Size initial_available_size(656, 1004);
  // Rows are not used for calculating productivity launcher configs - select a
  // reasonable arbitrary value for number of rows.
  const int preferred_rows = 5;
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kRegular, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(440, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 440.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid doesn't fit `preferred_rows` - the
    // config should not be scaled, as apps grid is expected to reduce the
    // number of visible rows in this case.
    const gfx::Size available_size(initial_available_size.width(), 532);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 1);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width and height, and expect the grid to scale down
    // horizontally only.
    const gfx::Size available_size(440, 532);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 440.0f / kMinGridWidth, 1);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaDensePortrait) {
  // The configs tested here are only used by ProductivityLauncher.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(600, 960);
  const gfx::Size initial_available_size(488, 764);
  // Rows are not used for calculating productivity launcher configs - select a
  // reasonable arbitrary value for number of rows.
  const int preferred_rows = 5;
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kDense, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(300, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid doesn't fit `preferred_rows` - the
    // config should not be scaled, as apps grid is expected to reduce the
    // number of visible rows in this case.
    const gfx::Size available_size(initial_available_size.width(), 360);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 1);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width and height, and expect the grid to scale down
    // horizontally only.
    const gfx::Size available_size(300, 320);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth, 1);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaLargePortrait) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(768, 1200);
  const gfx::Size initial_available_size(656, 1004);
  const int preferred_rows = GetPreferredGridRowsForWorkArea(work_area);
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kLarge, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;
  const int kMinGridHeight = base_config->grid_tile_height() * preferred_rows;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(440, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 440.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid scales down vertically.
    const gfx::Size available_size(initial_available_size.width(), 532);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 532.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width height so the grid scales down horizontally
    // and vertically.
    const gfx::Size available_size(440, 532);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 440.0f / kMinGridWidth,
                       532.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaMediumPortrait) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(600, 960);
  const gfx::Size initial_available_size(488, 764);
  const int preferred_rows = GetPreferredGridRowsForWorkArea(work_area);
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kMedium, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;
  const int kMinGridHeight = base_config->grid_tile_height() * preferred_rows;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(300, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid scales down vertically.
    const gfx::Size available_size(initial_available_size.width(), 360);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 360.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width height so the grid scales down horizontally
    // and vertically.
    const gfx::Size available_size(300, 360);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth,
                       360.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaSmallPortrait) {
  // The configs tested here are not used by ProductivityLauncher. This test
  // can be deleted when ProductivityLauncher is the default.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kProductivityLauncher);

  // The available grid size fits the grid - created config is not scaled.
  const gfx::Size work_area(500, 900);
  const gfx::Size initial_available_size(388, 704);
  const int preferred_rows = GetPreferredGridRowsForWorkArea(work_area);
  const int preferred_columns = GetPreferredGridColumnsForWorkArea(work_area);
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          work_area, preferred_rows, preferred_columns, initial_available_size,
          nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kSmall, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth = base_config->grid_tile_width() * preferred_columns;
  const int kMinGridHeight = base_config->grid_tile_height() * preferred_rows;

  {
    SCOPED_TRACE("Horizontal scaling");

    // Reduce available width so the grid scales down horizontally.
    const gfx::Size available_size(240, initial_available_size.height());
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 240.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");

    // Reduce available height so the grid scales down vertically.
    const gfx::Size available_size(initial_available_size.width(), 300);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 1, 300.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");

    // Reduce both available width height so the grid scales down horizontally
    // and vertically.
    const gfx::Size available_size(240, 300);
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForFullscreenAppList(
            work_area, preferred_rows, preferred_columns, available_size,
            nullptr);
    VerifyScaledConfig(*base_config, config.get(), 240.0f / kMinGridWidth,
                       300.0f / kMinGridHeight);
  }
}

}  // namespace ash
