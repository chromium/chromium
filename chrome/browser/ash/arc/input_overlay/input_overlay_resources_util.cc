// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"

#include <map>

#include "chrome/browser/ash/arc/input_overlay/actions/action_move_key.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_key.h"
#include "chrome/browser/ash/arc/input_overlay/actions/dependent_position.h"
#include "components/arc/grit/input_overlay_resources.h"
#include "ui/aura/window.h"

namespace arc {
namespace {

// Key strings in Json file about action.
constexpr char kTapAction[] = "tap";
constexpr char kKeyboard[] = "keyboard";
constexpr char kMoveAction[] = "move";

// Key strings in Json file about location.
constexpr char kType[] = "type";
constexpr char kPosition[] = "position";
constexpr char kDependentPosition[] = "dependent_position";

}  // namespace

absl::optional<int> GetInputOverlayResourceId(const std::string& package_name) {
  std::map<std::string, int> resource_id_map = {
      {"org.chromium.arc.testapp.inputoverlay",
       IDR_IO_ORG_CHROMIUM_ARC_TESTAPP_INPUTOVERLAY},
      {"com.blackpanther.ninjaarashi2", IDR_IO_COM_BLACKPANTHER_NINJAARASHI2},
      {"com.habby.archero", IDR_IO_COM_HABBY_ARCHERO},
  };

  auto it = resource_id_map.find(package_name);
  return (it != resource_id_map.end()) ? absl::optional<int>(it->second)
                                       : absl::optional<int>();
}

absl::optional<std::vector<std::unique_ptr<input_overlay::Action>>>
ParseJsonToActions(aura::Window* window, const base::Value& root) {
  std::vector<std::unique_ptr<input_overlay::Action>> actions;

  // Parse tap actions if they exist.
  const base::Value* tap_act_val = root.FindKey(kTapAction);
  if (tap_act_val) {
    const base::Value* keyboard_act_list = tap_act_val->FindListKey(kKeyboard);
    if (keyboard_act_list && keyboard_act_list->is_list()) {
      for (const base::Value& val : keyboard_act_list->GetList()) {
        std::unique_ptr<input_overlay::Action> action =
            std::make_unique<input_overlay::ActionTapKey>(window);
        bool succeed = action->ParseFromJson(val);
        if (succeed)
          actions.emplace_back(std::move(action));
      }
    }
  }

  // Parse move actions if they exist.
  const base::Value* move_act_val = root.FindKey(kMoveAction);
  if (move_act_val) {
    const base::Value* keyboard_act_list = move_act_val->FindListKey(kKeyboard);
    if (keyboard_act_list && keyboard_act_list->is_list()) {
      for (const base::Value& val : keyboard_act_list->GetList()) {
        std::unique_ptr<input_overlay::Action> action =
            std::make_unique<input_overlay::ActionMoveKey>(window);
        bool succeed = action->ParseFromJson(val);
        if (succeed)
          actions.emplace_back(std::move(action));
      }
    }
  }

  // TODO(cuicuiruan): parse more actions.
  return !actions.empty() ? absl::make_optional(std::move(actions))
                          : absl::nullopt;
}

absl::optional<std::vector<std::unique_ptr<input_overlay::Position>>>
ParseLocation(const base::Value& position) {
  std::vector<std::unique_ptr<input_overlay::Position>> positions;
  for (const base::Value& val : position.GetList()) {
    auto* type = val.FindStringKey(kType);
    if (!type) {
      LOG(ERROR) << "There must be position type for each location.";
      return absl::nullopt;
    }
    size_t size = positions.size();
    if (*type == kPosition) {
      positions.emplace_back(std::make_unique<input_overlay::Position>());
    } else if (*type == kDependentPosition) {
      positions.emplace_back(
          std::make_unique<input_overlay::DependentPosition>());
    }

    if (positions.size() == size) {
      LOG(ERROR) << "There is position with unknown type: " << *type;
      return absl::nullopt;
    }

    bool succeed = positions.back()->ParseFromJson(val);
    if (!succeed) {
      LOG(ERROR) << "Position is parsed incorrectly on type: " << *type;
      return absl::nullopt;
    }
  }

  return !positions.empty() ? absl::make_optional(std::move(positions))
                            : absl::nullopt;
}

}  // namespace arc
