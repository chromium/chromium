// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/crosapi/lacros_shelf_item_tracker.h"

#include <memory>
#include <string>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/test/ash_test_base.h"
#include "chrome/browser/ui/ash/shelf/lacros_shelf_item_controller.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/lacros_shelf_item_tracker.mojom.h"
#include "components/exo/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image_skia.h"

namespace crosapi {

namespace {

gfx::ImageSkia CreateGreenSquareIcon() {
  int size_px = 128;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size_px, size_px);
  bitmap.eraseColor(SK_ColorGREEN);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

// This class does nothing.
class TestShelfItemDelegate : public LacrosShelfItemController {
 public:
  explicit TestShelfItemDelegate(const ash::ShelfID& shelf_id)
      : LacrosShelfItemController(shelf_id) {}
  ~TestShelfItemDelegate() override = default;

  void AddWindow(aura::Window* window) override {}
  std::u16string GetTitle() override { return u"test title"; }

  // ShelfItemDelegate:
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

class TestLacrosShelfItemTracker : public LacrosShelfItemTracker {
 public:
  TestLacrosShelfItemTracker() = default;

  void RemoveFromShelf(const ash::ShelfID& shelf_id) override {
    last_removed_shelf_id_ = shelf_id;
    LacrosShelfItemTracker::RemoveFromShelf(shelf_id);
  }

  ash::ShelfItemDelegate* AddOrUpdateShelfItemAndReturnDelegate(
      mojom::WindowDataPtr window_data) override {
    last_updated_shelf_id_ = ash::ShelfID(window_data->item_id);
    return LacrosShelfItemTracker::AddOrUpdateShelfItemAndReturnDelegate(
        std::move(window_data));
  }

  const ash::ShelfID& last_updated_shelf_id() { return last_updated_shelf_id_; }
  const ash::ShelfID& last_removed_shelf_id() { return last_removed_shelf_id_; }

 private:
  std::unique_ptr<ash::ShelfItemDelegate> CreateDelegateByInstanceType(
      const ash::ShelfID& shelf_id,
      mojom::InstanceType instance_type) override {
    return std::make_unique<TestShelfItemDelegate>(shelf_id);
  }

  ash::ShelfID last_updated_shelf_id_;
  ash::ShelfID last_removed_shelf_id_;
};

class LacrosShelfItemTrackerTest : public ash::AshTestBase {
 public:
  ash::ShelfModel* shelf_model() { return ash::ShelfModel::Get(); }

  std::unique_ptr<aura::Window> CreateAndInitWindowWithId(std::string id) {
    auto window = std::make_unique<aura::Window>(nullptr);
    window->SetProperty(exo::kApplicationIdKey, id);
    CHECK(*(window->GetProperty(exo::kApplicationIdKey)) == id);
    window->Init(ui::LAYER_NOT_DRAWN);
    return window;
  }

  std::string lacros_window_prefix() { return kLacrosAppIdPrefix; }

  void AddOrUpdateWindow(std::string item_id,
                         std::string window_id,
                         gfx::ImageSkia icon) {
    mojom::WindowDataPtr window_data = mojom::WindowData::New(
        item_id, window_id, mojom::InstanceType::kIsolatedWebAppInstaller,
        icon);
    tracker_.AddOrUpdateWindow(std::move(window_data));
  }

  const ash::ShelfID& last_updated_shelf_id() {
    return tracker_.last_updated_shelf_id();
  }
  const ash::ShelfID& last_removed_shelf_id() {
    return tracker_.last_removed_shelf_id();
  }

 private:
  TestLacrosShelfItemTracker tracker_;
};

TEST_F(LacrosShelfItemTrackerTest, SingleShelfItemInitWindowBeforeCrosapi) {
  std::string item_id = "test_item_id";
  std::string window_id = lacros_window_prefix() + "test_window_id";
  gfx::ImageSkia icon = CreateGreenSquareIcon();

  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID());
  ASSERT_EQ(shelf_model()->item_count(), 0);

  auto window = CreateAndInitWindowWithId(window_id);
  AddOrUpdateWindow(item_id, window_id, icon);

  ASSERT_EQ(shelf_model()->item_count(), 1);
  EXPECT_TRUE(shelf_model()->items()[0].image.BackedBySameObjectAs(icon));
  EXPECT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id"));
}

TEST_F(LacrosShelfItemTrackerTest, SingleShelfItemInitWindowAfterCrosapi) {
  std::string item_id = "test_item_id";
  std::string window_id = lacros_window_prefix() + " test_window_id";
  gfx::ImageSkia icon = CreateGreenSquareIcon();

  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID());
  ASSERT_EQ(shelf_model()->item_count(), 0);

  AddOrUpdateWindow(item_id, window_id, icon);
  auto window = CreateAndInitWindowWithId(window_id);

  ASSERT_EQ(shelf_model()->item_count(), 1);
  EXPECT_TRUE(shelf_model()->items()[0].image.BackedBySameObjectAs(icon));
  EXPECT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id"));
}

TEST_F(LacrosShelfItemTrackerTest, IgnoresNotLacrosWindow) {
  std::string item_id = "test_item_id";
  std::string window_id = " test_window_id";
  gfx::ImageSkia icon = CreateGreenSquareIcon();

  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID());
  ASSERT_EQ(shelf_model()->item_count(), 0);

