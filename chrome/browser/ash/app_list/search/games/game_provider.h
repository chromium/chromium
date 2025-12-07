// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_GAMES_GAME_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_GAMES_GAME_PROVIDER_H_

#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ui/ash/thumbnail_loader/thumbnail_loader.h"

class AppListControllerDelegate;
class Profile;

namespace apps {

class AppDiscoveryService;
enum class DiscoveryError;

}  // namespace apps

namespace app_list {

// Provider for cloud gaming search.
class GameProvider : public SearchProvider {
 public:
  using GameIndex = std::vector<apps::Result>;

  GameProvider(Profile* profile, AppListControllerDelegate* list_controller);
  ~GameProvider() override;

  GameProvider(const GameProvider&) = delete;
  GameProvider& operator=(const GameProvider&) = delete;

  // SearchProvider:
  ash::AppListSearchResultType ResultType() const override;
  void Start(const std::u16string& query) override;
  void StopQuery() override;

  void SetGameIndexForTest(GameIndex game_index);

 private:
  void UpdateIndex();
  void OnIndexUpdated(const GameIndex& index, apps::DiscoveryError error);
  void OnIndexUpdatedBySubscription(const GameIndex& index);
  void OnSearchComplete(
      std::u16string query,
      std::vector<std::pair<const apps::Result*, double>> matches);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const raw_ptr<AppListControllerDelegate> list_controller_;
  const raw_ptr<apps::AppDiscoveryService, DanglingUntriaged>
      app_discovery_service_;

  GameIndex game_index_;
  base::CallbackListSubscription subscription_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<GameProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_GAMES_GAME_PROVIDER_H_
