// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_GAME_INSTALL_FLOW_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_GAME_INSTALL_FLOW_H_

#include <cstdint>

class Profile;

namespace borealis {

void UserRequestedSteamGameInstall(Profile* profile, uint32_t steam_game_id);

}

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_GAME_INSTALL_FLOW_H_
