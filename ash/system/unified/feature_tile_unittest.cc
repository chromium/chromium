// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

namespace {

class MockFeaturePodController : public FeaturePodControllerBase {
 public:
  MockFeaturePodController() = default;
  explicit MockFeaturePodController(bool togglable) : togglable_(togglable) {}

  MockFeaturePodController(const MockFeaturePodController&) = delete;
  MockFeaturePodController& operator=(const MockFeaturePodController&) = delete;

  ~MockFeaturePodController() override = default;

  FeaturePodButton* CreateButton() override {
    return new FeaturePodButton(/*controller=*/this);
  }

  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override {
    auto tile = std::make_unique<FeatureTile>(
        base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                            weak_ptr_factory_.GetWeakPtr()),
        togglable_,
        compact ? FeatureTile::TileType::kCompact
                : FeatureTile::TileType::kPrimary);
    tile->SetVectorIcon(vector_icons::kDogfoodIcon);
    tile_ = tile.get();
    return tile;
  }

  QsFeatureCatalogName GetCatalogName() override {
    return QsFeatureCatalogName::kUnknown;
  }

  void OnIconPressed() override {
    was_icon_pressed_ = true;
    // FeaturePodController elements in production know if they are togglable,
    // but in this mock we need to check before changing the toggled state.
    if (togglable_) {
      toggled_ = !toggled_;
      tile_->SetToggled(toggled_);
    }
  }

  void OnLabelPressed() override {
    // If button is not of type: Toggle + Drill-in, `OnLabelPressed` resolves to
    // `OnIconPressed`.
    if (!togglable_) {
      was_icon_pressed_ = true;
      return;
    }
    was_label_pressed_ = true;
  }

  // FeaturePodController elements in production know if they need to create a
  // drill-in button, but here we create it after creating the base button.
  void CreateDrillInButton() {
    tile_->CreateDrillInButton(
        base::BindRepeating(&FeaturePodControllerBase::OnLabelPressed,
                            weak_ptr_factory_.GetWeakPtr()),
        u"Tooltip text");
  }

  bool WasIconPressed() { return was_icon_pressed_; }
  bool WasLabelPressed() { return was_label_pressed_; }

 private:
  FeatureTile* tile_ = nullptr;
  bool was_icon_pressed_ = false;
  bool was_label_pressed_ = false;
  bool togglable_ = false;
  bool toggled_ = false;

  base::WeakPtrFactory<MockFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace

class FeatureTileTest : public AshTestBase {
 public:
  FeatureTileTest() { feature_list_.InitAndEnableFeature(features::kQsRevamp); }

  FeatureTileTest(const FeatureTileTest&) = delete;
  FeatureTileTest& operator=(const FeatureTileTest&) = delete;
  ~FeatureTileTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FeatureTileTest, PrimaryTile_LaunchSurface) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/false);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  EXPECT_FALSE(tile->drill_in_button());

  // Ensure icon hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasIconPressed());

  LeftClickOn(tile);

  // Ensure icon was pressed and button does not toggle after clicking it.
  EXPECT_TRUE(mock_controller->WasIconPressed());
  EXPECT_FALSE(tile->IsToggled());
}

TEST_F(FeatureTileTest, PrimaryTile_Toggle) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  EXPECT_FALSE(tile->drill_in_button());

  // Ensure icon hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasIconPressed());

  LeftClickOn(tile);

  // Ensure icon was pressed and button toggles after clicking it.
  EXPECT_TRUE(mock_controller->WasIconPressed());
  EXPECT_TRUE(tile->IsToggled());

  LeftClickOn(tile);

  // Ensure button toggles after clicking it again.
  EXPECT_FALSE(tile->IsToggled());
}

TEST_F(FeatureTileTest, PrimaryTile_DrillIn) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/false);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  mock_controller->CreateDrillInButton();
  views::test::RunScheduledLayout(tile);
  ASSERT_TRUE(tile->drill_in_button());
  EXPECT_TRUE(tile->drill_in_button()->GetVisible());

  // Ensure icon hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasIconPressed());

  LeftClickOn(tile->drill_in_button());

  // Ensure icon was pressed and button does not toggle after clicking it.
  EXPECT_TRUE(mock_controller->WasIconPressed());
  EXPECT_FALSE(tile->IsToggled());

  // TODO(b/259280052): Ensure drill-in button does not focus.
}

TEST_F(FeatureTileTest, PrimaryTile_ToggleWithDrillIn) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  mock_controller->CreateDrillInButton();
  views::test::RunScheduledLayout(tile);
  ASSERT_TRUE(tile->drill_in_button());
  EXPECT_TRUE(tile->drill_in_button()->GetVisible());

  // Ensure icon is not pressed or toggled.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasIconPressed());

  LeftClickOn(tile);

  // Ensure icon was pressed and button toggles after clicking it.
  EXPECT_TRUE(mock_controller->WasIconPressed());
  EXPECT_TRUE(tile->IsToggled());

  EXPECT_FALSE(mock_controller->WasLabelPressed());

  LeftClickOn(tile->drill_in_button());

  // Ensure `WasLabelPressed` after clicking drill-in button.
  EXPECT_TRUE(mock_controller->WasLabelPressed());

  // TODO(b/259280052): Ensure drill-in button has focus.
}

TEST_F(FeatureTileTest, CompactTile_LaunchSurface) {
  auto mock_controller = std::make_unique<MockFeaturePodController>(
      /*togglable=*/false);
  auto* tile =
      widget_->SetContentsView(mock_controller->CreateTile(/*compact=*/true));

  // Ensure icon hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasIconPressed());

  LeftClickOn(tile);

  // Ensure icon was pressed and button does not toggle after clicking it.
  EXPECT_TRUE(mock_controller->WasIconPressed());
  EXPECT_FALSE(tile->IsToggled());
}

TEST_F(FeatureTileTest, CompactTile_Toggle) {
  auto mock_controller = std::make_unique<MockFeaturePodController>(
      /*togglable=*/true);
  auto* tile =
      widget_->SetContentsView(mock_controller->CreateTile(/*compact=*/true));

  // Ensure icon hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasIconPressed());

  LeftClickOn(tile);

  // Ensure icon was pressed and button toggles after clicking it.
  EXPECT_TRUE(mock_controller->WasIconPressed());
  EXPECT_TRUE(tile->IsToggled());

  // Ensure button toggles after clicking it again.
  LeftClickOn(tile);
  EXPECT_FALSE(tile->IsToggled());
}

}  // namespace ash
