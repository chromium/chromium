// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crosapi/test/crosapi_test_base.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {
namespace {

constexpr char kFakeChromeAppName[] = "fake-chrome-app";

class FileManagerCrosapiTest : public CrosapiTestBase {
 protected:
  void SetUp() override {
    CrosapiTestBase::SetUp();

    app_publisher_ = BindCrosapiInterface(&mojom::Crosapi::BindWebAppPublisher);
    file_manager_ = BindCrosapiInterface(&mojom::Crosapi::BindFileManager);
  }
  const base::FilePath GetMyFilesPath() {
    return GetUserDataDir().Append("test-user").Append("MyFiles");
  }

  mojo::Remote<mojom::AppPublisher> app_publisher_;
  mojo::Remote<mojom::FileManager> file_manager_;
};

TEST_F(FileManagerCrosapiTest, ShowItemInFolder) {
  // A non-existent path.
  const base::FilePath bad_path("/does/not/exist");
  base::test::TestFuture<mojom::OpenResult> future1;
  file_manager_->ShowItemInFolder(bad_path, future1.GetCallback());
  EXPECT_EQ(future1.Get(), mojom::OpenResult::kFailedPathNotFound);

  // A valid folder.
  base::test::TestFuture<mojom::OpenResult> future2;
  file_manager_->ShowItemInFolder(GetMyFilesPath(), future2.GetCallback());
  EXPECT_EQ(future2.Get(), mojom::OpenResult::kSucceeded);

  // A valid file.
  const base::FilePath file_path = GetMyFilesPath().Append("test_file.txt");
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    EXPECT_TRUE(base::WriteFile(file_path, ""));
  }
  base::ScopedClosureRunner scoped_closure_runner(
      base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        EXPECT_TRUE(base::DeleteFile(file_path));
      }));

  base::test::TestFuture<mojom::OpenResult> future3;
  file_manager_->ShowItemInFolder(file_path, future3.GetCallback());
  EXPECT_EQ(future3.Get(), mojom::OpenResult::kSucceeded);
}

TEST_F(FileManagerCrosapiTest, OpenFolder) {
  // A non-existent path.
  const base::FilePath bad_path("/does/not/exist");
  base::test::TestFuture<mojom::OpenResult> future1;
  file_manager_->OpenFolder(bad_path, future1.GetCallback());
  EXPECT_EQ(future1.Get(), mojom::OpenResult::kFailedPathNotFound);

  // A valid folder.
  base::test::TestFuture<mojom::OpenResult> future2;
  file_manager_->OpenFolder(GetMyFilesPath(), future2.GetCallback());
  EXPECT_EQ(future2.Get(), mojom::OpenResult::kSucceeded);

  // A valid file.
  const base::FilePath file_path = GetMyFilesPath().Append("test_file.txt");
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    EXPECT_TRUE(base::WriteFile(file_path, ""));
  }
  base::ScopedClosureRunner scoped_closure_runner(
      base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        EXPECT_TRUE(base::DeleteFile(file_path));
      }));
  base::test::TestFuture<mojom::OpenResult> future3;
  file_manager_->OpenFolder(file_path, future3.GetCallback());
  EXPECT_EQ(future3.Get(), mojom::OpenResult::kFailedInvalidType);
}

TEST_F(FileManagerCrosapiTest, OpenFile) {
  // A non-existent path.
  const base::FilePath bad_path("/does/not/exist");
  base::test::TestFuture<mojom::OpenResult> future1;
  file_manager_->OpenFile(bad_path, future1.GetCallback());
  EXPECT_EQ(future1.Get(), mojom::OpenResult::kFailedPathNotFound);

  // A valid folder.
  base::test::TestFuture<mojom::OpenResult> future2;
  file_manager_->OpenFile(GetMyFilesPath(), future2.GetCallback());
  EXPECT_EQ(future2.Get(), mojom::OpenResult::kFailedInvalidType);
}

TEST_F(FileManagerCrosapiTest, OpenFileWithAppInstalled) {
  const base::FilePath pakfile_path = GetMyFilesPath().Append("test_file.pak");
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    EXPECT_TRUE(base::WriteFile(pakfile_path, ""));
  }
  base::ScopedClosureRunner scoped_closure_runner(
      base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        EXPECT_TRUE(base::DeleteFile(pakfile_path));
      }));
  // A valid file but there is no application to open .pak file.
  base::test::TestFuture<mojom::OpenResult> future1;
  file_manager_->OpenFile(pakfile_path, future1.GetCallback());
  EXPECT_EQ(future1.Get(), mojom::OpenResult::kFailedNoHandlerForFileType);

  // Register a stub AppController here, so that the registered apps will be
  // published to the subscribers. See WebAppServiceAsh::OnApps for more detail.
  mojo::PendingReceiver<crosapi::mojom::AppController> pending_receiver;
  app_publisher_->RegisterAppController(
      pending_receiver.InitWithNewPipeAndPassRemote());

  // Install an application to open .pak file.
  std::vector<apps::AppPtr> apps1;
  apps::AppPtr app =
      std::make_unique<apps::App>(apps::AppType::kWeb, kFakeChromeAppName);

  apps::ConditionValues values;
  values.push_back(std::make_unique<apps::ConditionValue>(
      "pak", apps::PatternMatchType::kFileExtension));

  apps::IntentFilterPtr intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kFile, std::move(values)));

  apps::IntentFilters filters;
  filters.push_back(std::move(intent_filter));

  app->intent_filters = std::move(filters);
  app->readiness = apps::Readiness::kReady;
  app->handles_intents = true;
  apps1.push_back(std::move(app));
  app_publisher_->OnApps(std::move(apps1));

  // A valid .pak file and the app which can handle .pak file exists.
  base::test::TestFuture<mojom::OpenResult> future2;
  file_manager_->OpenFile(pakfile_path, future2.GetCallback());
  EXPECT_EQ(future2.Get(), mojom::OpenResult::kSucceeded);

  // Remove the installed app.
  std::vector<apps::AppPtr> apps2;
  // Uninstall the app before removed.
  apps::AppPtr app_uninstall =
      std::make_unique<apps::App>(apps::AppType::kWeb, kFakeChromeAppName);
  app_uninstall->readiness = apps::Readiness::kUninstalledByUser;
  apps2.push_back(std::move(app_uninstall));

  apps::AppPtr app_remove =
      std::make_unique<apps::App>(apps::AppType::kWeb, kFakeChromeAppName);
  app_remove->readiness = apps::Readiness::kRemoved;
  apps2.push_back(std::move(app_remove));

  app_publisher_->OnApps(std::move(apps2));

  // A valid file but there is no application to open .pak file.
  base::test::TestFuture<mojom::OpenResult> future3;
  file_manager_->OpenFile(pakfile_path, future3.GetCallback());
  EXPECT_EQ(future3.Get(), mojom::OpenResult::kFailedNoHandlerForFileType);
}

}  // namespace
}  // namespace crosapi
