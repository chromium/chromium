// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_game_install_flow.h"

#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "url/gurl.h"

namespace {
constexpr char kSteamStoreUrlPrefix[] = "https://store.steampowered.com/app/";
}

namespace borealis {

void UserRequestedSteamGameInstall(Profile* profile, uint32_t steam_game_id) {
  bool installed = borealis::BorealisServiceFactory::GetForProfile(profile)
                       ->Features()
                       .IsEnabled();
  if (!installed) {
    borealis::BorealisServiceFactory::GetForProfile(profile)
        ->AppLauncher()
        .Launch(borealis::kClientAppId,
                borealis::BorealisLaunchSource::kUnifiedAppInstaller,
                base::DoNothing());
    return;
  }

  // Launch the game's Steam Store page in the Chrome browser.
  //
  // Aside: Since we know Borealis is installed, why not start the VM and
  // kick off the game install in the Steam client?
  //
  // * The user likely doesn't own the game yet, so we need to drop them
  //   into the Steam store page to give them the chance to buy it first.
  //
  // * We can't reliably open the Steam client's store page, because the
  //   user may not have logged in yet. They might not even have a Steam
  //   account.
  //
  // * Opening the Steam website in the browser is reliable. The website
  //   itself already handles the signup/login -> purchase -> install flow.
  //   After buying a game, users will be prompted to install the game,
  //   which leads to a steam:// URL. We handle those URLs and bring up the
  //   Steam client's game install dialog in Borealis, completing the flow.
  std::string url =
      base::StrCat({kSteamStoreUrlPrefix, base::NumberToString(steam_game_id)});

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(url), ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

}  // namespace borealis
