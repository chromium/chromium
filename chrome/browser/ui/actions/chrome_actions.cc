// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_actions.h"

#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "ui/actions/actions.h"

void InitializeChromeActions(actions::ActionManager* manager) {}

// TODO(crbug.com/40285337): Adding temporarily to unblock the side panel team.
// Should be removed/replaced when general solution to add action id mappings is
// implemented.
void InitializeActionIdStringMapping() {
#define MAP_ACTION_IDS_TO_STRINGS
#include "ui/actions/action_id_macros.inc"

  actions::ActionIdMap::AddActionIdToStringMappings(
      base::MakeFlatMap<actions::ActionId, std::string>(
          std::vector<std::pair<actions::ActionId, std::string>>{
              SIDE_PANEL_ACTION_IDS}));

  actions::ActionIdMap::AddActionIdToStringMappings(
      base::MakeFlatMap<actions::ActionId, std::string>(
          std::vector<std::pair<actions::ActionId, std::string>>{
              TOOLBAR_PINNABLE_ACTION_IDS}));

#include "ui/actions/action_id_macros.inc"
#undef MAP_ACTION_IDS_TO_STRINGS

#define MAP_STRING_TO_ACTION_IDS
#include "ui/actions/action_id_macros.inc"

  actions::ActionIdMap::AddStringToActionIdMappings(
      base::MakeFlatMap<std::string, actions::ActionId>(
          std::vector<std::pair<std::string, actions::ActionId>>{
              SIDE_PANEL_ACTION_IDS}));

  actions::ActionIdMap::AddStringToActionIdMappings(
      base::MakeFlatMap<std::string, actions::ActionId>(
          std::vector<std::pair<std::string, actions::ActionId>>{
              TOOLBAR_PINNABLE_ACTION_IDS}));

#include "ui/actions/action_id_macros.inc"
#undef MAP_STRING_TO_ACTION_IDS
}
