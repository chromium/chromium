// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/icon_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
namespace notifications {
namespace {

class MockIconConverter : public IconConverter {
 public:
  MockIconConverter() = default;
  MockIconConverter(const MockIconConverter&) = delete;
  MockIconConverter& operator=(const MockIconConverter&) = delete;
  MOCK_METHOD2(ConvertIconToString,
               void(std::vector<SkBitmap>, IconConverter::EncodeCallback));
  MOCK_METHOD2(ConvertStringToIcon,
               void(std::vector<std::string>, IconConverter::DecodeCallback));
};

class IconStoreTest : public testing::Test {
 public:
  IconStoreTest()
      : add_result_(false),
        load_result_(false),
        db_(nullptr),
        icon_converter_(nullptr) {}
  IconStoreTest(const IconStoreTest&) = delete;
  IconStoreTest& operator=(const IconStoreTest&) = delete;
  ~IconStoreTest() override = default;

  void SetUp() override {
    auto db =
        std::make_unique<leveldb_proto::test::FakeDB<proto::Icon, IconEntry>>(
            &db_entries_);
    db_ = db.get();
    auto icon_converter = std::make_unique<MockIconConverter>();
    icon_converter_ = icon_converter.get();
    store_ = std::make_unique<IconProtoDbStore>(std::move(db),
                                                std::move(icon_converter));
  }

  void InitDb() {
    store()->InitAndLoadKeys(base::DoNothing());
    db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  }

  IconStore::IconTypeBundleMap CreateIcons() {
    IconStore::IconTypeBundleMap result;
    SkBitmap large_icon;
    large_icon.allocN32Pixels(1, 1);
    IconBundle large_icon_bundle;
    large_icon_bundle.bitmap = std::move(large_icon);
    result.emplace(IconType::kLargeIcon, std::move(large_icon_bundle));
    SkBitmap small_icon;
    small_icon.allocN32Pixels(1, 1);
    IconBundle small_icon_bundle;
    small_icon_bundle.bitmap = std::move(small_icon);
    result.emplace(IconType::kSmallIcon, std::move(small_icon_bundle));
    return result;
  }

  void LoadIcons(std::vector<std::string> keys) {
    EXPECT_CALL(*icon_converter(), ConvertStringToIcon(_, _))
        .WillOnce(Invoke([&](std::vector<std::string> encoded_icons,
                             IconConverter::DecodeCallback callback) {
          std::vector<SkBitmap> icons(encoded_icons.size());
          std::move(callback).Run(
              std::make_unique<DecodeResult>(true, std::move(icons)));
        }));
    store()->LoadIcons(
        std::move(keys),
        base::BindOnce(&IconStoreTest::OnIconsLoaded, base::Unretained(this)));
  }

  void AddIcons(IconStore::IconTypeBundleMap input) {
    auto add_icons_callback =
        base::BindOnce(&IconStoreTest::OnIconsAdded, base::Unretained(this));
    EXPECT_CALL(*icon_converter(), ConvertIconToString(_, _))
        .WillOnce(Invoke([&](std::vector<SkBitmap> icons,
                             IconConverter::EncodeCallback callback) {
          std::vector<std::string> encoded_icons(icons.size());
          std::move(callback).Run(
              std::make_unique<EncodeResult>(true, std::move(encoded_icons)));
        }));
    store()->AddIcons(std::move(input), std::move(add_icons_callback));
  }

  void OnIconsAdded(IconStore::IconTypeUuidMap icons_uuid, bool success) {
    icons_uuid_map_ = std::move(icons_uuid);
    add_result_ = success;
  }

  void OnIconsLoaded(bool success, IconStore::LoadedIconsMap icons) {
    loaded_icons_ = std::move(icons);
    load_result_ = success;
  }

