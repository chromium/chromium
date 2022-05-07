// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"

#include <map>

#include "components/arc/grit/input_overlay_resources.h"

namespace arc {

absl::optional<int> GetInputOverlayResourceId(const std::string& package_name) {
  std::map<std::string, int> resource_id_map = {
      {"org.chromium.arc.testapp.inputoverlay",
       IDR_IO_ORG_CHROMIUM_ARC_TESTAPP_INPUTOVERLAY},
      {"com.blackpanther.ninjaarashi2", IDR_IO_COM_BLACKPANTHER_NINJAARASHI2},
      {"com.habby.archero", IDR_IO_COM_HABBY_ARCHERO},
      {"com.dts.freefireth", IDR_IO_COM_DTS_FREEFIRETH},
      {"com.fingersoft.hillclimb", IDR_IO_COM_FINGERSOFT_HILLCLIMB},
      {"com.androbaby.game2048", IDR_IO_COM_ANDROBABY_GAME2048},
      {"co.imba.archero", IDR_IO_CO_IMBA_ARCHERO},
      {"com.datavisionstudio.roguelike", IDR_IO_COM_DATAVISIONSTUDIO_ROGUELIKE},
      {"com.blackpanther.ninjaarashi", IDR_IO_COM_BLACKPANTHER_NINJAARASHI},
      {"com.loongcheer.neverlate.wizardlegend.fightmaster",
       IDR_IO_COM_LOONGCHEER_NEVERLATE_WIZARDLEGEND_FIGHTMASTER},
      {"com.direlight.grimvalor", IDR_IO_COM_DIRELIGHT_GRIMVALOR},
      {"com.pixelstar.pbr", IDR_IO_COM_PIXELSTAR_PBR},
  };

  auto it = resource_id_map.find(package_name);
  return (it != resource_id_map.end()) ? absl::optional<int>(it->second)
                                       : absl::optional<int>();
}

}  // namespace arc
