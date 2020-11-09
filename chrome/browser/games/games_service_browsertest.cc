// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/games/games_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/games_service_impl.h"
#include "components/games/core/games_utils.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"
#include "components/games/core/test/test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace games {

class GamesServiceBrowserTest : public PlatformBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    PlatformBrowserTest::SetUp();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void WriteCatalog(const GamesCatalog& catalog) {
    WriteStringToFile(GetGamesCatalogPath(install_dir()),
                      catalog.SerializeAsString());
  }

  void WriteHighlightedGamesResponse(const HighlightedGamesResponse& response) {
    WriteStringToFile(GetHighlightedGamesPath(install_dir()),
                      response.SerializeAsString());
  }

  void SetAsCurrentlyHighlighted(HighlightedGame& highlighted_game) {
    // Adding and removing 2 days to reduce risk of daylight savings time change
    // failing these tests flakily.
    auto now = base::Time::Now();
    test::SetDateProtoTo(now - base::TimeDelta::FromDays(2),
                         highlighted_game.mutable_start_date());
    test::SetDateProtoTo(now + base::TimeDelta::FromDays(2),
                         highlighted_game.mutable_end_date());
  }

  void SetPref() { prefs::SetInstallDirPath(pref_service(), install_dir()); }

  void ExpectGetHighlightedGameFailure(ResponseCode expected_code) {
    ExpectGetHighlightedGame(Game(), expected_code);
  }

  void ExpectGetHighlightedGame(const Game& expected_game,
                                ResponseCode expected_code) {
    base::RunLoop run_loop;
    games_service()->SetHighlightedGameCallback(
        base::BindLambdaForTesting([&expected_game, &expected_code, &run_loop](
                                       ResponseCode code, const Game game) {
          EXPECT_EQ(expected_code, code);
          test::ExpectProtosEqual(expected_game, game);
          run_loop.Quit();
        }));
    games_service()->GenerateHub();
    run_loop.Run();
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }
  GamesService* games_service() {
    return GamesServiceFactory::GetForBrowserContext(browser_context());
  }
  base::FilePath install_dir() { return temp_dir_.GetPath(); }

 private:
  void WriteStringToFile(const base::FilePath& file_path,
                         const std::string& string_data) {
    // Running the blocking IO call on a separate thread to comply to thread
    // restrictions (no blocking calls on the thread executing the tests).
    base::RunLoop run_loop;
    bool write_file_success = false;
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
          write_file_success = base::WriteFile(file_path, string_data);
          run_loop.Quit();
        }));
    run_loop.Run();

    ASSERT_TRUE(write_file_success);
  }

  content::BrowserContext* browser_context() {
    return GetActiveWebContents()->GetBrowserContext();
  }
  Profile* profile() { return Profile::FromBrowserContext(browser_context()); }

  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(GamesServiceBrowserTest,
                       GetHighlightedGame_NoComponent) {
  ExpectGetHighlightedGameFailure(ResponseCode::kComponentNotInstalled);
}

IN_PROC_BROWSER_TEST_F(GamesServiceBrowserTest,
                       GetHighlightedGame_MissingDataFiles) {
  SetPref();

  ExpectGetHighlightedGameFailure(ResponseCode::kFileNotFound);
}

IN_PROC_BROWSER_TEST_F(GamesServiceBrowserTest,
                       GetHighlightedGame_MissingCatalog) {
  WriteHighlightedGamesResponse(test::CreateHighlightedGamesResponse());
  SetPref();

  ExpectGetHighlightedGameFailure(ResponseCode::kFileNotFound);
}

IN_PROC_BROWSER_TEST_F(GamesServiceBrowserTest,
                       GetHighlightedGame_EmptyCatalog) {
  WriteCatalog(GamesCatalog());
  WriteHighlightedGamesResponse(test::CreateHighlightedGamesResponse());
  SetPref();

  ExpectGetHighlightedGameFailure(ResponseCode::kInvalidData);
}

IN_PROC_BROWSER_TEST_F(GamesServiceBrowserTest,
                       GetHighlightedGame_MissingHGResponse) {
  WriteCatalog(test::CreateCatalogWithTwoGames());
  SetPref();

  ExpectGetHighlightedGameFailure(ResponseCode::kFileNotFound);
}

IN_PROC_BROWSER_TEST_F(GamesServiceBrowserTest,
                       GetHighlightedGame_EmptyHGResponse) {
  WriteCatalog(test::CreateCatalogWithTwoGames());
  WriteHighlightedGamesResponse(HighlightedGamesResponse());
  SetPref();

  ExpectGetHighlightedGameFailure(ResponseCode::kInvalidData);
}

IN_PROC_BROWSER_TEST_F(GamesServiceBrowserTest, GetHighlightedGame_Success) {
  auto fake_catalog = test::CreateCatalogWithTwoGames();
  WriteCatalog(fake_catalog);

  auto fake_hg_response = test::CreateHighlightedGamesResponse();
  auto& fake_highlighted_game = fake_hg_response.mutable_games()->at(0);
  SetAsCurrentlyHighlighted(fake_highlighted_game);
  WriteHighlightedGamesResponse(fake_hg_response);

  SetPref();

  const auto fake_game_proto =
      TryFindGameById(fake_highlighted_game.game_id(), fake_catalog).value();

  ExpectGetHighlightedGame(fake_game_proto, ResponseCode::kSuccess);
}

}  // namespace games
