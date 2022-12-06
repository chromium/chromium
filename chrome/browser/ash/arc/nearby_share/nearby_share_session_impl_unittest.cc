// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/nearby_share_session_impl.h"

#include <memory>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/app_types.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_mounter.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"

namespace arc {

namespace {
constexpr uint32_t kTaskId = 100;
}

class NearbyShareSessionImplTest : public testing::Test {
 public:
  // Create a NearbyShareSessionImpl for sharing |share_info|. The share will
  // not commence until an ARC window is visible.
  NearbyShareSessionImpl* MakeSession(mojom::ShareIntentInfoPtr share_info) {
    shelf_model_ = std::make_unique<ash::ShelfModel>();
    shelf_controller_ = std::make_unique<ChromeShelfController>(
        &profile_, shelf_model_.get(), nullptr);
    shelf_controller_->Init();

    session_ = std::make_unique<NearbyShareSessionImpl>(
        &profile_, kTaskId, std::move(share_info),
        mojo::PendingRemote<mojom::NearbyShareSessionInstance>(),
        host_remote_.InitWithNewPipeAndPassReceiver(),
        /*session_finished_callback=*/base::BindOnce([](uint32_t) {}));
    return session_.get();
  }

  void ShowArcWindow() {
    window_ =
        base::WrapUnique(aura::test::CreateTestWindowWithId(kTaskId, nullptr));
    exo::SetShellApplicationId(
        window_.get(), "org.chromium.arc." + base::NumberToString(kTaskId));
    window_->SetProperty(aura::client::kAppType,
                         static_cast<int>(ash::AppType::ARC_APP));
    session_->OnWindowVisibilityChanged(window_.get(), /*visible=*/true);
  }

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  std::unique_ptr<ash::ShelfModel> shelf_model_;
  std::unique_ptr<ChromeShelfController> shelf_controller_;
  mojo::PendingRemote<mojom::NearbyShareSessionHost> host_remote_;
  std::unique_ptr<NearbyShareSessionImpl> session_;
  std::unique_ptr<aura::Window> window_;
};

TEST_F(NearbyShareSessionImplTest, TextShareIntent) {
  mojom::ShareIntentInfoPtr share_info = mojom::ShareIntentInfo::New();
  share_info->title = "Test share title";
  base::flat_map<std::string, std::string> extras = {
      {"android.intent.extra.TEXT", "Test share content"}};
  share_info->extras = std::move(extras);
  auto* session = MakeSession(std::move(share_info));

  base::RunLoop run_loop;
  apps::IntentPtr shared_intent;
  session->SetSharesheetCallbackForTesting(base::BindLambdaForTesting(
      [&](gfx::NativeWindow native_window, apps::IntentPtr intent,
          sharesheet::LaunchSource source,
          sharesheet::DeliveredCallback delivered_callback,
          sharesheet::CloseCallback close_callback,
          sharesheet::ActionCleanupCallback cleanup_callback) {
        run_loop.Quit();
        shared_intent = std::move(intent);
      }));
  ShowArcWindow();
  run_loop.Run();

  ASSERT_EQ(shared_intent->share_title, "Test share title");
  ASSERT_EQ(shared_intent->share_text, "Test share content");
}

class NearbyShareSessionImplFuseBoxTest : public NearbyShareSessionImplTest {
  void SetUp() override {
    NearbyShareSessionImplTest::SetUp();

    // Set up ArcFileSystemMounter, which is necessary for decoding ArcContent
    // FileSystemURLs.
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_service_manager_->set_browser_context(profile());
    EXPECT_TRUE(arc::ArcFileSystemMounter::GetForBrowserContext(profile()));
    fusebox_server_ = std::make_unique<fusebox::Server>(/*delegate=*/nullptr);
  }

 private:
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<fusebox::Server> fusebox_server_;

  base::test::ScopedFeatureList feature_list_{
      arc::kEnableArcNearbyShareFuseBox};
};

TEST_F(NearbyShareSessionImplFuseBoxTest, FileIntent) {
  mojom::ShareIntentInfoPtr share_info = mojom::ShareIntentInfo::New();

  std::vector<mojom::FileInfoPtr> files;
  files.emplace_back(absl::in_place,
                     GURL("content://com.example/provider/file.jpg"),
                     "image/jpeg", "file.jpg", 100);
  files.emplace_back(absl::in_place,
                     GURL("content://com.example/provider/some_opaque_name"),
                     "text/plain", "test.txt", 100);
  share_info->files = std::move(files);
  auto* session = MakeSession(std::move(share_info));

  base::RunLoop run_loop;
  apps::IntentPtr shared_intent;
  session->SetSharesheetCallbackForTesting(base::BindLambdaForTesting(
      [&](gfx::NativeWindow native_window, apps::IntentPtr intent,
          sharesheet::LaunchSource source,
          sharesheet::DeliveredCallback delivered_callback,
          sharesheet::CloseCallback close_callback,
          sharesheet::ActionCleanupCallback cleanup_callback) {
        run_loop.Quit();
        shared_intent = std::move(intent);
      }));
  ShowArcWindow();
  run_loop.Run();

  ASSERT_EQ(shared_intent->files.size(), 2u);

  ASSERT_TRUE(base::StartsWith(shared_intent->files[0]->url.spec(),
                               "file:///media/fuse/fusebox"));
  ASSERT_EQ(shared_intent->files[0]->file_name->path().AsUTF8Unsafe(),
            "file.jpg");
  ASSERT_EQ(shared_intent->files[0]->mime_type, "image/jpeg");

  ASSERT_EQ(shared_intent->files[1]->file_name->path().AsUTF8Unsafe(),
            "test.txt");
  ASSERT_EQ(shared_intent->files[1]->mime_type, "text/plain");
}

}  // namespace arc
