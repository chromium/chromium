// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config_provider.h"

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

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

// Tests that shared AppListConfig type is considered available by default, and
// that AppListConfig::instance() can be used to access the default, unscaled
// ash::AppListConfigType::kShared app list config.
TEST_F(AppListConfigProviderTest, SharedInstance) {
  AppListConfig* shared_config = AppListConfigProvider::Get().GetConfigForType(
      AppListConfigType::kShared, false);
  ASSERT_TRUE(shared_config);
  EXPECT_EQ(&AppListConfig::instance(), shared_config);
  // Observer not expected to trigger, as the shared config is considered
  // created by default (even though it's created lazily on first access).
  EXPECT_EQ(std::vector<AppListConfigType>(),
            registry_observer_.created_types());

  EXPECT_EQ(AppListConfigType::kShared, shared_config->type());
  EXPECT_EQ(1., shared_config->scale_x());
  EXPECT_EQ(1., shared_config->scale_y());
}

// Tests GetConfigForType behavior for non-shared app list configs.
TEST_F(AppListConfigProviderTest, NonSharedConfigGetters) {
  std::vector<AppListConfigType> test_cases = {AppListConfigType::kSmall,
                                               AppListConfigType::kMedium,
                                               AppListConfigType::kLarge};
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
    EXPECT_EQ(config_type, config->type());
    const std::vector<AppListConfigType> expected_created_types = {config_type};
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
  const struct TestCase {
    gfx::Size work_area_size;
    AppListConfigType config_type;
  } test_cases[] = {{gfx::Size(900, 500), AppListConfigType::kSmall},
                    {gfx::Size(500, 900), AppListConfigType::kSmall},
                    {gfx::Size(960, 600), AppListConfigType::kMedium},
                    {gfx::Size(1100, 700), AppListConfigType::kMedium},
                    {gfx::Size(600, 960), AppListConfigType::kMedium},
                    {gfx::Size(700, 1100), AppListConfigType::kMedium},
                    {gfx::Size(1200, 768), AppListConfigType::kLarge},
                    {gfx::Size(768, 1200), AppListConfigType::kLarge}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Size: " << test_case.work_area_size.ToString()
                 << ", expected config type: "
                 << static_cast<int>(test_case.config_type));

    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            test_case.work_area_size, gfx::Insets(0, 0, 56, 0) /*shelf_insets*/,
            nullptr);

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
    EXPECT_FALSE(AppListConfigProvider::Get().CreateForAppListWidget(
        test_case.work_area_size, gfx::Insets(0, 0, 56, 0) /*shelf_insets*/,
        config.get()));
  }
}

