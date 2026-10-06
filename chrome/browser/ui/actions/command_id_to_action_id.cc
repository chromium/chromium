// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/command_id_to_action_id.h"

#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"

namespace chrome {

#define MAP_ACTION_E1(action)
#define MAP_ACTION_E2(action, idc) {idc, action},
#define MAP_ACTION_E3(action, idc, scope) {idc, scope::action},
#define MAP_ACTION_E4(action, idc, val, scope) {idc, scope::action},

#define GET_MAP_ACTION_E(_1, _2, _3, _4, macro_name, ...) macro_name
#define E(...)                                                               \
  GET_MAP_ACTION_E(__VA_ARGS__, MAP_ACTION_E4, MAP_ACTION_E3, MAP_ACTION_E2, \
                   MAP_ACTION_E1)(__VA_ARGS__)

const base::flat_map<int, actions::ActionId>& GetCommandIdToActionIdMap() {
  static const base::NoDestructor<base::flat_map<int, actions::ActionId>> kMap(
      [] {
        std::vector<std::pair<int, actions::ActionId>> entries = {
            CHROME_ACTION_IDS SIDE_PANEL_ACTION_IDS TOOLBAR_PINNABLE_ACTION_IDS
                SUBMENU_ACTION_IDS};
        base::flat_map<int, actions::ActionId> map;
        map.reserve(entries.size());
        for (const auto& [idc, action_id] : entries) {
          map.insert({idc, action_id});
        }
        return map;
      }());
  return *kMap;
}

#undef E
#undef GET_MAP_ACTION_E
#undef MAP_ACTION_E1
#undef MAP_ACTION_E2
#undef MAP_ACTION_E3
#undef MAP_ACTION_E4

std::optional<actions::ActionId> GetActionIdForCommandId(int command_id) {
  const auto& map = GetCommandIdToActionIdMap();
  auto it = map.find(command_id);
  return it != map.end() ? std::make_optional(it->second) : std::nullopt;
}

}  // namespace chrome
