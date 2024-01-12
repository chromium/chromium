// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_GAMES_GAME_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_GAMES_GAME_RESULT_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace apps {
class AppDiscoveryService;
}

namespace gfx {
class ImageSkia;
}

namespace app_list {

// Search result for cloud gaming search.
class GameResult : public ChromeSearchResult, public ash::ColorModeObserver {
 public:
  GameResult(Profile* profile,
             AppListControllerDelegate* list_controller,
             apps::AppDiscoveryService* app_discovery_service,
             const apps::Result& game,
             double relevance,
             const std::u16string& query);
  ~GameResult() override;

  GameResult(const GameResult&) = delete;
  GameResult& operator=(const GameResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  void UpdateText(const apps::Result& game, const std::u16string& query);
  void OnIconLoaded(const gfx::ImageSkia& image, apps::DiscoveryError error);

  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<AppListControllerDelegate> list_controller_;

  GURL launch_url_;
  bool is_icon_masking_allowed_;
  const int dimension_;

  base::WeakPtrFactory<GameResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_GAMES_GAME_RESULT_H_
