// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/command_id_to_action_id.h"

#include <optional>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"

namespace chrome {

namespace {

#define MAP_ACTION_E1(action)
#define MAP_ACTION_E2(action, idc) {idc, action},
#define MAP_ACTION_E3(action, idc, scope) {idc, scope::action},
#define MAP_ACTION_E4(action, idc, val, scope) {idc, scope::action},

#define GET_MAP_ACTION_E(_1, _2, _3, _4, macro_name, ...) macro_name
#define E(...)                                                               \
  GET_MAP_ACTION_E(__VA_ARGS__, MAP_ACTION_E4, MAP_ACTION_E3, MAP_ACTION_E2, \
                   MAP_ACTION_E1)(__VA_ARGS__)

constexpr auto kCommandIdToActionIdMap =
    base::MakeFixedFlatMap<int, actions::ActionId>(
        {CHROME_ACTION_IDS SIDE_PANEL_ACTION_IDS TOOLBAR_PINNABLE_ACTION_IDS
             SUBMENU_ACTION_IDS});

#undef E
#undef GET_MAP_ACTION_E
#undef MAP_ACTION_E1
#undef MAP_ACTION_E2
#undef MAP_ACTION_E3
#undef MAP_ACTION_E4

}  // namespace

base::span<const std::pair<const int, actions::ActionId>>
GetCommandIdToActionIdMappings() {
  return kCommandIdToActionIdMap;
}

std::optional<actions::ActionId> GetActionIdForCommandId(int command_id) {
  auto it = kCommandIdToActionIdMap.find(command_id);
  return it != kCommandIdToActionIdMap.end() ? std::make_optional(it->second)
                                             : std::nullopt;
}

}  // namespace chrome
