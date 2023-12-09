// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"

#include <map>

#include "components/arc/grit/input_overlay_resources.h"

namespace arc::input_overlay {

std::optional<int> GetInputOverlayResourceId(const std::string& package_name) {
  std::map<std::string, int> resource_id_map = {
      {"org.chromium.arc.testapp.inputoverlay",
       IDR_IO_ORG_CHROMIUM_ARC_TESTAPP_INPUTOVERLAY},
      {"com.blackpanther.ninjaarashi2", IDR_IO_COM_BLACKPANTHER_NINJAARASHI2},
      {"com.habby.archero", IDR_IO_COM_HABBY_ARCHERO},
      {"com.fingersoft.hillclimb", IDR_IO_COM_FINGERSOFT_HILLCLIMB},
      {"com.androbaby.game2048", IDR_IO_COM_ANDROBABY_GAME2048},
      {"co.imba.archero", IDR_IO_CO_IMBA_ARCHERO},
      {"com.datavisionstudio.roguelike", IDR_IO_COM_DATAVISIONSTUDIO_ROGUELIKE},
      {"com.blackpanther.ninjaarashi", IDR_IO_COM_BLACKPANTHER_NINJAARASHI},
      {"com.loongcheer.neverlate.wizardlegend.fightmaster",
       IDR_IO_COM_LOONGCHEER_NEVERLATE_WIZARDLEGEND_FIGHTMASTER},
      {"com.direlight.grimvalor", IDR_IO_COM_DIRELIGHT_GRIMVALOR},
      {"com.pixelstar.pbr", IDR_IO_COM_PIXELSTAR_PBR},
      {"com.gabrielecirulli.app2048", IDR_IO_COM_GABRIELECIRULLI_APP2048},
      {"com.androbaby.original2048", IDR_IO_COM_ANDROBABY_ORIGINAL2048},
      {"com.estoty.game2048", IDR_IO_COM_ESTOTY_GAME2048},
      {"com.s2apps.game2048", IDR_IO_COM_S2APPS_GAME2048},
      {"com.aldagames.zombero.bullet.hell",
       IDR_IO_COM_ALDAGAMES_ZOMBERO_BULLET_HELL},
      {"games.lightheart.mrautofire", IDR_IO_GAMES_LIGHTHEART_MRAUTOFIRE},
      {"com.yuriychechulin.throwio", IDR_IO_COM_YURIYCHECHULIN_THROWIO},
      {"com.azurgames.stackball", IDR_IO_COM_AZURGAMES_STACKBALL},
      {"com.hiroba.helix", IDR_IO_COM_HIROBA_HELIX},
      {"com.match3blaster.DropStackBallFall",
       IDR_IO_COM_MATCH3BLASTER_DROPSTACKBALLFALL},
      {"com.stack.ball.destroy.wood", IDR_IO_COM_STACK_BALL_DESTROY_WOOD},
      {"com.twist.stackball", IDR_IO_COM_TWIST_STACKBALL},
      {"com.NikSanTech.FireDots3D", IDR_IO_COM_NIKSANTECH_FIREDOTS3D},
      {"com.nama.stackball", IDR_IO_COM_NAMA_STACKBALL},
      {"com.stack.ball.crush", IDR_IO_COM_STACK_BALL_CRUSH},
      {"com.elegant.stack.ball.blast.crush",
       IDR_IO_COM_ELEGANT_STACK_BALL_BLAST_CRUSH},
      {"com.Stellar.StackFall", IDR_IO_COM_STELLAR_STACKFALL},
      {"com.tohsoft.arashi.ninja.shadow",
       IDR_IO_COM_TOHSOFT_ARASHI_NINJA_SHADOW},
      {"com.gamehivecorp.taptitans2", IDR_IO_COM_GAMEHIVECORP_TAPTITANS2},
      {"com.oddrok.powerhover", IDR_IO_COM_ODDROK_POWERHOVER},
      {"com.robtopx.geometryjumplite", IDR_IO_COM_ROBTOPX_GEOMETRYJUMPLITE},
      {"com.robtopx.geometrydashworld", IDR_IO_COM_ROBTOPX_GEOMETRYDASHWORLD},
      {"com.robtopx.geometrydashmeltdown", IDR_IO_COM_ROBTOPX_GEOMETRYDASHMELTDOWN},
      {"com.robtopx.geometrydashsubzero", IDR_IO_COM_ROBTOPX_GEOMETRYDASHSUBZERO},
      {"com.kitkagames.fallbuddies", IDR_IO_COM_KITKAGAMES_FALLBUDDIES},
  };

  auto it = resource_id_map.find(package_name);
  return (it != resource_id_map.end()) ? std::optional<int>(it->second)
                                       : std::optional<int>();
}

}  // namespace arc::input_overlay
