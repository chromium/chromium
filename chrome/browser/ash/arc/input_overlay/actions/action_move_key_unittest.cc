// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_move_key.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace arc {
namespace input_overlay {

class ActionMoveKeyTest : public testing::Test {
 protected:
  ActionMoveKeyTest() = default;
};

constexpr const char kValidJson[] =
    R"json({
      "name": "Virtual Joystick",
      "keys": [
        "KeyW",
        "KeyA",
        "KeyS",
        "KeyD"
      ],
      "location": [
        {
          "type": "position",
          "anchor": [
            0,
            0
          ],
          "anchor_to_target": [
            0.5,
            0.5
          ]
        }
      ]
    })json";

constexpr const char kInValidJsonWrongAmountKeys[] =
    R"json({
      "name": "Virtual Joystick",
      "keys": [
        "KeyW",
        "KeyA",
        "KeyS"
      ],
      "location": [
        {
          "type": "position",
          "anchor": [
            0,
            0
          ],
          "anchor_to_target": [
            0.5,
            0.5
          ]
        }
      ]
    })json";

constexpr const char kInValidJsonDuplicatedKeys[] =
    R"json({
      "name": "Virtual Joystick",
      "keys": [
        "KeyW",
        "KeyW",
        "KeyS",
        "KeyD"
      ],
      "location": [
        {
          "type": "position",
          "anchor": [
            0,
            0
          ],
          "anchor_to_target": [
            0.5,
            0.5
          ]
        }
      ]
    })json";

TEST(ActionMoveKeyTest, TestParseJson) {
  // Parse valid Json.
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  aura::test::TestWindowDelegate dummy_delegate;
  auto window = base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 11, gfx::Rect(200, 400), nullptr));
  std::unique_ptr<ActionMoveKey> action =
      std::make_unique<ActionMoveKey>(window.get());
  EXPECT_TRUE(action->ParseFromJson(json_value.value.value()));
  auto keys = action->keys();
  EXPECT_TRUE(keys.size() == kActionMoveKeysSize);
  EXPECT_TRUE(action->keys()[0] == ui::DomCode::US_W);
  EXPECT_TRUE(action->keys()[1] == ui::DomCode::US_A);
  EXPECT_TRUE(action->keys()[2] == ui::DomCode::US_S);
  EXPECT_TRUE(action->keys()[3] == ui::DomCode::US_D);
  EXPECT_TRUE(action->name() == std::string("Virtual Joystick"));
  action.reset();

  // Parse invalid Json with wrong amount of keys.
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonWrongAmountKeys);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  action = std::make_unique<ActionMoveKey>(window.get());
  EXPECT_FALSE(action->ParseFromJson(json_value.value.value()));
  action.reset();

  // Parse invalid Json with duplicated keys.
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kInValidJsonDuplicatedKeys);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  action = std::make_unique<ActionMoveKey>(window.get());
  EXPECT_FALSE(action->ParseFromJson(json_value.value.value()));
  action.reset();
}

}  // namespace input_overlay
}  // namespace arc