// Tests whether CreateForAppListWidget returns a new config depending on the
// value of the old config passed to the method.
TEST_F(AppListConfigProviderTest,
       CreateConfigByDisplayWorkAreaWithNonNullConfig) {
  // Create initial configuration.
  std::unique_ptr<AppListConfig> config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(1200, 768), gfx::Insets(0, 0, 56, 0) /*shelf_insets*/,
          nullptr);
  ASSERT_TRUE(config);
  EXPECT_EQ(AppListConfigType::kLarge, config->type());

  // Verify CreateForAppListWidget returns nullptr if the created config would
  // be the same as |config|.
  EXPECT_FALSE(AppListConfigProvider::Get().CreateForAppListWidget(
      gfx::Size(768, 1200), gfx::Insets(0, 0, 56, 0) /*shelf_insets*/,
      config.get()));

  // Create different config.
  std::unique_ptr<AppListConfig> updated_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(960, 600), gfx::Insets(0, 0, 56, 0) /*shelf_insets*/,
          config.get());
  ASSERT_TRUE(updated_config);
  EXPECT_EQ(AppListConfigType::kMedium, updated_config->type());
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaLargeLandscape) {
  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(1200, 768) /*display_work_area_size*/,
          gfx::Insets(0, 0, 56, 0) /*shelf_insets*/, nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kLarge, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth =
      base_config->grid_tile_width() * base_config->preferred_cols();
  const int kMinGridHeight =
      base_config->grid_tile_height() * base_config->preferred_rows();

  {
    SCOPED_TRACE("Horizontal scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(1200, 768) /*display_work_area_size*/,
            gfx::Insets(0, 304, 56, 304) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 480.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(1200, 600) /*display_work_area_size*/,
            gfx::Insets(0, 0, 40, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 1, 400.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(1200, 600) /*display_work_area_size*/,
            gfx::Insets(0, 304, 40, 304) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 480.0f / kMinGridWidth,
                       400.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaMediumLandscape) {
  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(960, 600) /*display_work_area_size*/,
          gfx::Insets(0, 0, 56, 0) /*shelf_insets*/, nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kMedium, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth =
      base_config->grid_tile_width() * base_config->preferred_cols();
  const int kMinGridHeight =
      base_config->grid_tile_height() * base_config->preferred_rows();

  {
    SCOPED_TRACE("Horizontal scaling");

    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(960, 600) /*display_work_area_size*/,
            gfx::Insets(0, 224, 56, 224) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 400.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(960, 500) /*display_work_area_size*/,
            gfx::Insets(0, 0, 40, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 1, 300.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(960, 500) /*display_work_area_size*/,
            gfx::Insets(0, 224, 40, 224) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 400.0f / kMinGridWidth,
                       300.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaSmallLandscape) {
  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(900, 500) /*display_work_area_size*/,
          gfx::Insets(0, 0, 56, 0) /*shelf_insets*/, nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kSmall, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth =
      base_config->grid_tile_width() * base_config->preferred_cols();
  const int kMinGridHeight =
      base_config->grid_tile_height() * base_config->preferred_rows();

  {
    SCOPED_TRACE("Horizontal scaling");

    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(500, 480) /*display_work_area_size*/,
            gfx::Insets(0, 24, 0, 24) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 340.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(900, 460) /*display_work_area_size*/,
            gfx::Insets(0, 0, 40, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 1, 260.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(500, 460) /*display_work_area_size*/,
            gfx::Insets(0, 24, 40, 24) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 340.0f / kMinGridWidth,
                       260.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaLargePortrait) {
  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(768, 1200) /*display_work_area_size*/,
          gfx::Insets(0, 0, 56, 0) /*shelf_insets*/, nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kLarge, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth =
      base_config->grid_tile_width() * base_config->preferred_rows();
  const int kMinGridHeight =
      base_config->grid_tile_height() * base_config->preferred_cols();

  {
    SCOPED_TRACE("Horizontal scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(768, 1200) /*display_work_area_size*/,
            gfx::Insets(0, 108, 56, 108) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 440.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(768, 800) /*display_work_area_size*/,
            gfx::Insets(0, 0, 100, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 1, 532.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(768, 800) /*display_work_area_size*/,
            gfx::Insets(0, 108, 100, 108) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 440.0f / kMinGridWidth,
                       532.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaMediumPortrait) {
  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(600, 960) /*display_work_area_size*/,
          gfx::Insets(0, 0, 56, 0) /*shelf_insets*/, nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kMedium, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth =
      base_config->grid_tile_width() * base_config->preferred_rows();
  const int kMinGridHeight =
      base_config->grid_tile_height() * base_config->preferred_cols();

  {
    SCOPED_TRACE("Horizontal scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(600, 960) /*display_work_area_size*/,
            gfx::Insets(0, 94, 0, 94) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(600, 620) /*display_work_area_size*/,
            gfx::Insets(0, 0, 100, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 1, 360.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(600, 620) /*display_work_area_size*/,
            gfx::Insets(0, 94, 100, 94) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 300.0f / kMinGridWidth,
                       360.0f / kMinGridHeight);
  }
}

TEST_F(AppListConfigProviderTest,
       CreateScaledConfigByDisplayWorkAreaSmallPortrait) {
  // The available grid size fits the grid - created config is not scaled.
  std::unique_ptr<AppListConfig> base_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          gfx::Size(500, 900) /*display_work_area_size*/,
          gfx::Insets(0, 0, 56, 0) /*shelf_insets*/, nullptr);

  ASSERT_TRUE(base_config.get());
  ASSERT_EQ(AppListConfigType::kSmall, base_config->type());
  ASSERT_EQ(1, base_config->scale_x());
  ASSERT_EQ(1, base_config->scale_y());

  const int kMinGridWidth =
      base_config->grid_tile_width() * base_config->preferred_rows();
  const int kMinGridHeight =
      base_config->grid_tile_height() * base_config->preferred_cols();

  {
    SCOPED_TRACE("Horizontal scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(352, 900) /*display_work_area_size*/,
            gfx::Insets(0, 0, 56, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 240.0f / kMinGridWidth, 1);
  }

  {
    SCOPED_TRACE("Vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(480, 500) /*display_work_area_size*/,
            gfx::Insets(0, 0, 40, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 1, 300.0f / kMinGridHeight);
  }

  {
    SCOPED_TRACE("Horizontal and vertical scaling");
    std::unique_ptr<AppListConfig> config =
        AppListConfigProvider::Get().CreateForAppListWidget(
            gfx::Size(352, 500) /*display_work_area_size*/,
            gfx::Insets(0, 0, 40, 0) /*shelf_insets*/, base_config.get());
    VerifyScaledConfig(*base_config, config.get(), 240.0f / kMinGridWidth,
                       300.0f / kMinGridHeight);
  }
}

}  // namespace ash
