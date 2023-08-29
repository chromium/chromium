// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_fetcher.h"

#include <memory>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace apps {

class GameFetcherTest : public testing::Test {
 public:
  GameFetcherTest() {
    game_fetcher_ = std::make_unique<GameFetcher>(&profile_);
    app_data_ = std::make_unique<proto::AppWithLocaleList>();
  }

  GameFetcher* game_fetcher() { return game_fetcher_.get(); }

 protected:
  base::CallbackListSubscription subscription_;
  std::unique_ptr<proto::AppWithLocaleList> app_data_;

 private:
  std::unique_ptr<GameFetcher> game_fetcher_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GameFetcherTest, RegisterForUpdates) {
  base::RunLoop run_loop;

  bool update_verified = false;
  subscription_ =
      game_fetcher()->RegisterForAppUpdates(base::BindLambdaForTesting(
          [&run_loop, &update_verified](const std::vector<Result>& results) {
            EXPECT_EQ(results.size(), 2u);
            EXPECT_EQ(results[0].GetAppSource(), AppSource::kGames);
            EXPECT_EQ(results[0].GetAppId(), "jrioj324j2095245234320o");
            EXPECT_EQ(results[0].GetAppTitle(), u"CA Name");
            EXPECT_TRUE(results[0].GetSourceExtras());
            auto* game_extras = results[0].GetSourceExtras()->AsGameExtras();
            EXPECT_TRUE(game_extras);
            EXPECT_EQ(game_extras->GetSource(), u"LuckyMe");
            EXPECT_EQ(
                game_extras->GetDeeplinkUrl(),
                GURL("https://todo.com/games?game-id=jrioj324j2095245234320o"));

            EXPECT_EQ(results[1].GetAppSource(), AppSource::kGames);
            EXPECT_EQ(results[1].GetAppId(), "reijarowaiore131983u12jkljs893");
            // This result doesn't have an app title in the specified language,
            // so we are defaulting to the en-US app title.
            EXPECT_EQ(results[1].GetAppTitle(), u"14 days");
            EXPECT_TRUE(results[1].GetSourceExtras());
            game_extras = results[1].GetSourceExtras()->AsGameExtras();
            EXPECT_TRUE(game_extras);
            EXPECT_EQ(game_extras->GetSource(), u"LuckyMe");
            EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                      GURL("https://todo.com/"
                           "games?game-id=reijarowaiore131983u12jkljs893"));
            update_verified = true;
            run_loop.Quit();
          }));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        base::FilePath path;
        EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
        path = path.AppendASCII(
            "app_discovery_service/binary_test_data.textproto");

        std::string app_file_text;
        if (!base::ReadFileToString(path, &app_file_text)) {
          LOG(ERROR) << "Could not read " << app_file_text;
          return;
        }

        if (!app_data_->ParseFromString(app_file_text)) {
          LOG(ERROR) << "Failed to parse protobuf";
          return;
        }

        game_fetcher()->SetLocaleForTesting("CA", "en-CA");
        game_fetcher()->SetResultsForTesting(*app_data_.get());
      }));

  run_loop.Run();
  EXPECT_TRUE(update_verified);
}

TEST_F(GameFetcherTest, RegisterForUpdatesLocaleWithNoResults) {
  base::RunLoop run_loop;

  bool update_verified = false;
  subscription_ =
      game_fetcher()->RegisterForAppUpdates(base::BindLambdaForTesting(
          [&run_loop, &update_verified](const std::vector<Result>& results) {
            EXPECT_EQ(results.size(), 0u);
            update_verified = true;
            run_loop.Quit();
          }));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        base::FilePath path;
        EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
        path = path.AppendASCII(
            "app_discovery_service/binary_test_data.textproto");

        std::string app_file_text;
        if (!base::ReadFileToString(path, &app_file_text)) {
          LOG(ERROR) << "Could not read " << app_file_text;
          return;
        }

        if (!app_data_->ParseFromString(app_file_text)) {
          LOG(ERROR) << "Failed to parse protobuf";
          return;
        }

        game_fetcher()->SetLocaleForTesting("US", "en-US");
        game_fetcher()->SetResultsForTesting(*app_data_.get());
      }));

  run_loop.Run();
  EXPECT_TRUE(update_verified);
}

}  // namespace apps
