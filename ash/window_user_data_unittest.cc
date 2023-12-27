// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/window_user_data.h"

#include <memory>

#include "ash/public/cpp/autotest_private_api_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"

namespace ash {
namespace {

// Class that sets a bool* to true from the destructor. Used to track
// destruction.
class Data {
 public:
  explicit Data(bool* delete_setter) : delete_setter_(delete_setter) {}

  Data(const Data&) = delete;
  Data& operator=(const Data&) = delete;

  ~Data() { *delete_setter_ = true; }

 private:
  raw_ptr<bool> delete_setter_;
};

}  // namespace

using WindowUserDataTest = AshTestBase;

// Verifies clear() deletes the data associated with a window.
TEST_F(WindowUserDataTest, ClearDestroys) {
  WindowUserData<Data> user_data;
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  bool data_deleted = false;
  user_data.Set(window.get(), std::make_unique<Data>(&data_deleted));
  EXPECT_FALSE(data_deleted);
  user_data.clear();
  EXPECT_TRUE(data_deleted);
}

// Verifies Set() called with an existing window replaces the existing data.
TEST_F(WindowUserDataTest, ReplaceDestroys) {
  WindowUserData<Data> user_data;
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  bool data1_deleted = false;
  user_data.Set(window.get(), std::make_unique<Data>(&data1_deleted));
  EXPECT_FALSE(data1_deleted);
  bool data2_deleted = false;
  user_data.Set(window.get(), std::make_unique<Data>(&data2_deleted));
  EXPECT_TRUE(data1_deleted);
  EXPECT_FALSE(data2_deleted);
  ASSERT_EQ(1u, user_data.GetWindows().size());
  EXPECT_EQ(window.get(), *user_data.GetWindows().begin());
  window.reset();
  EXPECT_TRUE(data2_deleted);
  EXPECT_TRUE(user_data.GetWindows().empty());
}

// Verifies Set() with null deletes existing data.
TEST_F(WindowUserDataTest, NullClears) {
  WindowUserData<Data> user_data;
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  bool data1_deleted = false;
  user_data.Set(window.get(), std::make_unique<Data>(&data1_deleted));
  EXPECT_FALSE(data1_deleted);
  user_data.Set(window.get(), nullptr);
  EXPECT_TRUE(data1_deleted);
  EXPECT_TRUE(user_data.GetWindows().empty());
}

}  // namespace ash