  AddOrUpdateWindow(item_id, window_id, icon);
  auto window = CreateAndInitWindowWithId(window_id);

  ASSERT_EQ(shelf_model()->item_count(), 0);
  EXPECT_EQ(last_updated_shelf_id(), ash::ShelfID());
}

TEST_F(LacrosShelfItemTrackerTest, SingleShelfItemUpdateIcon) {
  std::string item_id = "test_item_id";
  std::string window_id = lacros_window_prefix() + " test_window_id";
  gfx::ImageSkia icon = CreateGreenSquareIcon();

  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID());
  ASSERT_EQ(shelf_model()->item_count(), 0);

  auto window = CreateAndInitWindowWithId(window_id);
  AddOrUpdateWindow(item_id, window_id, gfx::ImageSkia());

  ASSERT_EQ(shelf_model()->item_count(), 1);
  EXPECT_TRUE(shelf_model()->items()[0].image.isNull());

  AddOrUpdateWindow(item_id, window_id, icon);
  ASSERT_EQ(shelf_model()->item_count(), 1);
  EXPECT_TRUE(shelf_model()->items()[0].image.BackedBySameObjectAs(icon));
}

TEST_F(LacrosShelfItemTrackerTest, SingleShelfItemMultipleWindow) {
  std::string item_id = "test_item_id";
  gfx::ImageSkia icon = CreateGreenSquareIcon();

  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID());
  ASSERT_EQ(shelf_model()->item_count(), 0);

  std::string window_id_0 = lacros_window_prefix() + " test_window_id_0";
  auto window_0 = CreateAndInitWindowWithId(window_id_0);
  AddOrUpdateWindow(item_id, window_id_0, icon);

  ASSERT_EQ(shelf_model()->item_count(), 1);
  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id"));

  std::string window_id_1 = lacros_window_prefix() + " test_window_id_1";
  auto window_1 = CreateAndInitWindowWithId(window_id_1);
  AddOrUpdateWindow(item_id, window_id_1, icon);

  EXPECT_EQ(shelf_model()->item_count(), 1);
  EXPECT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id"));

  std::string window_id_2 = lacros_window_prefix() + " test_window_id_2";
  auto window_2 = CreateAndInitWindowWithId(window_id_2);
  AddOrUpdateWindow(item_id, window_id_2, icon);

  EXPECT_EQ(shelf_model()->item_count(), 1);
  EXPECT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id"));
}

TEST_F(LacrosShelfItemTrackerTest, MultipleShelfItem) {
  gfx::ImageSkia icon = CreateGreenSquareIcon();

  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID());
  ASSERT_EQ(shelf_model()->item_count(), 0);

  std::string item_id_0 = "test_item_id_0";
  std::string window_id_0 = lacros_window_prefix() + " test_window_id_0";
  auto window_0 = CreateAndInitWindowWithId(window_id_0);
  AddOrUpdateWindow(item_id_0, window_id_0, icon);

  ASSERT_EQ(shelf_model()->item_count(), 1);
  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id_0"));

  std::string item_id_1 = "test_item_id_1";
  std::string window_id_1 = lacros_window_prefix() + " test_window_id_1";
  auto window_1 = CreateAndInitWindowWithId(window_id_1);
  AddOrUpdateWindow(item_id_1, window_id_1, icon);

  EXPECT_EQ(shelf_model()->item_count(), 2);
  EXPECT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id_1"));

  std::string item_id_2 = "test_item_id_2";
  std::string window_id_2 = lacros_window_prefix() + " test_window_id_2";
  auto window_2 = CreateAndInitWindowWithId(window_id_2);
  AddOrUpdateWindow(item_id_2, window_id_2, icon);

  EXPECT_EQ(shelf_model()->item_count(), 3);
  EXPECT_EQ(last_updated_shelf_id(), ash::ShelfID("test_item_id_2"));
}

TEST_F(LacrosShelfItemTrackerTest, ShelfItemRemovedWhenAllWindowDestoryed) {
  std::string item_id = "test_item_id";
  gfx::ImageSkia icon = CreateGreenSquareIcon();

  ASSERT_EQ(last_updated_shelf_id(), ash::ShelfID());
  ASSERT_EQ(shelf_model()->item_count(), 0);

  std::string window_id_0 = lacros_window_prefix() + " test_window_id_0";
  auto window_0 = CreateAndInitWindowWithId(window_id_0);
  AddOrUpdateWindow(item_id, window_id_0, icon);

  ASSERT_EQ(shelf_model()->item_count(), 1);

  std::string window_id_1 = lacros_window_prefix() + " test_window_id_1";
  auto window_1 = CreateAndInitWindowWithId(window_id_1);
  AddOrUpdateWindow(item_id, window_id_1, icon);

  EXPECT_EQ(shelf_model()->item_count(), 1);

  std::string window_id_2 = lacros_window_prefix() + " test_window_id_2";
  auto window_2 = CreateAndInitWindowWithId(window_id_2);
  AddOrUpdateWindow(item_id, window_id_2, icon);

  EXPECT_EQ(shelf_model()->item_count(), 1);

  window_2.reset();
  EXPECT_EQ(shelf_model()->item_count(), 1);

  window_0.reset();
  EXPECT_EQ(shelf_model()->item_count(), 1);

  window_1.reset();
  EXPECT_EQ(shelf_model()->item_count(), 0);
}

}  // namespace crosapi