 protected:
  IconStore* store() { return store_.get(); }
  MockIconConverter* icon_converter() { return icon_converter_; }
  leveldb_proto::test::FakeDB<proto::Icon, IconEntry>* db() { return db_; }
  bool load_result() const { return load_result_; }
  bool add_result() const { return add_result_; }
  std::vector<std::string> icons_uuid() const {
    std::vector<std::string> result;
    for (const auto& pair : icons_uuid_map_) {
      result.emplace_back(pair.second);
    }
    return result;
  }
  const IconStore::LoadedIconsMap& loaded_icons() const {
    return loaded_icons_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  bool add_result_;
  bool load_result_;
  IconStore::LoadedIconsMap loaded_icons_;
  IconStore::IconTypeUuidMap icons_uuid_map_;
  std::unique_ptr<IconStore> store_;
  std::map<std::string, proto::Icon> db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::Icon, IconEntry>> db_;
  raw_ptr<MockIconConverter> icon_converter_;
};

TEST_F(IconStoreTest, Init) {
  store()->InitAndLoadKeys(
      base::BindOnce([](bool success, IconStore::LoadedIconKeys keys) {
        EXPECT_TRUE(success);
        EXPECT_NE(keys, nullptr);
      }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  base::RunLoop().RunUntilIdle();
}

TEST_F(IconStoreTest, InitFailed) {
  store()->InitAndLoadKeys(base::BindOnce(
      [](bool success, std::unique_ptr<std::vector<std::string>> keys) {
        EXPECT_FALSE(success);
        EXPECT_EQ(keys, nullptr);
      }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);
  base::RunLoop().RunUntilIdle();
}

TEST_F(IconStoreTest, AddIconsFailed) {
  InitDb();
  auto input = CreateIcons();

  // Save two icons into store.
  AddIcons(input);
  db()->UpdateCallback(false);
  EXPECT_FALSE(add_result());
  EXPECT_EQ(icons_uuid().size(), input.size());
}

TEST_F(IconStoreTest, AddAndLoadIcons) {
  InitDb();
  auto input = CreateIcons();

  // Save two icons into store.
  AddIcons(input);
  db()->UpdateCallback(true);
  EXPECT_TRUE(add_result());
  EXPECT_EQ(icons_uuid().size(), input.size());

  // Load two icons.
  LoadIcons(icons_uuid());
  db()->LoadCallback(true);
  EXPECT_TRUE(load_result());
  EXPECT_EQ(loaded_icons().size(), input.size());
}

TEST_F(IconStoreTest, LoadIconsFailed) {
  InitDb();
  auto input = CreateIcons();

  // Save two icons into store.
  AddIcons(input);
  db()->UpdateCallback(true);
  EXPECT_TRUE(add_result());
  EXPECT_EQ(icons_uuid().size(), input.size());

  // Load two icons.
  store()->LoadIcons(icons_uuid(), base::BindOnce(&IconStoreTest::OnIconsLoaded,
                                                  base::Unretained(this)));
  db()->LoadCallback(false);
  EXPECT_FALSE(load_result());
  EXPECT_TRUE(loaded_icons().empty());
}

TEST_F(IconStoreTest, DeleteIcons) {
  InitDb();
  auto input = CreateIcons();

  // Save two icons into store.
  AddIcons(input);
  db()->UpdateCallback(true);
  EXPECT_TRUE(add_result());
  EXPECT_EQ(icons_uuid().size(), input.size());

  // Delete icons.
  store()->DeleteIcons(
      icons_uuid(), base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  // Load two icons.
  LoadIcons(icons_uuid());
  db()->LoadCallback(true);
  EXPECT_TRUE(load_result());
  EXPECT_TRUE(loaded_icons().empty());
}

TEST_F(IconStoreTest, DeleteIconsFailed) {
  InitDb();
  auto input = CreateIcons();

  // Save two icons into store.
  AddIcons(input);
  db()->UpdateCallback(true);
  EXPECT_TRUE(add_result());
  EXPECT_EQ(icons_uuid().size(), input.size());

  // Delete icons.
  store()->DeleteIcons(icons_uuid(), base::BindOnce([](bool success) {
                         EXPECT_FALSE(success);
                       }));
  db()->UpdateCallback(false);

  // Load two icons.
  LoadIcons(icons_uuid());
  db()->LoadCallback(true);
  EXPECT_TRUE(load_result());
  EXPECT_EQ(icons_uuid().size(), input.size());
}
}  // namespace
}  // namespace notifications
