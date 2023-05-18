// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller_impl.h"

#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "ash/wallpaper/test_wallpaper_drivefs_delegate.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_blur_manager.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time_override.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

using session_manager::SessionState;

namespace ash {
namespace {

// Containers IDs used for tests.
constexpr int kWallpaperId = kShellWindowId_WallpaperContainer;
constexpr int kLockScreenWallpaperId =
    kShellWindowId_LockScreenWallpaperContainer;
constexpr int kAlwaysOnTopWallpaperId =
    kShellWindowId_AlwaysOnTopWallpaperContainer;

constexpr char kDefaultSmallWallpaperName[] = "small.jpg";
constexpr char kDefaultLargeWallpaperName[] = "large.jpg";
constexpr char kGuestSmallWallpaperName[] = "guest_small.jpg";
constexpr char kGuestLargeWallpaperName[] = "guest_large.jpg";
constexpr char kChildSmallWallpaperName[] = "child_small.jpg";
constexpr char kChildLargeWallpaperName[] = "child_large.jpg";

constexpr char kCustomizationSmallWallpaperName[] = "small_customization.jpeg";
constexpr char kCustomizationLargeWallpaperName[] = "large_customization.jpeg";

// Colors used to distinguish between wallpapers with large and small
// resolution.
constexpr SkColor kLargeCustomWallpaperColor = SK_ColorDKGRAY;
constexpr SkColor kSmallCustomWallpaperColor = SK_ColorLTGRAY;

// A color that can be passed to |CreateImage|. Specifically chosen to not
// conflict with any of the custom wallpaper colors.
constexpr SkColor kWallpaperColor = SK_ColorMAGENTA;

std::string GetDummyFileId(const AccountId& account_id) {
  return account_id.GetUserEmail() + "-hash";
}

std::string GetDummyFileName(const AccountId& account_id) {
  return account_id.GetUserEmail() + "-file";
}

constexpr char kUser1[] = "user1@test.com";
const AccountId kAccountId1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);
const std::string kWallpaperFilesId1 = GetDummyFileId(kAccountId1);
const std::string kFileName1 = GetDummyFileName(kAccountId1);

constexpr char kUser2[] = "user2@test.com";
const AccountId kAccountId2 = AccountId::FromUserEmailGaiaId(kUser2, kUser2);
const std::string kWallpaperFilesId2 = GetDummyFileId(kAccountId2);
const std::string kFileName2 = GetDummyFileName(kAccountId2);

constexpr char kChildEmail[] = "child@test.com";

constexpr char kDummyUrl[] = "https://best_wallpaper/1";
constexpr char kDummyUrl2[] = "https://best_wallpaper/2";
constexpr char kDummyUrl3[] = "https://best_wallpaper/3";
constexpr char kDummyUrl4[] = "https://best_wallpaper/4";

const uint64_t kAssetId = 1;
const uint64_t kAssetId2 = 2;
const uint64_t kAssetId3 = 3;
const uint64_t kAssetId4 = 4;
const uint64_t kUnitId = 1;
const uint64_t kUnitId2 = 2;

const std::string kFakeGooglePhotosAlbumId = "fake_album";
const std::string kFakeGooglePhotosPhotoId = "fake_photo";

// Creates an image of size |size|.
gfx::ImageSkia CreateImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

// Returns number of child windows in a shell window container.
int ChildCountForContainer(int container_id) {
  aura::Window* root = Shell::Get()->GetPrimaryRootWindow();
  aura::Window* container = root->GetChildById(container_id);
  return static_cast<int>(container->children().size());
}

// Steps a layer animation until it is completed. Animations must be enabled.
void RunAnimationForLayer(ui::Layer* layer) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    layer->GetAnimator()->Step(step_time + base::Milliseconds(1000));
  }
}

// Writes a JPEG image of the specified size and color to |path|. Returns true
// on success.
bool WriteJPEGFile(const base::FilePath& path,
                   int width,
                   int height,
                   SkColor color) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  std::vector<unsigned char> output;
  if (!gfx::JPEGCodec::Encode(bitmap, 80 /*quality*/, &output)) {
    LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
    return false;
  }

  if (!base::WriteFile(path, output)) {
    LOG(ERROR) << "Writing to " << path.value() << " failed.";
    return false;
  }
  return true;
}

// Returns custom wallpaper path. Creates the directory if it doesn't exist.
base::FilePath GetCustomWallpaperPath(const char* sub_dir,
                                      const std::string& wallpaper_files_id,
                                      const std::string& file_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath wallpaper_path =
      WallpaperControllerImpl::GetCustomWallpaperPath(
          sub_dir, wallpaper_files_id, file_name);
  if (!base::DirectoryExists(wallpaper_path.DirName()))
    base::CreateDirectory(wallpaper_path.DirName());

  return wallpaper_path;
}

void WaitUntilCustomWallpapersDeleted(const AccountId& account_id) {
  const std::string wallpaper_file_id = GetDummyFileId(account_id);

  base::FilePath small_wallpaper_dir =
      WallpaperControllerImpl::GetCustomWallpaperDir(
          WallpaperControllerImpl::kSmallWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath large_wallpaper_dir =
      WallpaperControllerImpl::GetCustomWallpaperDir(
          WallpaperControllerImpl::kLargeWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath original_wallpaper_dir =
      WallpaperControllerImpl::GetCustomWallpaperDir(
          WallpaperControllerImpl::kOriginalWallpaperSubDir)
          .Append(wallpaper_file_id);

  while (base::PathExists(small_wallpaper_dir) ||
         base::PathExists(large_wallpaper_dir) ||
         base::PathExists(original_wallpaper_dir)) {
  }
}

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}

  TaskObserver(const TaskObserver&) = delete;
  TaskObserver& operator=(const TaskObserver&) = delete;

  ~TaskObserver() override = default;

  // TaskObserver:
  void WillProcessTask(const base::PendingTask& /* pending_task */,
                       bool /* was_blocked_or_low_priority */) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    processed_ = true;
  }

  // Returns true if any task was processed.
  bool processed() const { return processed_; }

 private:
  bool processed_;
};

// See content::RunAllTasksUntilIdle().
void RunAllTasksUntilIdle() {
  while (true) {
    TaskObserver task_observer;
    base::CurrentThread::Get()->AddTaskObserver(&task_observer);
    // May spin message loop.
    base::ThreadPoolInstance::Get()->FlushForTesting();

    base::RunLoop().RunUntilIdle();
    base::CurrentThread::Get()->RemoveTaskObserver(&task_observer);

    if (!task_observer.processed())
      break;
  }
}

PrefService* GetProfilePrefService(const AccountId& account_id) {
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

WallpaperInfo InfoWithType(WallpaperType type) {
  WallpaperInfo info(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, type,
                     base::Time::Now());
  if (IsOnlineWallpaper(type)) {
    // Daily and Online types require asset id and collection id.
    info.unit_id = 1234;
    info.collection_id = "placeholder collection";
    info.location = "https://example.com/example.jpeg";
  }
  if (type == WallpaperType::kOnceGooglePhotos)
    info.dedup_key = "dedup_key";
  return info;
}

base::Time DayBeforeYesterdayish() {
  base::TimeDelta today_delta =
      base::Time::Now().LocalMidnight().ToDeltaSinceWindowsEpoch();
  base::TimeDelta yesterday_delta = today_delta - base::Days(2);
  return base::Time::FromDeltaSinceWindowsEpoch(yesterday_delta);
}

// A test implementation of the WallpaperControllerObserver interface.
class TestWallpaperControllerObserver : public WallpaperControllerObserver {
 public:
  explicit TestWallpaperControllerObserver(WallpaperController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }

  TestWallpaperControllerObserver(const TestWallpaperControllerObserver&) =
      delete;
  TestWallpaperControllerObserver& operator=(
      const TestWallpaperControllerObserver&) = delete;

  ~TestWallpaperControllerObserver() override {
    controller_->RemoveObserver(this);
  }

  void SetOnResizeCallback(base::RepeatingClosure callback) {
    resize_callback_ = callback;
  }

  void SetOnColorsCalculatedCallback(base::RepeatingClosure callback) {
    colors_calculated_callback_ = callback;
  }

  // WallpaperControllerObserver
  void OnWallpaperChanged() override { ++wallpaper_changed_count_; }
  void OnWallpaperResized() override {
    if (resize_callback_) {
      resize_callback_.Run();
    }
  }
  void OnWallpaperColorsChanged() override {
    ++colors_changed_count_;

    if (colors_calculated_callback_) {
      colors_calculated_callback_.Run();
    }
  }
  void OnWallpaperBlurChanged() override { ++blur_changed_count_; }
  void OnFirstWallpaperShown() override { ++first_shown_count_; }
  void OnWallpaperPreviewStarted() override {
    DCHECK(!is_in_wallpaper_preview_);
    is_in_wallpaper_preview_ = true;
  }
  void OnWallpaperPreviewEnded() override {
    DCHECK(is_in_wallpaper_preview_);
    is_in_wallpaper_preview_ = false;
  }

  int colors_changed_count() const { return colors_changed_count_; }
  int blur_changed_count() const { return blur_changed_count_; }
  int first_shown_count() const { return first_shown_count_; }
  int wallpaper_changed_count() const { return wallpaper_changed_count_; }
  bool is_in_wallpaper_preview() const { return is_in_wallpaper_preview_; }

 private:
  raw_ptr<WallpaperController, ExperimentalAsh> controller_;

  base::RepeatingClosure resize_callback_;
  base::RepeatingClosure colors_calculated_callback_;

  int colors_changed_count_ = 0;
  int blur_changed_count_ = 0;
  int first_shown_count_ = 0;
  int wallpaper_changed_count_ = 0;
  bool is_in_wallpaper_preview_ = false;
};

}  // namespace

class WallpaperControllerTest : public AshTestBase {
 public:
  WallpaperControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  WallpaperControllerTest(const WallpaperControllerTest&) = delete;
  WallpaperControllerTest& operator=(const WallpaperControllerTest&) = delete;

  void SetUp() override {
    auto pref_manager = WallpaperPrefManager::Create(local_state());
    pref_manager_ = pref_manager.get();
    // Override the pref manager and image downloader that will be used to
    // construct the WallpaperController.
    WallpaperControllerImpl::SetWallpaperPrefManagerForTesting(
        std::move(pref_manager));

    auto test_wallpaper_image_downloader =
        std::make_unique<TestWallpaperImageDownloader>();
    test_wallpaper_image_downloader_ = test_wallpaper_image_downloader.get();
    WallpaperControllerImpl::SetWallpaperImageDownloaderForTesting(
        std::move(test_wallpaper_image_downloader));

    AshTestBase::SetUp();

    TestSessionControllerClient* const client = GetSessionControllerClient();
    client->ProvidePrefServiceForUser(kAccountId1);
    client->ProvidePrefServiceForUser(kAccountId2);
    client->ProvidePrefServiceForUser(
        AccountId::FromUserEmail(user_manager::kGuestUserName));
    client->ProvidePrefServiceForUser(kChildAccountId);

    // This is almost certainly not what was originally intended for these
    // tests, but they have never actually exercised properly decoded
    // wallpapers, as they've never actually been connected to a Data Decoder.
    // We simulate a "crashing" ImageDcoder to get the behavior the tests were
    // written around, but at some point they should probably be fixed.
    in_process_data_decoder_.service().SimulateImageDecoderCrashForTesting(
        true);

    controller_ = Shell::Get()->wallpaper_controller();
    controller_->set_wallpaper_reload_no_delay_for_test();

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(online_wallpaper_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(custom_wallpaper_dir_.CreateUniqueTempDir());
    base::FilePath policy_wallpaper;
    controller_->Init(user_data_dir_.GetPath(), online_wallpaper_dir_.GetPath(),
                      custom_wallpaper_dir_.GetPath(), policy_wallpaper);
    client_.ResetCounts();
    controller_->SetClient(&client_);
    std::unique_ptr<TestWallpaperDriveFsDelegate> drivefs_delegate =
        std::make_unique<TestWallpaperDriveFsDelegate>();
    drivefs_delegate_ = drivefs_delegate.get();
    controller_->SetDriveFsDelegate(std::move(drivefs_delegate));
    client_.set_fake_files_id_for_account_id(kAccountId1, kWallpaperFilesId1);
    client_.set_fake_files_id_for_account_id(kAccountId2, kWallpaperFilesId2);
  }

  void TearDown() override {
    // Although pref services outlive wallpaper controller in the os, in ash
    // tests, they are destroyed in tear down (See |AshTestHelper|). We don't
    // want this timer to run a task after tear down, since it relies on a pref
    // service being around.
    controller_->GetUpdateWallpaperTimerForTesting().Stop();

    AshTestBase::TearDown();
  }

  WallpaperView* wallpaper_view() {
    return Shell::Get()
        ->GetPrimaryRootWindowController()
        ->wallpaper_widget_controller()
        ->wallpaper_view();
  }

 protected:
  // Helper function that tests the wallpaper is always fitted to the native
  // display resolution when the layout is WALLPAPER_LAYOUT_CENTER.
  void WallpaperFitToNativeResolution(WallpaperView* view,
                                      float device_scale_factor,
                                      int image_width,
                                      int image_height,
                                      SkColor color) {
    gfx::Size size = view->bounds().size();
    gfx::Canvas canvas(size, device_scale_factor, true);
    view->OnPaint(&canvas);

    SkBitmap bitmap = canvas.GetBitmap();
    int bitmap_width = bitmap.width();
    int bitmap_height = bitmap.height();
    for (int i = 0; i < bitmap_width; i++) {
      for (int j = 0; j < bitmap_height; j++) {
        if (i >= (bitmap_width - image_width) / 2 &&
            i < (bitmap_width + image_width) / 2 &&
            j >= (bitmap_height - image_height) / 2 &&
            j < (bitmap_height + image_height) / 2) {
          EXPECT_EQ(color, bitmap.getColor(i, j));
        } else {
          EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(i, j));
        }
      }
    }
  }

  // Runs AnimatingWallpaperWidgetController's animation to completion.
  void RunDesktopControllerAnimation() {
    WallpaperWidgetController* controller =
        Shell::Get()
            ->GetPrimaryRootWindowController()
            ->wallpaper_widget_controller();
    ASSERT_TRUE(controller);

    ui::LayerTreeOwner* owner = controller->old_layer_tree_owner_for_testing();
    if (!owner)
      return;

    ASSERT_NO_FATAL_FAILURE(RunAnimationForLayer(owner->root()));
  }

  // Convenience function to ensure ShouldCalculateColors() returns true.
  void EnableShelfColoring() {
    const gfx::ImageSkia kImage = CreateImage(10, 10, kWallpaperColor);
    controller_->ShowWallpaperImage(
        kImage, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
        /*preview_mode=*/false, /*is_override=*/false);
    SetSessionState(SessionState::ACTIVE);

    EXPECT_TRUE(ShouldCalculateColors());
  }

  // Convenience function to set the SessionState.
  void SetSessionState(SessionState session_state) {
    GetSessionControllerClient()->SetSessionState(session_state);
  }

  // Helper function to create a |WallpaperInfo| struct with dummy values
  // given the desired layout.
  WallpaperInfo CreateWallpaperInfo(WallpaperLayout layout) {
    return WallpaperInfo(std::string(), layout, WallpaperType::kDefault,
                         base::Time::Now().LocalMidnight());
  }

  // Saves wallpaper images in the appropriate location for |account_id| and
  // returns the relative path of the file.
  base::FilePath PrecacheWallpapers(const AccountId& account_id) {
    std::string wallpaper_files_id = GetDummyFileId(account_id);

    std::string file_name = GetDummyFileName(account_id);
    base::FilePath small_wallpaper_path =
        GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                               wallpaper_files_id, file_name);
    base::FilePath large_wallpaper_path =
        GetCustomWallpaperPath(WallpaperControllerImpl::kLargeWallpaperSubDir,
                               wallpaper_files_id, file_name);

    // Saves the small/large resolution wallpapers to small/large custom
    // wallpaper paths.
    CHECK(WriteJPEGFile(small_wallpaper_path, kSmallWallpaperMaxWidth,
                        kSmallWallpaperMaxHeight, kSmallCustomWallpaperColor));
    CHECK(WriteJPEGFile(large_wallpaper_path, kLargeWallpaperMaxWidth,
                        kLargeWallpaperMaxHeight, kLargeCustomWallpaperColor));

    return base::FilePath(wallpaper_files_id).Append(file_name);
  }

  // Saves images with different resolution to corresponding paths and saves
  // wallpaper info to local state, so that subsequent calls of |ShowWallpaper|
  // can retrieve the images and info.
  void CreateAndSaveWallpapers(const AccountId& account_id) {
    base::FilePath relative_path = PrecacheWallpapers(account_id);
    // Saves wallpaper info to local state for user.
    WallpaperInfo info = {
        relative_path.value(), WALLPAPER_LAYOUT_CENTER_CROPPED,
        WallpaperType::kCustomized, base::Time::Now().LocalMidnight()};
    ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(account_id, info));
  }

  // Simulates setting a custom wallpaper by directly setting the wallpaper
  // info.
  void SimulateSettingCustomWallpaper(const AccountId& account_id) {
    ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(
        account_id,
        WallpaperInfo("dummy_file_location", WALLPAPER_LAYOUT_CENTER,
                      WallpaperType::kCustomized,
                      base::Time::Now().LocalMidnight())));
  }

  // Initializes default wallpaper paths "*default_*file" and writes JPEG
  // wallpaper images to them. Only needs to be called (once) by tests that
  // want to test loading of default wallpapers.
  void CreateDefaultWallpapers() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(default_wallpaper_dir_.CreateUniqueTempDir());
    const base::FilePath default_wallpaper_path =
        default_wallpaper_dir_.GetPath();

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const base::FilePath small_file =
        default_wallpaper_path.Append(kDefaultSmallWallpaperName);
    command_line->AppendSwitchASCII(switches::kDefaultWallpaperSmall,
                                    small_file.value());
    const base::FilePath large_file =
        default_wallpaper_path.Append(kDefaultLargeWallpaperName);
    command_line->AppendSwitchASCII(switches::kDefaultWallpaperLarge,
                                    large_file.value());

    const base::FilePath guest_small_file =
        default_wallpaper_path.Append(kGuestSmallWallpaperName);
    command_line->AppendSwitchASCII(switches::kGuestWallpaperSmall,
                                    guest_small_file.value());
    const base::FilePath guest_large_file =
        default_wallpaper_path.Append(kGuestLargeWallpaperName);
    command_line->AppendSwitchASCII(switches::kGuestWallpaperLarge,
                                    guest_large_file.value());

    const base::FilePath child_small_file =
        default_wallpaper_path.Append(kChildSmallWallpaperName);
    command_line->AppendSwitchASCII(switches::kChildWallpaperSmall,
                                    child_small_file.value());
    const base::FilePath child_large_file =
        default_wallpaper_path.Append(kChildLargeWallpaperName);
    command_line->AppendSwitchASCII(switches::kChildWallpaperLarge,
                                    child_large_file.value());

    const int kWallpaperSize = 2;
    ASSERT_TRUE(WriteJPEGFile(small_file, kWallpaperSize, kWallpaperSize,
                              kWallpaperColor));
    ASSERT_TRUE(WriteJPEGFile(large_file, kWallpaperSize, kWallpaperSize,
                              kWallpaperColor));

    ASSERT_TRUE(WriteJPEGFile(guest_small_file, kWallpaperSize, kWallpaperSize,
                              kWallpaperColor));
    ASSERT_TRUE(WriteJPEGFile(guest_large_file, kWallpaperSize, kWallpaperSize,
                              kWallpaperColor));

    ASSERT_TRUE(WriteJPEGFile(child_small_file, kWallpaperSize, kWallpaperSize,
                              kWallpaperColor));
    ASSERT_TRUE(WriteJPEGFile(child_large_file, kWallpaperSize, kWallpaperSize,
                              kWallpaperColor));
  }

  // Returns the paths of a small and large jpeg for use with customized default
  // wallpapers.
  [[nodiscard]] std::pair<const base::FilePath, const base::FilePath>
  CreateCustomizationWallpapers() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(customization_wallpaper_dir_.CreateUniqueTempDir());

    base::FilePath root = customization_wallpaper_dir_.GetPath();

    const base::FilePath small_file =
        root.Append(kCustomizationSmallWallpaperName);
    const base::FilePath large_file =
        root.Append(kCustomizationLargeWallpaperName);

    CHECK(WriteJPEGFile(small_file, 800, 800, SK_ColorGREEN));
    CHECK(WriteJPEGFile(large_file, 2000, 2000, SK_ColorBLUE));

    return {small_file, large_file};
  }

  // A helper to test the behavior of setting online wallpaper after the image
  // is decoded. This is needed because image decoding is not supported in unit
  // tests.
  void SetOnlineWallpaperFromImage(
      const AccountId& account_id,
      uint64_t asset_id,
      const gfx::ImageSkia& image,
      const std::string& url,
      const std::string& collection_id,
      WallpaperLayout layout,
      bool save_file,
      bool preview_mode,
      bool from_user,
      uint64_t unit_id,
      const std::vector<OnlineWallpaperVariant>& variants,
      WallpaperController::SetWallpaperCallback callback) {
    const OnlineWallpaperParams params = {
        account_id, asset_id,     GURL(url), collection_id,
        layout,     preview_mode, from_user, /*daily_refresh_enabled=*/false,
        unit_id,    variants};
    controller_->OnOnlineWallpaperDecoded(params, save_file,
                                          std::move(callback), image);
  }

  // Returns color of the current wallpaper. Note: this function assumes the
  // wallpaper has a solid color.
  SkColor GetWallpaperColor() {
    const gfx::ImageSkiaRep& representation =
        controller_->GetWallpaper().GetRepresentation(1.0f);
    return representation.GetBitmap().getColor(0, 0);
  }

  // Wrapper for private ShouldCalculateColors().
  bool ShouldCalculateColors() { return controller_->ShouldCalculateColors(); }

  // Wrapper for private IsDevicePolicyWallpaper().
  bool IsDevicePolicyWallpaper() {
    return controller_->IsDevicePolicyWallpaper();
  }

  int GetWallpaperCount() { return controller_->wallpaper_count_for_testing_; }

  const std::vector<base::FilePath>& GetDecodeFilePaths() {
    return controller_->decode_requests_for_testing_;
  }

  // Returns the `WallpaperInfo` associated with the current
  // `WallpaperResizer`. Usually, this is the same as
  // `GetActiveUserWallpaperInfo()` except when the user is not logged in.
  const WallpaperInfo GetCurrentWallpaperInfo() {
    WallpaperResizer* wallpaper = controller_->current_wallpaper_.get();
    if (!wallpaper)
      return WallpaperInfo();

    return wallpaper->wallpaper_info();
  }

  void SetBypassDecode() { controller_->set_bypass_decode_for_testing(); }

  void ClearWallpaperCount() { controller_->wallpaper_count_for_testing_ = 0; }

  void ClearDecodeFilePaths() {
    controller_->decode_requests_for_testing_.clear();
  }

  void ClearWallpaper() { controller_->current_wallpaper_.reset(); }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  void CacheOnlineWallpaper(std::string path) {
    // Set an Online Wallpaper from Data, so syncing in doesn't need to download
    // an Online Wallpaper.
    SetBypassDecode();
    SimulateUserLogin(kAccountId1);
    ClearWallpaperCount();
    controller_->SetOnlineWallpaper(
        OnlineWallpaperParams(
            kAccountId1, kAssetId, GURL(path),
            /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
            /*preview_mode=*/false, /*from_user=*/false,
            /*daily_refresh_enabled=*/false, kUnitId,
            /*variants=*/std::vector<OnlineWallpaperVariant>()),
        base::DoNothing());
    RunAllTasksUntilIdle();

    // Change the on-screen wallpaper to a different one. (Otherwise the
    // subsequent calls will be no-op since we intentionally prevent reloading
    // the same wallpaper.)
    ClearWallpaperCount();
    controller_->SetDecodedCustomWallpaper(
        kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER_CROPPED,
        /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"",
        CreateImage(640, 480, kWallpaperColor));
    RunAllTasksUntilIdle();
  }

  raw_ptr<WallpaperControllerImpl, ExperimentalAsh> controller_;
  raw_ptr<WallpaperPrefManager, ExperimentalAsh> pref_manager_ =
      nullptr;  // owned by controller

  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir online_wallpaper_dir_;
  base::ScopedTempDir custom_wallpaper_dir_;
  base::ScopedTempDir default_wallpaper_dir_;
  base::ScopedTempDir customization_wallpaper_dir_;
  base::HistogramTester histogram_tester_;

  TestWallpaperControllerClient client_;
  raw_ptr<TestWallpaperDriveFsDelegate> drivefs_delegate_;
  raw_ptr<TestWallpaperImageDownloader> test_wallpaper_image_downloader_;

  const AccountId kChildAccountId =
      AccountId::FromUserEmailGaiaId(kChildEmail, kChildEmail);

 private:
  // TODO(esum): Use ash::InProcessImageDecoder here instead and exercise actual
  // decoding in these tests.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(WallpaperControllerTest, Client) {
  base::FilePath empty_path;
  controller_->Init(empty_path, empty_path, empty_path, empty_path);

  EXPECT_EQ(0u, client_.open_count());
  EXPECT_TRUE(controller_->CanOpenWallpaperPicker());
  controller_->OpenWallpaperPickerIfAllowed();
  EXPECT_EQ(1u, client_.open_count());
}

TEST_F(WallpaperControllerTest, BasicReparenting) {
  WallpaperControllerImpl* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // Wallpaper view/window exists in the wallpaper container and nothing is in
  // the lock screen wallpaper container.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));

  controller->OnSessionStateChanged(session_manager::SessionState::LOCKED);

  // One window is moved from desktop to lock container.
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(1, ChildCountForContainer(kLockScreenWallpaperId));

  controller->OnSessionStateChanged(session_manager::SessionState::ACTIVE);

  // One window is moved from lock to desktop container.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));
}

TEST_F(WallpaperControllerTest, SwitchWallpapersWhenNewWallpaperAnimationEnds) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create the wallpaper and its view.
  WallpaperControllerImpl* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // The new wallpaper is ready to animate.
  WallpaperWidgetController* widget_controller =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->wallpaper_widget_controller();
  EXPECT_TRUE(widget_controller->IsAnimating());

  // Force the animation to play to completion.
  RunDesktopControllerAnimation();
  EXPECT_FALSE(widget_controller->IsAnimating());
}

// Test for crbug.com/149043 "Unlock screen, no launcher appears". Ensure we
// move all wallpaper views if there are more than one.
TEST_F(WallpaperControllerTest, WallpaperMovementDuringUnlock) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  WallpaperControllerImpl* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // User locks the screen, which moves the wallpaper forward.
  controller->OnSessionStateChanged(session_manager::SessionState::LOCKED);

  // Suspend/resume cycle causes wallpaper to refresh, loading a new wallpaper
  // that will animate in on top of the old one.
  controller->CreateEmptyWallpaperForTesting();

  // In this state we have a wallpaper views stored in
  // LockScreenWallpaperContainer.
  WallpaperWidgetController* widget_controller =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->wallpaper_widget_controller();
  EXPECT_TRUE(widget_controller->IsAnimating());
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(1, ChildCountForContainer(kLockScreenWallpaperId));
  // There must be three layers, shield, original and old layers.
  ASSERT_EQ(3u, wallpaper_view()->layer()->parent()->children().size());

  // Before the wallpaper's animation completes, user unlocks the screen, which
  // moves the wallpaper to the back.
  controller->OnSessionStateChanged(session_manager::SessionState::ACTIVE);

  // Ensure that widget has moved.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  // There must be two layers, original and old layers while animating.
  ASSERT_EQ(2u, wallpaper_view()->layer()->parent()->children().size());
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));

  // Finish the new wallpaper animation.
  RunDesktopControllerAnimation();

  // Now there is one wallpaper and layer.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  ASSERT_EQ(1u, wallpaper_view()->layer()->parent()->children().size());
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));
}

// Test for crbug.com/156542. Animating wallpaper should immediately finish
// animation and replace current wallpaper before next animation starts.
TEST_F(WallpaperControllerTest, ChangeWallpaperQuick) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  WallpaperControllerImpl* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // Change to a new wallpaper.
  controller->CreateEmptyWallpaperForTesting();

  WallpaperWidgetController* widget_controller =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->wallpaper_widget_controller();
  EXPECT_TRUE(widget_controller->IsAnimating());

  // Change to another wallpaper before animation finished.
  controller->CreateEmptyWallpaperForTesting();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  EXPECT_FALSE(widget_controller->IsAnimating());
}

TEST_F(WallpaperControllerTest, ResizeCustomWallpaper) {
  UpdateDisplay("320x200");

  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);

  // Set the image as custom wallpaper, wait for the resize to finish, and check
  // that the resized image is the expected size.
  controller_->ShowWallpaperImage(
      image, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
      /*preview_mode=*/false, /*is_override=*/false);
  EXPECT_TRUE(image.BackedBySameObjectAs(controller_->GetWallpaper()));
  RunAllTasksUntilIdle();
  gfx::ImageSkia resized_image = controller_->GetWallpaper();
  EXPECT_FALSE(image.BackedBySameObjectAs(resized_image));
  EXPECT_EQ(gfx::Size(320, 200).ToString(), resized_image.size().ToString());

  // Load the original wallpaper again and check that we're still using the
  // previously-resized image instead of doing another resize
  // (http://crbug.com/321402).
  controller_->ShowWallpaperImage(
      image, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
      /*preview_mode=*/false, /*is_override=*/false);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(resized_image.BackedBySameObjectAs(controller_->GetWallpaper()));
}

// Test that the wallpaper is always fitted to the native display resolution
// when the layout is WALLPAPER_LAYOUT_CENTER to prevent blurry images.
TEST_F(WallpaperControllerTest, DontScaleWallpaperWithCenterLayout) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Size high_resolution(3600, 2400);
  const gfx::Size low_resolution(360, 240);
  const float high_dsf = 2.0f;
  const float low_dsf = 1.0f;

  gfx::ImageSkia image_high_res = CreateImage(
      high_resolution.width(), high_resolution.height(), kWallpaperColor);
  gfx::ImageSkia image_low_res = CreateImage(
      low_resolution.width(), low_resolution.height(), kWallpaperColor);

  UpdateDisplay("1200x600*2");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 high resolution"));
    controller_->ShowWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*is_override=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*is_override=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 high resolution"));
    controller_->ShowWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*is_override=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*is_override=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600/u@1.5");  // 1.5 ui scale
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 high resolution"));
    controller_->ShowWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*is_override=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*is_override=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }
}

TEST_F(WallpaperControllerTest, ShouldCalculateColorsBasedOnImage) {
  EnableShelfColoring();
  EXPECT_TRUE(ShouldCalculateColors());

  controller_->CreateEmptyWallpaperForTesting();
  EXPECT_FALSE(ShouldCalculateColors());
}

TEST_F(WallpaperControllerTest, ShouldCalculateColorsBasedOnSessionState) {
  EnableShelfColoring();

  SetSessionState(SessionState::UNKNOWN);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::OOBE);
  EXPECT_TRUE(ShouldCalculateColors());

  SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(ShouldCalculateColors());

  SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::LOGIN_SECONDARY);
  EXPECT_FALSE(ShouldCalculateColors());
}

TEST_F(WallpaperControllerTest, ColorsCalculatedForMostRecentWallpaper) {
  TestWallpaperControllerObserver observer(controller_);
  // Total size of image must be greater than 100 pixels to trigger the async
  // codepath (and any potential cancellation).
  const int dimension = 20;

  // Activate so we calculate colors.
  SetSessionState(SessionState::ACTIVE);

  base::RunLoop run_loop;
  observer.SetOnResizeCallback(run_loop.QuitClosure());
  // Sets the wallpaper to magenta.
  const gfx::ImageSkia old_image =
      CreateImage(dimension, dimension, kWallpaperColor);
  WallpaperInfo old_info = CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH);
  old_info.location = "old";
  controller_->ShowWallpaperImage(old_image, old_info,
                                  /*preview_mode=*/false,
                                  /*is_override=*/false);
  // Run the controller until resize completes for the first wallpaper and
  // color calculation starts.
  run_loop.Run();
  observer.SetOnResizeCallback(base::NullCallback());

  base::RunLoop colors_loop;
  observer.SetOnColorsCalculatedCallback(colors_loop.QuitClosure());

  // Immediately switch the wallpaper color to blue.
  const gfx::ImageSkia image = CreateImage(dimension, dimension, SK_ColorBLUE);
  WallpaperInfo info = CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH);
  // Set location to somethind different than in `old_info`.
  info.location = "new";

  controller_->ShowWallpaperImage(image, info,
                                  /*preview_mode=*/false,
                                  /*is_override=*/false);

  // Run until we get a notification of colors changed.
  colors_loop.Run();

  // There should only be one color change event if we interrupted the first
  // attempt.
  EXPECT_EQ(observer.colors_changed_count(), 1);
  EXPECT_EQ(controller_->calculated_colors()->k_mean_color, SK_ColorBLUE);
  EXPECT_FALSE(pref_manager_->GetCachedKMeanColor("old"));
  EXPECT_TRUE(pref_manager_->GetCachedKMeanColor("new"));

  base::RunLoop load_preview_image_loop;
  controller_->LoadPreviewImage(base::BindLambdaForTesting(
      [quit = load_preview_image_loop.QuitClosure()](
          scoped_refptr<base::RefCountedMemory> image_bytes) {
        EXPECT_TRUE(image_bytes);
        std::move(quit).Run();
      }));
  load_preview_image_loop.Run();
}

TEST_F(WallpaperControllerTest, CelebiNotSavedWhenJellyIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(chromeos::features::kJelly);
  TestWallpaperControllerObserver observer(controller_);

  const char location[] = "test_wallpaper_here";

  // Set the wallpaper with a valid location.
  WallpaperInfo wallpaper_info = CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH);
  wallpaper_info.location = location;
  const gfx::ImageSkia kImage = CreateImage(10, 10, kWallpaperColor);
  controller_->ShowWallpaperImage(kImage, wallpaper_info,
                                  /*preview_mode=*/false,
                                  /*is_override=*/false);
  SetSessionState(SessionState::ACTIVE);

  // Wait for color computation to complete.
  base::RunLoop colors_loop;
  observer.SetOnColorsCalculatedCallback(colors_loop.QuitClosure());
  colors_loop.Run();

  EXPECT_FALSE(pref_manager_->GetCelebiColor(location));
}

TEST_F(WallpaperControllerTest, SaveCelebiColorWhenJellyActive) {
  base::test::ScopedFeatureList features(chromeos::features::kJelly);
  TestWallpaperControllerObserver observer(controller_);

  const char location[] = "test_wallpaper_here";

  // Set the wallpaper with a valid location.
  WallpaperInfo wallpaper_info = CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH);
  wallpaper_info.location = location;
  const gfx::ImageSkia kImage = CreateImage(10, 10, kWallpaperColor);
  controller_->ShowWallpaperImage(kImage, wallpaper_info,
                                  /*preview_mode=*/false,
                                  /*is_override=*/false);
  SetSessionState(SessionState::ACTIVE);

  // Wait for color computation to complete.
  base::RunLoop colors_loop;
  observer.SetOnColorsCalculatedCallback(colors_loop.QuitClosure());
  colors_loop.Run();

  EXPECT_EQ(kWallpaperColor, pref_manager_->GetCelebiColor(location));
}

TEST_F(WallpaperControllerTest, EnableShelfColoringNotifiesObservers) {
  TestWallpaperControllerObserver observer(controller_);
  EXPECT_EQ(0, observer.colors_changed_count());

  // Enable shelf coloring will set a customized wallpaper image and change
  // session state to ACTIVE, which will trigger wallpaper colors calculation.
  EnableShelfColoring();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.colors_changed_count());
}

TEST_F(WallpaperControllerTest, ProminentColor_CachedColorsAvailableAtLogin) {
  SetBypassDecode();
  // Cache some wallpapers and store that in the local prefs. Otherwise, we
  // can't cache colors.
  base::FilePath relative_path = PrecacheWallpapers(kAccountId1);
  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  info.location = relative_path.value();
  ASSERT_TRUE(pref_manager_->SetLocalWallpaperInfo(kAccountId1, info));

  // Store colors in local prefs simulating cache behavior.
  const std::vector<SkColor> prominent_colors = {SK_ColorGREEN, SK_ColorRED,
                                                 SK_ColorBLUE,  SK_ColorWHITE,
                                                 SK_ColorWHITE, SK_ColorWHITE};
  pref_manager_->CacheProminentColors(relative_path.value(), prominent_colors);
  const SkColor k_means_color = SK_ColorLTGRAY;
  pref_manager_->CacheKMeanColor(relative_path.value(), k_means_color);

  // Reset to login screen.
  GetSessionControllerClient()->RequestSignOut();

  TestWallpaperControllerObserver observer(controller_);
  ASSERT_EQ(0, observer.colors_changed_count());

  // Show user wallpaper in login screen. We are *not* logged in yet.
  controller_->ShowUserWallpaper(kAccountId1,
                                 user_manager::UserType::USER_TYPE_REGULAR);
  task_environment()->RunUntilIdle();

  // Showing a user wallpaper should cause the cached colors to be fetched and
  // reported.
  EXPECT_EQ(1, observer.colors_changed_count());

  // DARK_VIBRANT happens to be prominent color 0.
  EXPECT_EQ(SK_ColorGREEN, controller_->GetProminentColor(
                               {color_utils::LumaRange::DARK,
                                color_utils::SaturationRange::VIBRANT}));
  EXPECT_EQ(k_means_color, controller_->GetKMeanColor());
}

TEST_F(WallpaperControllerTest, ProminentColor_ClearedBetweenUsers) {
  SetBypassDecode();
  // Setup prominent colors for account 1.
  base::FilePath relative_path = PrecacheWallpapers(kAccountId1);
  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  info.location = relative_path.value();
  ASSERT_TRUE(pref_manager_->SetLocalWallpaperInfo(kAccountId1, info));

  const std::vector<SkColor> prominent_colors = {SK_ColorGREEN, SK_ColorRED,
                                                 SK_ColorBLUE,  SK_ColorWHITE,
                                                 SK_ColorWHITE, SK_ColorWHITE};
  pref_manager_->CacheProminentColors(relative_path.value(), prominent_colors);
  const SkColor k_means_color = SK_ColorLTGRAY;
  pref_manager_->CacheKMeanColor(relative_path.value(), k_means_color);

  // Set a wallpaper for account 2.
  WallpaperInfo info2 = InfoWithType(WallpaperType::kDefault);
  ASSERT_TRUE(pref_manager_->SetLocalWallpaperInfo(kAccountId2, info2));

  // Reset to login screen.
  GetSessionControllerClient()->RequestSignOut();

  TestWallpaperControllerObserver observer(controller_);

  // No notifications should have occurred yet.
  EXPECT_EQ(0, observer.colors_changed_count());

  // Show wallpaper for account 1.
  controller_->ShowUserWallpaper(kAccountId1,
                                 user_manager::UserType::USER_TYPE_REGULAR);
  task_environment()->RunUntilIdle();

  // Should have received a notification for the cached colors.
  EXPECT_EQ(1, observer.colors_changed_count());

  // Verify that we can retrieve the prominent color.
  EXPECT_EQ(SK_ColorGREEN, controller_->GetProminentColor(
                               {color_utils::LumaRange::DARK,
                                color_utils::SaturationRange::VIBRANT}));

  // Show wallpaper for account 2.
  controller_->ShowUserWallpaper(kAccountId2,
                                 user_manager::UserType::USER_TYPE_REGULAR);
  task_environment()->RunUntilIdle();
  // Since account 2 has not cached colors and wallpaper decoding is disabled,
  // the prominent color should be invalid.
  EXPECT_EQ(
      kInvalidWallpaperColor,
      controller_->GetProminentColor({color_utils::LumaRange::DARK,
                                      color_utils::SaturationRange::VIBRANT}));
  // We got one notification for the first user but nothing after because the
  // wallpaper color hasn't been computed yet.
  EXPECT_EQ(1, observer.colors_changed_count());
}

TEST_F(WallpaperControllerTest,
       OnWallpaperColorsChangedAlwaysCalledOnFirstUpdate) {
  TestWallpaperControllerObserver observer(controller_);
  controller_->ShowUserWallpaper(kAccountId1,
                                 user_manager::UserType::USER_TYPE_REGULAR);
  task_environment()->RunUntilIdle();

  // Even though the wallpaper color is invalid, observers should still be
  // notified for the first update.
  EXPECT_EQ(observer.colors_changed_count(), 1);

  controller_->ShowUserWallpaper(kAccountId2,
                                 user_manager::UserType::USER_TYPE_REGULAR);
  task_environment()->RunUntilIdle();

  // Observers should not be notified after the first update if the colors do
  // not change.
  EXPECT_EQ(observer.colors_changed_count(), 1);
}

TEST_F(WallpaperControllerTest,
       UpdatePrimaryUserWallpaperWhileSecondUserActive) {
  SetBypassDecode();
  WallpaperInfo wallpaper_info;

  SimulateUserLogin(kAccountId1);

  // Set an online wallpaper with image data. Verify that the wallpaper is set
  // successfully.
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>());
  controller_->SetOnlineWallpaper(params, base::DoNothing());
  RunAllTasksUntilIdle();
  // Verify that the user wallpaper info is updated.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(params);
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));

  // Log in |kUser2|, and set another online wallpaper for |kUser1|. Verify that
  // the on-screen wallpaper doesn't change since |kUser1| is not active, but
  // wallpaper info is updated properly.
  SimulateUserLogin(kAccountId2);
  ClearWallpaperCount();
  const OnlineWallpaperParams& new_params = OnlineWallpaperParams(
      kAccountId1, kAssetId2, GURL(kDummyUrl2),
      /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId2,
      /*variants=*/std::vector<OnlineWallpaperVariant>());
  controller_->SetOnlineWallpaper(new_params, base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info_2(new_params);
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info_2));
}

TEST_F(WallpaperControllerTest, SetOnlineWallpaper) {
  SetBypassDecode();

  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
  SimulateUserLogin(kAccountId1);

  // Verify that calling |SetOnlineWallpaper| will download the image data if it
  // does not exist. Verify that the wallpaper is set successfully.
  auto run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>());
  controller_->SetOnlineWallpaper(
      params, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
  // Verify that the user wallpaper info is updated.
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(params);
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
  // Verify that wallpaper & collection metrics are logged.
  histogram_tester().ExpectBucketCount("Ash.Wallpaper.Image", kUnitId, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Wallpaper.Collection",
      static_cast<int>(base::PersistentHash(
          TestWallpaperControllerClient::kDummyCollectionId)),
      1);
  histogram_tester().ExpectBucketCount("Ash.Wallpaper.Type",
                                       WallpaperType::kOnline, 1);

  // Verify that the wallpaper with |url| is available offline, and the returned
  // file name should not contain the small wallpaper suffix.
  //
  // The ThreadPool must be flushed to ensure that the online wallpaper is saved
  // to disc before checking the test expectation below. Ideally, we'd wait for
  // an explicit event, but the production code does not need this and it's not
  // worthwhile to add something to the API just for tests.
  RunAllTasksUntilIdle();
  EXPECT_TRUE(base::PathExists(online_wallpaper_dir_.GetPath().Append(
      GURL(kDummyUrl).ExtractFileName())));
}

TEST_F(WallpaperControllerTest, SetAndRemovePolicyWallpaper) {
  SetBypassDecode();
  // Simulate the login screen.
  ClearLogin();

  // The user starts with no wallpaper info and is not controlled by policy.
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_FALSE(controller_->IsWallpaperControlledByPolicy(kAccountId1));
  // A default wallpaper is shown for the user.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);

  // Set a policy wallpaper. Verify that the user becomes policy controlled and
  // the wallpaper info is updated.
  ClearWallpaperCount();
  controller_->SetPolicyWallpaper(kAccountId1, user_manager::USER_TYPE_REGULAR,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo policy_wallpaper_info(base::FilePath(kWallpaperFilesId1)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED,
                                      WallpaperType::kPolicy,
                                      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId1));
  // Verify the wallpaper is not updated since the user hasn't logged in.
  EXPECT_EQ(0, GetWallpaperCount());

  // Log in the user. Verify the policy wallpaper is now being shown.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kPolicy);

  // Clear the wallpaper and log out the user. Verify the policy wallpaper is
  // shown in the login screen.
  ClearWallpaper();
  ClearLogin();
  controller_->ClearPrefChangeObserverForTesting();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kPolicy);
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId1));
  // Remove the policy wallpaper. Verify the wallpaper info is reset to default
  // and the user is no longer policy controlled.
  ClearWallpaperCount();
  controller_->RemovePolicyWallpaper(kAccountId1);
  WaitUntilCustomWallpapersDeleted(kAccountId1);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));
  EXPECT_FALSE(controller_->IsWallpaperControlledByPolicy(kAccountId1));
  // Verify the wallpaper is not updated since the user hasn't logged in (to
  // avoid abrupt wallpaper change in login screen).
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kPolicy);

  // Log in the user. Verify the default wallpaper is now being shown.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
}

// Simulates the scenario where the wallpaper are not yet resized and only the
// original size image is available.
TEST_F(WallpaperControllerTest, ShowUserWallpaper_OriginalFallback) {
  SetBypassDecode();
  CreateDefaultWallpapers();

  // Simulate the login screen.
  ClearLogin();

  // Set a wallpaper.
  CreateAndSaveWallpapers(kAccountId1);
  RunAllTasksUntilIdle();

  // Verify the wallpaper was set.
  WallpaperInfo wallpaper_info;
  ASSERT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  ASSERT_EQ(WallpaperType::kCustomized, wallpaper_info.type);
  ASSERT_EQ("user1@test.com-hash/user1@test.com-file", wallpaper_info.location);

  // Move the wallpaper file to the original folder.
  base::FilePath saved_wallpaper = custom_wallpaper_dir_.GetPath().Append(
      "small/user1@test.com-hash/user1@test.com-file");
  ASSERT_TRUE(base::PathExists(saved_wallpaper));
  base::CreateDirectory(
      WallpaperControllerImpl::GetCustomWallpaperDir("original")
          .Append("user1@test.com-hash"));
  ASSERT_TRUE(base::PathExists(
      WallpaperControllerImpl::GetCustomWallpaperDir("original")));
  ASSERT_TRUE(
      base::Move(saved_wallpaper,
                 WallpaperControllerImpl::GetCustomWallpaperDir("original")
                     .Append(wallpaper_info.location)));
  ASSERT_FALSE(base::PathExists(saved_wallpaper));
  ClearDecodeFilePaths();

  // Show wallpaper
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();

  // Verify the wallpaper was found in the original folder.
  EXPECT_FALSE(GetDecodeFilePaths().empty());
  EXPECT_THAT(
      GetDecodeFilePaths().back().value(),
      testing::EndsWith("original/user1@test.com-hash/user1@test.com-file"));
}

// Simulates a missing wallpaper due (possibly) an outdated preference. In this
// situation, we fallback to the default.
TEST_F(WallpaperControllerTest, ShowUserWallpaper_MissingFile) {
  CreateDefaultWallpapers();

  // Simulate the login screen.
  ClearLogin();

  // Set a wallpaper.
  CreateAndSaveWallpapers(kAccountId1);
  RunAllTasksUntilIdle();

  // Verify the wallpaper was set.
  WallpaperInfo wallpaper_info;
  ASSERT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  ASSERT_EQ(WallpaperType::kCustomized, wallpaper_info.type);
  ASSERT_EQ("user1@test.com-hash/user1@test.com-file", wallpaper_info.location);

  // Delete wallpaper file.
  EXPECT_TRUE(base::DeleteFile(
      user_data_dir_.GetPath().Append(wallpaper_info.location)));
  ClearDecodeFilePaths();

  // Show wallpaper
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();

  // Verify the default wallpaper was used because the stored wallpaper was
  // missing.
  EXPECT_FALSE(GetDecodeFilePaths().empty());
  EXPECT_THAT(GetDecodeFilePaths().back().value(),
              testing::EndsWith(kDefaultSmallWallpaperName));
}

TEST_F(WallpaperControllerTest, RemovePolicyWallpaperNoOp) {
  auto verify_custom_wallpaper_info = [&]() {
    EXPECT_EQ(WallpaperType::kCustomized, controller_->GetWallpaperType());
    EXPECT_EQ(kWallpaperColor, GetWallpaperColor());

    WallpaperInfo wallpaper_info;
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    WallpaperInfo expected_wallpaper_info(
        base::FilePath(kWallpaperFilesId1).Append(kFileName1).value(),
        WALLPAPER_LAYOUT_CENTER, WallpaperType::kCustomized,
        base::Time::Now().LocalMidnight());
    EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
  };

  // Set a custom wallpaper. Verify the user is not policy controlled and the
  // wallpaper info is correct.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"",
      CreateImage(640, 480, kWallpaperColor));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_FALSE(controller_->IsWallpaperControlledByPolicy(kAccountId1));
  verify_custom_wallpaper_info();

  // Verify RemovePolicyWallpaper() is a no-op when the user doesn't have a
  // policy wallpaper.
  controller_->RemovePolicyWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  verify_custom_wallpaper_info();
}

TEST_F(WallpaperControllerTest, SetThirdPartyWallpaper) {
  SimulateUserLogin(kAccountId1);
  // Verify the user starts with no wallpaper info.
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia third_party_wallpaper = CreateImage(640, 480, kWallpaperColor);

  // Set a third-party wallpaper for |kUser1|.
  EXPECT_TRUE(controller_->SetThirdPartyWallpaper(
      kAccountId1, kFileName1, layout, third_party_wallpaper));

  RunAllTasksUntilIdle();
  // Verify the wallpaper is shown.
  EXPECT_EQ(1, GetWallpaperCount());
  // Verify the user wallpaper info is updated.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(
      base::FilePath(kWallpaperFilesId1).Append(kFileName1).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
  EXPECT_EQ(kAccountId1, drivefs_delegate_->get_save_wallpaper_account_id());
}

TEST_F(WallpaperControllerTest, SetThirdPartyWallpaper_NonactiveUser) {
  // Active user is |kUser2|, but set another third-party wallpaper for
  // |kUser1|; the operation should not be allowed, because |kUser1| is not the
  // active user.
  SimulateUserLogin(kAccountId2);
  WallpaperInfo wallpaper_info;
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia third_party_wallpaper = CreateImage(640, 480, kWallpaperColor);

  EXPECT_FALSE(controller_->SetThirdPartyWallpaper(
      kAccountId1, kFileName2, layout, third_party_wallpaper));

  // Verify the wallpaper is not shown.
  EXPECT_EQ(0, GetWallpaperCount());
  // Verify the wallpaper info for |kUser1| is updated, because setting
  // wallpaper is still allowed for non-active users.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info_2(
      base::FilePath(kWallpaperFilesId1).Append(kFileName2).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info_2));
}

TEST_F(WallpaperControllerTest, SetThirdPartyWallpaper_PolicyWallpaper) {
  SetBypassDecode();
  SimulateUserLogin(kAccountId2);
  WallpaperInfo wallpaper_info;
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia third_party_wallpaper = CreateImage(640, 480, kWallpaperColor);
  // Set a policy wallpaper for |kUser2|. Verify that |kUser2| becomes policy
  // controlled.
  controller_->SetPolicyWallpaper(kAccountId2, user_manager::USER_TYPE_REGULAR,
                                  /*data=*/std::string());
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId2));
  EXPECT_TRUE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Setting a third-party wallpaper for |kUser2| should not be allowed, because
  // third-party wallpapers cannot be set for policy controlled users.
  ClearWallpaperCount();
  EXPECT_FALSE(controller_->SetThirdPartyWallpaper(
      kAccountId2, kFileName1, layout, third_party_wallpaper));

  // Verify the wallpaper is not shown.
  EXPECT_EQ(0, GetWallpaperCount());
  // Verify |kUser2| is still policy controlled and has the policy wallpaper
  // info.
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId2));
  EXPECT_TRUE(controller_->IsActiveUserWallpaperControlledByPolicy());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId2, &wallpaper_info));
  WallpaperInfo policy_wallpaper_info(base::FilePath(kWallpaperFilesId2)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED,
                                      WallpaperType::kPolicy,
                                      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForRegularAccount) {
  CreateDefaultWallpapers();
  SimulateUserLogin(kAccountId1);

  // First, simulate setting a user custom wallpaper.
  SimulateSettingCustomWallpaper(kAccountId1);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the large default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(kAccountId1, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  // The user wallpaper info has been reset to the default value.
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));

  SimulateSettingCustomWallpaper(kAccountId1);
  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the small default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(kAccountId1, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  // The user wallpaper info has been reset to the default value.
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));

  SimulateSettingCustomWallpaper(kAccountId1);
  // Verify that when screen is rotated, |SetDefaultWallpaper| removes the
  // previously set custom wallpaper info, and the small default wallpaper is
  // set successfully with the correct file path.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(kAccountId1, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  // The user wallpaper info has been reset to the default value.
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForChildAccount) {
  CreateDefaultWallpapers();

  SimulateUserLogin(kChildAccountId, user_manager::USER_TYPE_CHILD);

  // Verify the large child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(kChildAccountId, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kChildLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  // Verify the small child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(kChildAccountId, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kChildSmallWallpaperName),
            GetDecodeFilePaths()[0]);
}

// Verify that the |ShowWallpaperImage| will be called with the default image
// for the guest session only even if there's a policy that has been set for
// another user which invokes |SetPolicyWallpaper|.
TEST_F(WallpaperControllerTest,
       SetDefaultWallpaperForGuestSessionUnaffectedByWallpaperPolicy) {
  SetBypassDecode();
  // Simulate the login screen.
  ClearLogin();
  CreateDefaultWallpapers();
  ClearWallpaperCount();

  // First, simulate settings for a guest user which will show the default
  // wallpaper image by invoking |ShowWallpaperImage|.
  SimulateGuestLogin();

  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();

  const AccountId guest_id =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  SimulateUserLogin(guest_id, user_manager::USER_TYPE_GUEST);
  controller_->SetDefaultWallpaper(guest_id, /*show_wallpaper=*/true,
                                   base::DoNothing());

  WallpaperInfo wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  // Verify that the current displayed wallpaper is the default one inside the
  // guest session.
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  // Second, set a user policy for which is being set for another
  // user and verifying that the policy has been applied successfully.
  WallpaperInfo policy_wallpaper_info;
  controller_->SetPolicyWallpaper(kAccountId1, user_manager::USER_TYPE_REGULAR,
                                  /*data=*/std::string());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &policy_wallpaper_info));
  WallpaperInfo expected_policy_wallpaper_info(
      base::FilePath(kWallpaperFilesId1)
          .Append("policy-controlled.jpeg")
          .value(),
      WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kPolicy,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      policy_wallpaper_info.MatchesSelection(expected_policy_wallpaper_info));
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId1));

  // Finally, verifying that the guest session hasn't been affected by the new
  // policy and |ShowWallpaperImage| hasn't been invoked another time.

  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestLargeWallpaperName),
            GetDecodeFilePaths()[0]);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForGuestSessionAndPreview) {
  CreateDefaultWallpapers();

  const AccountId guest_id =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  controller_->ShowUserWallpaper(guest_id);
  SimulateUserLogin(guest_id, user_manager::USER_TYPE_GUEST);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_EQ(wallpaper_info.type, WallpaperType::kDefault);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForGuestSession) {
  CreateDefaultWallpapers();

  // First, simulate setting a custom wallpaper for a regular user.
  SimulateUserLogin(kAccountId1);
  SimulateSettingCustomWallpaper(kAccountId1);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  const AccountId guest_id =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  SimulateUserLogin(guest_id, user_manager::USER_TYPE_GUEST);

  // Verify that during a guest session, |SetDefaultWallpaper| removes the user
  // custom wallpaper info, but a guest specific wallpaper should be set,
  // instead of the regular default wallpaper.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(guest_id, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(guest_id, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestSmallWallpaperName),
            GetDecodeFilePaths()[0]);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperCallbackTiming) {
  SetBypassDecode();
  SimulateUserLogin(kAccountId1);

  // First, simulate setting a user custom wallpaper.
  SimulateSettingCustomWallpaper(kAccountId1);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_NE(wallpaper_info.type, WallpaperType::kDefault);

  TestWallpaperControllerObserver observer(controller_);

  // Set default wallpaper and wait for success callback.
  base::RunLoop loop;
  controller_->SetDefaultWallpaper(
      kAccountId1, /*show_wallpaper=*/true,
      base::BindLambdaForTesting([&loop, &observer](bool success) {
        ASSERT_TRUE(success);
        // Success callback should run before wallpaper observer is notified of
        // change.
        ASSERT_EQ(0, observer.wallpaper_changed_count());
        loop.Quit();
      }));
  loop.Run();
  // Wallpaper observer should have been notified of wallpaper change.
  EXPECT_EQ(1, observer.wallpaper_changed_count());
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestInKioskMode) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin("kiosk", user_manager::USER_TYPE_KIOSK_APP);

  // Verify that |SetDecodedCustomWallpaper| doesn't set wallpaper in kiosk
  // mode, and |kAccountId1|'s wallpaper info is not updated.
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"", image);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  // Verify that |SetOnlineWallpaper| doesn't set wallpaper in kiosk
  // mode, and |kAccountId1|'s wallpaper info is not updated.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  controller_->SetOnlineWallpaper(
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            /*collection_id=*/std::string(),
                            WALLPAPER_LAYOUT_CENTER,
                            /*preview_mode=*/false, /*from_user=*/false,
                            /*daily_refresh_enabled=*/false, kUnitId,
                            /*variants=*/std::vector<OnlineWallpaperVariant>()),
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper in kiosk mode, and
  // |kAccountId1|'s wallpaper info is not updated.
  ClearWallpaperCount();
  controller_->SetDefaultWallpaper(kAccountId1, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
}

// Disable the wallpaper setting for public session since it is ephemeral.
TEST_F(WallpaperControllerTest, NotShowWallpaperSettingInPublicSession) {
  SimulateUserLogin("public_session", user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(controller_->ShouldShowWallpaperSetting());
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestWhenPolicyIsEnforced) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(kAccountId1);

  // Set a policy wallpaper for the user. Verify the user is policy controlled.
  controller_->SetPolicyWallpaper(kAccountId1, user_manager::USER_TYPE_REGULAR,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId1));

  WallpaperInfo wallpaper_info;
  WallpaperInfo policy_wallpaper_info(base::FilePath(kWallpaperFilesId1)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED,
                                      WallpaperType::kPolicy,
                                      base::Time::Now().LocalMidnight());

  {
    // Verify that |SetDecodedCustomWallpaper| doesn't set wallpaper when policy
    // is enforced, and the user wallpaper info is not updated.
    ClearWallpaperCount();
    controller_->SetDecodedCustomWallpaper(
        kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
        /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"", image);
    RunAllTasksUntilIdle();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }

  {
    // Verify that |SetCustomWallpaper| with callback doesn't set wallpaper when
    // policy is enforced, and the user wallpaper info is not updated.
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    ClearWallpaperCount();
    controller_->SetCustomWallpaper(
        kAccountId1, base::FilePath(kFileName1), WALLPAPER_LAYOUT_CENTER,
        /*preview_mode=*/false,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop->Quit();
        }));
    run_loop->Run();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }

  {
    // Verify that |SetOnlineWallpaper| doesn't set wallpaper when
    // policy is enforced, and the user wallpaper info is not updated.
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    ClearWallpaperCount();
    controller_->SetOnlineWallpaper(
        OnlineWallpaperParams(
            kAccountId1, kAssetId, GURL(kDummyUrl),
            /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
            /*preview_mode=*/false, /*from_user=*/false,
            /*daily_refresh_enabled=*/false, kUnitId,
            /*variants=*/std::vector<OnlineWallpaperVariant>()),
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop->Quit();
        }));
    run_loop->Run();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }

  {
    // Verify that |SetOnlineWallpaper| doesn't set wallpaper when policy is
    // enforced, and the user wallpaper info is not updated.
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    ClearWallpaperCount();
    controller_->SetOnlineWallpaper(
        OnlineWallpaperParams(
            kAccountId1, kAssetId, GURL(kDummyUrl),
            /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
            /*preview_mode=*/false, /*from_user=*/false,
            /*daily_refresh_enabled=*/false, kUnitId,
            /*variants=*/std::vector<OnlineWallpaperVariant>()),
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop->Quit();
        }));
    run_loop->Run();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }

  {
    // Verify that |SetDefaultWallpaper| doesn't set wallpaper when policy is
    // enforced, and the user wallpaper info is not updated.
    ClearWallpaperCount();
    controller_->SetDefaultWallpaper(kAccountId1, true /*show_wallpaper=*/,
                                     base::DoNothing());
    RunAllTasksUntilIdle();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }
}

TEST_F(WallpaperControllerTest, VerifyWallpaperCache) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(kAccountId1);

  // |kUser1| doesn't have wallpaper cache in the beginning.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  base::FilePath path;
  EXPECT_FALSE(controller_->GetPathFromCache(kAccountId1, &path));

  // Verify |SetOnlineWallpaper| updates wallpaper cache for |user1|.
  controller_->SetOnlineWallpaper(
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            /*collection_id=*/std::string(),
                            WALLPAPER_LAYOUT_CENTER,
                            /*preview_mode=*/false, /*from_user=*/false,
                            /*daily_refresh_enabled=*/false, kUnitId,
                            /*variants=*/std::vector<OnlineWallpaperVariant>()),
      base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(kAccountId1, &path));

  // After |kUser2| is logged in, |user1|'s wallpaper cache should still be kept
  // (crbug.com/339576). Note the active user is still |user1|.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(kUser2);
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(kAccountId1, &path));

  // Verify |SetDefaultWallpaper| clears wallpaper cache.
  controller_->SetDefaultWallpaper(kAccountId1, true /*show_wallpaper=*/,
                                   base::DoNothing());
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(kAccountId1, &path));

  // Verify |SetDecodedCustomWallpaper| updates wallpaper cache for |user1|.
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"", image);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(kAccountId1, &path));

  // Verify |RemoveUserWallpaper| clears wallpaper cache.
  controller_->RemoveUserWallpaper(kAccountId1, base::DoNothing());
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(kAccountId1, &path));
}

// Tests that the appropriate wallpaper (large vs. small) is shown depending
// on the desktop resolution.
TEST_F(WallpaperControllerTest, ShowCustomWallpaperWithCorrectResolution) {
  CreateDefaultWallpapers();
  const base::FilePath small_custom_wallpaper_path =
      GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                             kWallpaperFilesId1, kFileName1);
  const base::FilePath large_custom_wallpaper_path =
      GetCustomWallpaperPath(WallpaperControllerImpl::kLargeWallpaperSubDir,
                             kWallpaperFilesId1, kFileName1);
  const base::FilePath small_default_wallpaper_path =
      default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName);
  const base::FilePath large_default_wallpaper_path =
      default_wallpaper_dir_.GetPath().Append(kDefaultLargeWallpaperName);

  CreateAndSaveWallpapers(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  // Display is initialized to 800x600. The small resolution custom wallpaper is
  // expected. A second decode request with small resolution default wallpaper
  // is also expected. (Because unit tests don't support actual wallpaper
  // decoding, it falls back to the default wallpaper.)
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(2u, GetDecodeFilePaths().size());
  EXPECT_EQ(small_custom_wallpaper_path, GetDecodeFilePaths()[0]);
  EXPECT_EQ(small_default_wallpaper_path, GetDecodeFilePaths()[1]);

  // Hook up another 800x600 display. This shouldn't trigger a reload.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,800x600");
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(0u, GetDecodeFilePaths().size());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,3000x2000");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(2u, GetDecodeFilePaths().size());
  EXPECT_EQ(large_custom_wallpaper_path, GetDecodeFilePaths()[0]);
  EXPECT_EQ(large_default_wallpaper_path, GetDecodeFilePaths()[1]);

  // Detach the secondary display.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  // Hook up the 3000x2000 display again. The large resolution default wallpaper
  // should persist. Test for crbug/165788.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,3000x2000");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(2u, GetDecodeFilePaths().size());
  EXPECT_EQ(large_custom_wallpaper_path, GetDecodeFilePaths()[0]);
  EXPECT_EQ(large_default_wallpaper_path, GetDecodeFilePaths()[1]);
}

// After the display is rotated, the sign in wallpaper should be kept. Test for
// crbug.com/794725.
TEST_F(WallpaperControllerTest, SigninWallpaperIsKeptAfterRotation) {
  CreateDefaultWallpapers();

  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  controller_->ShowSigninWallpaper();
  RunAllTasksUntilIdle();
  // Display is initialized to 800x600. The small resolution default wallpaper
  // is expected.
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);

  ClearWallpaperCount();
  ClearDecodeFilePaths();
  // After rotating the display, the small resolution default wallpaper should
  // still be expected, instead of a custom wallpaper.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);
}

// Display size change should trigger wallpaper reload.
TEST_F(WallpaperControllerTest, ReloadWallpaper) {
  CreateAndSaveWallpapers(kAccountId1);

  // Show a user wallpaper.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  // Rotating the display should trigger a wallpaper reload.
  ClearWallpaperCount();
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  // Calling |ShowUserWallpaper| again with the same account id and display
  // size should not trigger wallpaper reload (crbug.com/158383).
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());

  // Start wallpaper preview.
  SimulateUserLogin(kAccountId1);
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,

      /*preview_mode=*/true, base::DoNothing(), /*file_path=*/"",
      CreateImage(640, 480, kWallpaperColor));
  ClearWallpaperCount();
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());

  // Show an override wallpaper.
  const base::FilePath image_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kGuestWallpaperLarge);
  CreateDefaultWallpapers();
  SetBypassDecode();
  ClearWallpaperCount();
  controller_->ShowOverrideWallpaper(image_path, /*always_on_top=*/true);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  // Rotating the display should trigger a wallpaper reload.
  ClearWallpaperCount();
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, UpdateCurrentWallpaperLayout) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kSmallCustomWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_STRETCH;
  WallpaperLayout new_layout = WALLPAPER_LAYOUT_CENTER;
  SimulateUserLogin(kAccountId1);

  // Set a custom wallpaper for the user. Verify that it's set successfully
  // and the wallpaper info is updated.
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/false,
                                         base::DoNothing(),
                                         /*file_path=*/"", image);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_custom_wallpaper_info(
      base::FilePath(kWallpaperFilesId1).Append(kFileName1).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_custom_wallpaper_info));

  // Now change to a different layout. Verify that the layout is updated for
  // both the current wallpaper and the saved wallpaper info.
  ClearWallpaperCount();
  controller_->UpdateCurrentWallpaperLayout(kAccountId1, new_layout);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), new_layout);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  expected_custom_wallpaper_info.layout = new_layout;
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_custom_wallpaper_info));

  {
    // Now set a Google Photos wallpaper. Verify that it's set successfully and
    // the wallpaper info is updated.
    ClearWallpaperCount();
    controller_->SetGooglePhotosWallpaper(
        GooglePhotosWallpaperParams(kAccountId1, "id",
                                    /*daily_refresh_enabled=*/false, layout,
                                    /*preview_mode=*/false, "dedup_key"),
        base::DoNothing());
    RunAllTasksUntilIdle();
    EXPECT_EQ(1, GetWallpaperCount());
    EXPECT_EQ(controller_->GetWallpaperType(),
              WallpaperType::kOnceGooglePhotos);
    EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(
        WallpaperInfo(GooglePhotosWallpaperParams(
            kAccountId1, "id", /*daily_refresh_enabled=*/false, layout,
            /*preview_mode=*/false, "dedup_key"))));

    // Now change to a different layout. Verify that the layout is updated for
    // both the current wallpaper and the saved wallpaper info.
    ClearWallpaperCount();
    controller_->UpdateCurrentWallpaperLayout(kAccountId1, new_layout);
    RunAllTasksUntilIdle();
    EXPECT_EQ(1, GetWallpaperCount());
    EXPECT_EQ(controller_->GetWallpaperLayout(), new_layout);
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(
        WallpaperInfo(GooglePhotosWallpaperParams(
            kAccountId1, "id", /*daily_refresh_enabled=*/false, new_layout,
            /*preview_mode=*/false, "dedup_key"))));
  }

  // Now set an online wallpaper. Verify that it's set successfully and the
  // wallpaper info is updated.
  image = CreateImage(640, 480, kWallpaperColor);
  ClearWallpaperCount();
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            /*collection_id=*/std::string(), layout,
                            /*preview_mode=*/false, /*from_user=*/false,
                            /*daily_refresh_enabled=*/false, kUnitId,
                            /*variants=*/std::vector<OnlineWallpaperVariant>());
  controller_->SetOnlineWallpaper(params, base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_online_wallpaper_info(params);
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_online_wallpaper_info));

  // Now change the layout of the online wallpaper. Verify that it's a no-op.
  ClearWallpaperCount();
  controller_->UpdateCurrentWallpaperLayout(kAccountId1, new_layout);
  RunAllTasksUntilIdle();
  // The wallpaper is not updated.
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  // The saved wallpaper info is not updated.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_online_wallpaper_info));
}

// Tests that if a user who has a custom wallpaper is removed from the device,
// only the directory that contains the user's custom wallpapers gets removed.
// The other user's custom wallpaper is not affected.
TEST_F(WallpaperControllerTest, RemoveUserWithCustomWallpaper) {
  SimulateUserLogin(kAccountId1);
  base::FilePath small_wallpaper_path_1 =
      GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                             kWallpaperFilesId1, kFileName1);

  // Set a custom wallpaper for |kUser1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(kAccountId1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a custom wallpaper for the user.
  SimulateUserLogin(kAccountId2);
  base::FilePath small_wallpaper_path_2 =
      GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                             kWallpaperFilesId2, GetDummyFileName(kAccountId2));
  CreateAndSaveWallpapers(kAccountId2);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_2));

  // Simulate the removal of |kUser2|.
  controller_->RemoveUserWallpaper(kAccountId2, base::DoNothing());
  // Wait until all files under the user's custom wallpaper directory are
  // removed.
  WaitUntilCustomWallpapersDeleted(kAccountId2);
  EXPECT_FALSE(base::PathExists(small_wallpaper_path_2));

  // Verify that the other user's wallpaper is not affected.
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));
}

// Tests that if a user who has a default wallpaper is removed from the device,
// the other user's custom wallpaper is not affected.
TEST_F(WallpaperControllerTest, RemoveUserWithDefaultWallpaper) {
  SimulateUserLogin(kAccountId1);
  base::FilePath small_wallpaper_path_1 =
      GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                             kWallpaperFilesId1, kFileName1);
  // Set a custom wallpaper for |kUser1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(kAccountId1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a default wallpaper for the user.
  SimulateUserLogin(kAccountId2);
  controller_->SetDefaultWallpaper(kAccountId2, true /*show_wallpaper=*/,
                                   base::DoNothing());

  // Simulate the removal of |kUser2|.
  controller_->RemoveUserWallpaper(kAccountId2,
                                   /*on_removed=*/base::DoNothing());

  // Verify that the other user's wallpaper is not affected.
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));
}

// Tests that when a user who has a default wallpaper is removed from the
// device, the `on_remove` callback is called.
TEST_F(WallpaperControllerTest, RemoveUserWallpaperOnRemoveCallbackCalled) {
  SimulateUserLogin(kAccountId1);
  controller_->SetDefaultWallpaper(kAccountId1, /*show_wallpaper=*/true,
                                   /*callback=*/base::DoNothing());

  base::test::TestFuture<void> remove_was_called;

  // Simulate the removal of |kUser1|.
  controller_->RemoveUserWallpaper(kAccountId1,
                                   remove_was_called.GetCallback());

  // Assert that the `on_remove` callback is called
  ASSERT_TRUE(remove_was_called.Wait());
}

TEST_F(WallpaperControllerTest, IsActiveUserWallpaperControlledByPolicy) {
  SetBypassDecode();
  // Simulate the login screen. Verify that it returns false since there's no
  // active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  SimulateUserLogin(kAccountId1);
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());
  // Set a policy wallpaper for the active user. Verify that the active user
  // becomes policy controlled.
  controller_->SetPolicyWallpaper(kAccountId1, user_manager::USER_TYPE_REGULAR,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Switch the active user. Verify the active user is not policy controlled.
  SimulateUserLogin(kAccountId2);
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Logs out. Verify that it returns false since there's no active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());
}

TEST_F(WallpaperControllerTest,
       IsManagedGuestSessionWallpaperControlledByPolicy) {
  SetBypassDecode();
  // Simulate the login screen. Verify that it returns false since there's no
  // active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Set a policy wallpaper for the managed guest session. Verify that the
  // managed guest session becomes policy controlled.
  controller_->SetPolicyWallpaper(kAccountId1,
                                  user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                  std::string() /*data=*/);
  SimulateUserLogin(kAccountId1, user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId1));

  // Verify the wallpaper policy is applied after logging in.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kPolicy);

  // Switch the active user. Verify the active user is not policy controlled.
  SimulateUserLogin(kAccountId2);
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Logs out. Verify that it returns false since there's no active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());
}

TEST_F(WallpaperControllerTest, WallpaperBlur) {
  TestWallpaperControllerObserver observer(controller_);

  ASSERT_TRUE(controller_->blur_manager()->IsBlurAllowedForLockState(
      controller_->GetWallpaperType()));
  ASSERT_FALSE(controller_->IsWallpaperBlurredForLockState());

  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(0, observer.blur_changed_count());

  SetSessionState(SessionState::LOCKED);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(1, observer.blur_changed_count());

  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(2, observer.blur_changed_count());

  SetSessionState(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(3, observer.blur_changed_count());

  // Blur state does not change below.
  SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(3, observer.blur_changed_count());

  SetSessionState(SessionState::OOBE);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(3, observer.blur_changed_count());

  SetSessionState(SessionState::UNKNOWN);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(3, observer.blur_changed_count());
}

TEST_F(WallpaperControllerTest, WallpaperBlurDuringLockScreenTransition) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  gfx::ImageSkia image = CreateImage(600, 400, kWallpaperColor);
  controller_->ShowWallpaperImage(
      image, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
      /*preview_mode=*/false, /*is_override=*/false);

  TestWallpaperControllerObserver observer(controller_);

  ASSERT_TRUE(controller_->blur_manager()->IsBlurAllowedForLockState(
      controller_->GetWallpaperType()));
  ASSERT_FALSE(controller_->IsWallpaperBlurredForLockState());

  ASSERT_EQ(2u, wallpaper_view()->layer()->parent()->children().size());
  EXPECT_EQ(ui::LAYER_TEXTURED,
            wallpaper_view()->layer()->parent()->children()[0]->type());
  EXPECT_EQ(ui::LAYER_TEXTURED,
            wallpaper_view()->layer()->parent()->children()[1]->type());

  // Simulate lock and unlock sequence.
  controller_->UpdateWallpaperBlurForLockState(true);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(1, observer.blur_changed_count());

  SetSessionState(SessionState::LOCKED);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  ASSERT_EQ(3u, wallpaper_view()->layer()->parent()->children().size());
  EXPECT_EQ(ui::LAYER_SOLID_COLOR,
            wallpaper_view()->layer()->parent()->children()[0]->type());
  EXPECT_EQ(ui::LAYER_TEXTURED,
            wallpaper_view()->layer()->parent()->children()[1]->type());
  EXPECT_EQ(ui::LAYER_TEXTURED,
            wallpaper_view()->layer()->parent()->children()[2]->type());

  // Change of state to ACTIVE triggers post lock animation and
  // UpdateWallpaperBlur(false)
  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(2, observer.blur_changed_count());
  ASSERT_EQ(2u, wallpaper_view()->layer()->parent()->children().size());
  EXPECT_EQ(ui::LAYER_TEXTURED,
            wallpaper_view()->layer()->parent()->children()[0]->type());
  EXPECT_EQ(ui::LAYER_TEXTURED,
            wallpaper_view()->layer()->parent()->children()[1]->type());
}

TEST_F(WallpaperControllerTest, LockDuringOverview) {
  gfx::ImageSkia image = CreateImage(600, 400, kWallpaperColor);
  controller_->ShowWallpaperImage(
      image, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
      /*preview_mode=*/false, /*is_override=*/false);
  TestWallpaperControllerObserver observer(controller_);

  EnterOverview();

  EXPECT_FALSE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(0, observer.blur_changed_count());

  // Simulate lock and unlock sequence.
  SetSessionState(SessionState::LOCKED);

  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());

  // Get wallpaper_view directly because it's not animating.
  auto* wallpaper_view = Shell::Get()
                             ->GetPrimaryRootWindowController()
                             ->wallpaper_widget_controller()
                             ->wallpaper_view();

  // Make sure that wallpaper still have blur.
  ASSERT_EQ(30, wallpaper_view->blur_sigma());
}

TEST_F(WallpaperControllerTest, DontLeakShieldView) {
  SetSessionState(SessionState::LOCKED);
  views::View* shield_view = wallpaper_view()->shield_view_for_testing();
  ASSERT_TRUE(shield_view);
  views::ViewTracker view_tracker(shield_view);
  SetSessionState(SessionState::ACTIVE);
  EXPECT_EQ(nullptr, wallpaper_view()->shield_view_for_testing());
  EXPECT_EQ(nullptr, view_tracker.view());
}

TEST_F(WallpaperControllerTest, OnlyShowDevicePolicyWallpaperOnLoginScreen) {
  SetBypassDecode();

  // Verify the device policy wallpaper is shown on login screen.
  SetSessionState(SessionState::LOGIN_PRIMARY);
  controller_->SetDevicePolicyWallpaperPath(
      base::FilePath(kDefaultSmallWallpaperName));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_TRUE(IsDevicePolicyWallpaper());
  // Verify the device policy wallpaper shouldn't be blurred.
  ASSERT_FALSE(controller_->blur_manager()->IsBlurAllowedForLockState(
      controller_->GetWallpaperType()));
  ASSERT_FALSE(controller_->IsWallpaperBlurredForLockState());

  // Verify the device policy wallpaper is replaced when session state is no
  // longer LOGIN_PRIMARY.
  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_FALSE(IsDevicePolicyWallpaper());

  // Verify the device policy wallpaper never shows up again when session
  // state changes.
  SetSessionState(SessionState::ACTIVE);
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_FALSE(IsDevicePolicyWallpaper());

  SetSessionState(SessionState::LOCKED);
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_FALSE(IsDevicePolicyWallpaper());

  SetSessionState(SessionState::LOGIN_SECONDARY);
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_FALSE(IsDevicePolicyWallpaper());
}

TEST_F(WallpaperControllerTest, ShouldShowInitialAnimationAfterBoot) {
  CreateDefaultWallpapers();

  // Simulate the login screen after system boot.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFirstExecAfterBoot);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kLoginManager);
  ClearLogin();

  // Show the first wallpaper. Verify that the slower animation should be used.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Show the second wallpaper. Verify that the slower animation should not be
  // used. (Use a different user type to ensure a different wallpaper is shown,
  // otherwise requests of loading the same wallpaper are ignored.)
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kChildAccountId);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Log in the user and show the wallpaper. Verify that the slower animation
  // should not be used.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, ShouldNotShowInitialAnimationAfterSignOut) {
  CreateDefaultWallpapers();

  // Simulate the login screen after user sign-out. Verify that the slower
  // animation should never be used.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kLoginManager);
  CreateAndSaveWallpapers(kAccountId1);
  ClearLogin();

  // Show the first wallpaper.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Show the second wallpaper.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kChildAccountId);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Log in the user and show the wallpaper.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, ClosePreviewWallpaperOnOverviewStart) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  ClearWallpaperCount();

  TestWallpaperControllerObserver observer(controller_);
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/true,
                                         base::DoNothing(),
                                         /*file_path=*/"", custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(observer.is_in_wallpaper_preview());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Now enter overview mode. Verify the wallpaper changes back to the default,
  // the user wallpaper info remains unchanged, and enters overview mode
  // properly.
  ClearWallpaperCount();
  EnterOverview();
  RunAllTasksUntilIdle();
  EXPECT_FALSE(observer.is_in_wallpaper_preview());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(WallpaperControllerTest, ClosePreviewWallpaperOnWindowCycleStart) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  TestWallpaperControllerObserver observer(controller_);

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/true,
                                         base::DoNothing(),
                                         /*file_path=*/"", custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(observer.is_in_wallpaper_preview());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Now start window cycle. Verify the wallpaper changes back to the default,
  // the user wallpaper info remains unchanged, and enters window cycle.
  ClearWallpaperCount();
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(observer.is_in_wallpaper_preview());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
  EXPECT_TRUE(Shell::Get()->window_cycle_controller()->IsCycling());
}

TEST_F(WallpaperControllerTest,
       ClosePreviewWallpaperOnActiveUserSessionChanged) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  TestWallpaperControllerObserver observer(controller_);

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/true,
                                         base::DoNothing(),
                                         /*file_path=*/"", custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(observer.is_in_wallpaper_preview());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Now switch to another user. Verify the wallpaper changes back to the
  // default and the user wallpaper remains unchanged.
  ClearWallpaperCount();
  SimulateUserLogin(kAccountId2);
  controller_->ShowUserWallpaper(kAccountId2);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(observer.is_in_wallpaper_preview());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId2, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
}

TEST_F(WallpaperControllerTest, ConfirmPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/true,
                                         base::DoNothing(),
                                         /*file_path=*/"", custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Preview.Show", 1);

  // Now confirm the preview wallpaper, verify that there's no wallpaper change
  // because the wallpaper is already shown.
  ClearWallpaperCount();
  controller_->ConfirmPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());

  // Verify that the user wallpaper info is now updated to the custom wallpaper
  // info.
  WallpaperInfo custom_wallpaper_info(
      base::FilePath(kWallpaperFilesId1).Append(kFileName1).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(custom_wallpaper_info));

  // Set an empty online wallpaper for the user, verify it fails.
  ClearWallpaperCount();
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetOnlineWallpaperFromImage(
      kAccountId1, kAssetId, gfx::ImageSkia(), kDummyUrl,
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*save_file=*/false, /*preview_mode=*/true, /*from_user=*/true, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>(),
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());

  // Now set a valid online wallpaper for the user and enable preview. Verify
  // that the wallpaper is changed to the expected color.
  const SkColor online_wallpaper_color = SK_ColorCYAN;
  gfx::ImageSkia online_wallpaper =
      CreateImage(640, 480, online_wallpaper_color);
  EXPECT_NE(online_wallpaper_color, GetWallpaperColor());
  run_loop = std::make_unique<base::RunLoop>();
  SetOnlineWallpaperFromImage(
      kAccountId1, kAssetId, online_wallpaper, kDummyUrl,
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*save_file=*/false, /*preview_mode=*/true, /*from_user=*/true, kUnitId,
      /*variants=*/
      std::vector<OnlineWallpaperVariant>(),
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(online_wallpaper_color, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(custom_wallpaper_info));

  // Now confirm the preview wallpaper, verify that there's no wallpaper change
  // because the wallpaper is already shown.
  ClearWallpaperCount();
  controller_->ConfirmPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(online_wallpaper_color, GetWallpaperColor());
  // Verify that the user wallpaper info is now updated to the online wallpaper
  // info.
  WallpaperInfo online_wallpaper_info(OnlineWallpaperParams(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*preview_mode=*/false,
      /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/
      std::vector<OnlineWallpaperVariant>()));
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(online_wallpaper_info));
}

TEST_F(WallpaperControllerTest, CancelPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/true,
                                         base::DoNothing(),
                                         /*file_path=*/"", custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Now cancel the preview. Verify the wallpaper changes back to the default
  // and the user wallpaper info remains unchanged.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Now set an online wallpaper for the user and enable preview. Verify that
  // the wallpaper is changed to the expected color.
  const SkColor online_wallpaper_color = SK_ColorCYAN;
  gfx::ImageSkia online_wallpaper =
      CreateImage(640, 480, online_wallpaper_color);
  EXPECT_NE(online_wallpaper_color, GetWallpaperColor());
  ClearWallpaperCount();
  SetOnlineWallpaperFromImage(
      kAccountId1, kAssetId, online_wallpaper, kDummyUrl,
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*save_file=*/false, /*preview_mode=*/true, /*from_user=*/true, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>(),
      WallpaperController::SetWallpaperCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(online_wallpaper_color, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Now cancel the preview. Verify the wallpaper changes back to the default
  // and the user wallpaper info remains unchanged.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(online_wallpaper_color, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
}

TEST_F(WallpaperControllerTest, WallpaperSyncedDuringPreview) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/true,
                                         base::DoNothing(),
                                         /*file_path=*/"", custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Now set another custom wallpaper for the user and disable preview (this
  // happens if a custom wallpaper set on another device is being synced).
  // Verify there's no wallpaper change since preview mode shouldn't be
  // interrupted.
  const SkColor synced_custom_wallpaper_color = SK_ColorBLUE;
  gfx::ImageSkia synced_custom_wallpaper =
      CreateImage(640, 480, synced_custom_wallpaper_color);
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName2, layout,
      /*preview_mode=*/false, base::DoNothing(),
      /*file_path=*/"", synced_custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // However, the user wallpaper info should already be updated to the new info.
  WallpaperInfo synced_custom_wallpaper_info(
      base::FilePath(kWallpaperFilesId1).Append(kFileName2).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(synced_custom_wallpaper_info));

  // Now cancel the preview. Verify the synced custom wallpaper is shown instead
  // of the initial default wallpaper, and the user wallpaper info is still
  // correct.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(synced_custom_wallpaper_color, GetWallpaperColor());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(synced_custom_wallpaper_info));

  // Repeat the above steps for online wallpapers: set a online wallpaper for
  // the user and enable preview. Verify that the wallpaper is changed to the
  // expected color.
  gfx::ImageSkia online_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());

  ClearWallpaperCount();
  SetOnlineWallpaperFromImage(
      kAccountId1, kAssetId, online_wallpaper, kDummyUrl,
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*save_file=*/false, /*preview_mode=*/true, /*from_user=*/true, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>(),
      WallpaperController::SetWallpaperCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(synced_custom_wallpaper_info));

  // Now set another online wallpaper for the user and disable preview. Verify
  // there's no wallpaper change since preview mode shouldn't be interrupted.
  const SkColor synced_online_wallpaper_color = SK_ColorCYAN;
  gfx::ImageSkia synced_online_wallpaper =
      CreateImage(640, 480, synced_online_wallpaper_color);
  ClearWallpaperCount();
  SetOnlineWallpaperFromImage(
      kAccountId1, kAssetId, synced_online_wallpaper, kDummyUrl2,
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*save_file=*/false, /*preview_mode=*/false,
      /*from_user=*/true, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>(),
      WallpaperController::SetWallpaperCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // However, the user wallpaper info should already be updated to the new info.
  WallpaperInfo synced_online_wallpaper_info =
      WallpaperInfo(OnlineWallpaperParams(
          kAccountId1, kAssetId, GURL(kDummyUrl2),
          TestWallpaperControllerClient::kDummyCollectionId, layout,
          /*preview_mode=*/false,
          /*from_user=*/true,
          /*daily_refresh_enabled=*/false, kUnitId,
          /*variants=*/
          std::vector<OnlineWallpaperVariant>()));
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(synced_online_wallpaper_info));

  // Now cancel the preview. Verify the synced online wallpaper is shown instead
  // of the previous custom wallpaper, and the user wallpaper info is still
  // correct.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(synced_online_wallpaper_color, GetWallpaperColor());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(synced_online_wallpaper_info));
}

TEST_F(WallpaperControllerTest, AddFirstWallpaperAnimationEndCallback) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<aura::Window> test_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));

  base::RunLoop test_loop;
  controller_->AddFirstWallpaperAnimationEndCallback(test_loop.QuitClosure(),
                                                     test_window.get());
  // The callback is not run because the first wallpaper hasn't been set.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(test_loop.AnyQuitCalled());

  // Set the first wallpaper.
  controller_->ShowDefaultWallpaperForTesting();
  controller_->AddFirstWallpaperAnimationEndCallback(test_loop.QuitClosure(),
                                                     test_window.get());
  task_environment()->RunUntilIdle();
  // Neither callback is run because the animation of the first wallpaper
  // hasn't finished yet.
  EXPECT_FALSE(test_loop.AnyQuitCalled());

  // Force the animation to complete. The two callbacks are both run.
  RunDesktopControllerAnimation();
  test_loop.Run();
  EXPECT_TRUE(test_loop.AnyQuitCalled());

  // The callback added after the first wallpaper animation is run right away.
  bool is_third_callback_run = false;
  controller_->AddFirstWallpaperAnimationEndCallback(
      base::BindLambdaForTesting(
          [&is_third_callback_run]() { is_third_callback_run = true; }),
      test_window.get());
  EXPECT_TRUE(is_third_callback_run);
}

TEST_F(WallpaperControllerTest, ShowOneShotWallpaper) {
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(kAccountId1);
  // First, set a custom wallpaper for |kUser1|. Verify the wallpaper is shown
  // successfully and the user wallpaper info is updated.
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/false,
                                         base::DoNothing(),
                                         /*file_path=*/"", custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::kCustomized, controller_->GetWallpaperType());
  const WallpaperInfo expected_wallpaper_info(
      base::FilePath(kWallpaperFilesId1).Append(kFileName1).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));

  // Show a one-shot wallpaper. Verify it is shown successfully.
  ClearWallpaperCount();
  constexpr SkColor kOneShotWallpaperColor = SK_ColorWHITE;
  gfx::ImageSkia one_shot_wallpaper =
      CreateImage(640, 480, kOneShotWallpaperColor);
  controller_->ShowOneShotWallpaper(one_shot_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kOneShotWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::kOneShot, controller_->GetWallpaperType());
  EXPECT_FALSE(controller_->blur_manager()->IsBlurAllowedForLockState(
      controller_->GetWallpaperType()));
  EXPECT_FALSE(controller_->ShouldApplyShield());

  // Verify that we can reload wallpaer without losing it.
  // This is important for screen rotation.
  controller_->ReloadWallpaperForTesting(/*clear_cache=*/false);
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());  // Reload increments count.
  EXPECT_EQ(kOneShotWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::kOneShot, controller_->GetWallpaperType());
  EXPECT_FALSE(controller_->blur_manager()->IsBlurAllowedForLockState(
      controller_->GetWallpaperType()));
  EXPECT_FALSE(controller_->ShouldApplyShield());

  // Verify the user wallpaper info is unaffected, and the one-shot wallpaper
  // can be replaced by the user wallpaper.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::kCustomized, controller_->GetWallpaperType());
}

TEST_F(WallpaperControllerTest, OnFirstWallpaperShown) {
  TestWallpaperControllerObserver observer(controller_);
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(0, observer.first_shown_count());
  // Show the first wallpaper, verify the observer is notified.
  controller_->ShowWallpaperImage(CreateImage(640, 480, SK_ColorBLUE),
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  /*preview_mode=*/false,
                                  /*is_override=*/false);
  RunAllTasksUntilIdle();
  EXPECT_EQ(SK_ColorBLUE, GetWallpaperColor());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(1, observer.first_shown_count());
  // Show the second wallpaper, verify the observer is not notified.
  controller_->ShowWallpaperImage(CreateImage(640, 480, SK_ColorCYAN),
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  /*preview_mode=*/false,
                                  /*is_override=*/false);
  RunAllTasksUntilIdle();
  EXPECT_EQ(SK_ColorCYAN, GetWallpaperColor());
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_EQ(1, observer.first_shown_count());
}

// Although ephemeral users' custom wallpapers are not saved to disk, they
// should be kept within the user session. Test for https://crbug.com/825237.
TEST_F(WallpaperControllerTest, ShowWallpaperForEphemeralUser) {
  // Clear the local pref so we can make sure nothing writes to it.
  local_state()->ClearPref(prefs::kUserWallpaperInfo);

  // Add an ephemeral user session and simulate login, like SimulateUserLogin.
  UserSession session;
  session.session_id = 0;
  session.user_info.account_id = kAccountId1;
  session.user_info.is_ephemeral = true;
  Shell::Get()->session_controller()->UpdateUserSession(std::move(session));
  TestSessionControllerClient* const client = GetSessionControllerClient();
  client->SwitchActiveUser(kAccountId1);
  client->SetSessionState(SessionState::ACTIVE);

  // The user doesn't have wallpaper cache in the beginning.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  base::FilePath path;
  EXPECT_FALSE(controller_->GetPathFromCache(kAccountId1, &path));

  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"",
      CreateImage(640, 480, kWallpaperColor));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(WallpaperType::kCustomized, controller_->GetWallpaperType());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Assert that we do not use local state for an ephemeral user.
  auto* dict = local_state()->GetUserPrefValue(prefs::kUserWallpaperInfo);
  ASSERT_FALSE(dict) << *dict;

  // The custom wallpaper is cached.
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
  EXPECT_EQ(
      kWallpaperColor,
      cached_wallpaper.GetRepresentation(1.0f).GetBitmap().getColor(0, 0));
  EXPECT_TRUE(controller_->GetPathFromCache(kAccountId1, &path));

  // Calling |ShowUserWallpaper| will continue showing the custom wallpaper
  // instead of reverting to the default.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(WallpaperType::kCustomized, controller_->GetWallpaperType());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
}

// Base class for `WallpaperControllerTest` parameterized by device properties
// which OOBE wallpaper flow should be used
class WallpaperControllerOobeWallpaperTest
    : public WallpaperControllerTest,
      public testing::WithParamInterface<
          std::tuple</*OobeSimon*/ bool, /*OobeJelly*/ bool>> {
 public:
  WallpaperControllerOobeWallpaperTest() {
    std::vector<base::test::FeatureRef> EnabledFeatures;
    std::vector<base::test::FeatureRef> DisabledFeatures;

    bool OobeSimon = std::get<0>(GetParam());
    if (OobeSimon) {
      EnabledFeatures.push_back(ash::features::kFeatureManagementOobeSimon);
      EnabledFeatures.push_back(ash::features::kOobeSimon);
    } else {
      DisabledFeatures.push_back(ash::features::kFeatureManagementOobeSimon);
      DisabledFeatures.push_back(ash::features::kOobeSimon);
    }

    bool OobeJelly = std::get<1>(GetParam());
    if (OobeJelly) {
      EnabledFeatures.push_back(ash::features::kOobeJelly);
      EnabledFeatures.push_back(chromeos::features::kJelly);
    } else {
      DisabledFeatures.push_back(ash::features::kOobeJelly);
      DisabledFeatures.push_back(chromeos::features::kJelly);
    }

    scoped_feature_list_.InitWithFeatures(EnabledFeatures, DisabledFeatures);
  }
  ~WallpaperControllerOobeWallpaperTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WallpaperControllerOobeWallpaperTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_P(WallpaperControllerOobeWallpaperTest, ShowOobeWallpaper) {
  SetBypassDecode();

  controller_->ShowDefaultWallpaperForTesting();
  RunAllTasksUntilIdle();

  // Verify the OOBE wallpaper is shown during OOBE.
  SetSessionState(SessionState::OOBE);
  controller_->ReloadWallpaperForTesting(/*clear_cache=*/false);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsOobeWallpaper());

  controller_->ShowSigninWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsOobeWallpaper());

  // Verify the OOBE wallpaper is replaced when session state is no
  // longer OOBE.
  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->IsOobeWallpaper());

  // Verify the OOBE wallpaper never shows up again when session
  // state changes.
  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->IsOobeWallpaper());
  SetSessionState(SessionState::ACTIVE);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->IsOobeWallpaper());

  SetSessionState(SessionState::LOCKED);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->IsOobeWallpaper());

  SetSessionState(SessionState::LOGIN_SECONDARY);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->IsOobeWallpaper());
}

// Base class for `WallpaperControllerTest` parameterized by whether override
// wallpapers should be shown on top of everything except for the power off
// animation.
class WallpaperControllerOverrideWallpaperTest
    : public WallpaperControllerTest,
      public testing::WithParamInterface</*always_on_top=*/bool> {
 public:
  // Returns whether override wallpapers should be shown on top of everything
  // except for the power off animation given test parameterization.
  bool always_on_top() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         WallpaperControllerOverrideWallpaperTest,
                         /*always_on_top=*/testing::Bool());

TEST_P(WallpaperControllerOverrideWallpaperTest, OverrideWallpaper) {
  CreateDefaultWallpapers();
  SetBypassDecode();

  // Show a default wallpaper.
  EXPECT_EQ(0, GetWallpaperCount());
  controller_->ShowSigninWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // Show an override wallpaper.
  const base::FilePath image_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kGuestWallpaperLarge);
  controller_->ShowOverrideWallpaper(image_path, always_on_top());
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOneShot);
  EXPECT_EQ(always_on_top() ? 0 : 1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(always_on_top() ? 1 : 0,
            ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // Subsequent wallpaper requests are ignored when the current wallpaper is
  // overridden.
  controller_->ShowSigninWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOneShot);
  EXPECT_EQ(always_on_top() ? 0 : 1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(always_on_top() ? 1 : 0,
            ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // The wallpaper reverts to the default after the override wallpaper is
  // removed.
  controller_->RemoveOverrideWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(3, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // Calling |RemoveOverrideWallpaper()| is a no-op when the current wallpaper
  // is not overridden.
  controller_->RemoveOverrideWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(3, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kAlwaysOnTopWallpaperId));
}

namespace {

class WallpaperControllerPrefTest : public AshTestBase {
 public:
  WallpaperControllerPrefTest() {
    base::Value::Dict property;
    property.Set("rotation", static_cast<int>(display::Display::ROTATE_90));
    property.Set("width", 800);
    property.Set("height", 600);

    ScopedDictPrefUpdate update(local_state(), prefs::kDisplayProperties);
    update->Set("2200000000", std::move(property));
  }

  ~WallpaperControllerPrefTest() override = default;

  void SetUp() override { AshTestBase::SetUp(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Make sure that the display and the wallpaper view are rotated correctly at
// startup.
TEST_F(WallpaperControllerPrefTest, InitWithPrefs) {
  auto* wallpaper_view = Shell::GetPrimaryRootWindowController()
                             ->wallpaper_widget_controller()
                             ->wallpaper_view();
  auto* root_window =
      wallpaper_view->GetWidget()->GetNativeWindow()->GetRootWindow();

  EXPECT_EQ(gfx::Size(600, 800), display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(root_window)
                                     .size());
  EXPECT_EQ(root_window->bounds().size(), wallpaper_view->bounds().size());
}

TEST_F(WallpaperControllerTest, NoAnimationForNewRootWindowWhenLocked) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  SetSessionState(SessionState::LOCKED);
  UpdateDisplay("800x600, 800x600");
  auto* secondary_root_window_controller =
      Shell::Get()->GetAllRootWindowControllers()[1];
  EXPECT_FALSE(secondary_root_window_controller->wallpaper_widget_controller()
                   ->IsAnimating());
  EXPECT_FALSE(secondary_root_window_controller->wallpaper_widget_controller()
                   ->GetWidget()
                   ->GetLayer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(WallpaperControllerTest, SetCustomWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(kAccountId1);

  // Set a custom wallpaper for |kUser1|. Verify the wallpaper is set
  // successfully and wallpaper info is updated.
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/false,
                                         base::DoNothing(),
                                         /*file_path=*/"", image);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kCustomized);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(
      base::FilePath(kWallpaperFilesId1).Append(kFileName1).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
  EXPECT_EQ(kAccountId1, drivefs_delegate_->get_save_wallpaper_account_id());

  // Now set another custom wallpaper for |kUser1|. Verify that the on-screen
  // wallpaper doesn't change since |kUser1| is not active, but wallpaper info
  // is updated properly.
  SimulateUserLogin(kAccountId2);
  const SkColor custom_wallpaper_color = SK_ColorCYAN;
  image = CreateImage(640, 480, custom_wallpaper_color);
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/false,
                                         base::DoNothing(),
                                         /*file_path=*/"", image);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));

  // Verify the updated wallpaper is shown after |kUser1| becomes active again.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(custom_wallpaper_color, GetWallpaperColor());
}

TEST_F(WallpaperControllerTest, OldOnlineInfoSynced_Discarded) {
  // Create a dictionary that looks like the preference from crrev.com/a040384.
  // DO NOT CHANGE as there are preferences like this in production.
  base::Value::Dict wallpaper_info_dict;
  wallpaper_info_dict.Set(
      WallpaperPrefManager::kNewWallpaperDateNodeName,
      base::NumberToString(
          base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds()));
  wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperLocationNodeName,
                          "location");
  wallpaper_info_dict.Set(
      WallpaperPrefManager::kNewWallpaperUserFilePathNodeName,
      "user_file_path");
  wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperLayoutNodeName,
                          WallpaperLayout::WALLPAPER_LAYOUT_CENTER);
  wallpaper_info_dict.Set(WallpaperPrefManager::kNewWallpaperTypeNodeName,
                          static_cast<int>(WallpaperType::kOnline));

  {
    ScopedDictPrefUpdate wallpaper_update(GetProfilePrefService(kAccountId1),
                                          prefs::kSyncableWallpaperInfo);
    wallpaper_update->Set(kAccountId1.GetUserEmail(),
                          std::move(wallpaper_info_dict));
  }
  SimulateUserLogin(kAccountId1);
  task_environment()->RunUntilIdle();

  // Unmigrated synced wallpaper info are discarded.
  WallpaperInfo actual;
  EXPECT_FALSE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual));
}

TEST_F(WallpaperControllerTest, MigrateWallpaperInfo_Online) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kOnline);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, expected_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  EXPECT_TRUE(info.MatchesSelection(expected_info));
}

TEST_F(WallpaperControllerTest, MigrateWallpaperInfoCustomized) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kCustomized);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, expected_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  EXPECT_TRUE(info.MatchesSelection(expected_info));
}

TEST_F(WallpaperControllerTest, MigrateWallpaperInfoDaily) {
  WallpaperInfo expected_info = WallpaperInfo(OnlineWallpaperParams(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId,
      std::vector<OnlineWallpaperVariant>()));
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, expected_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  EXPECT_TRUE(info.MatchesSelection(expected_info));
}

TEST_F(WallpaperControllerTest,
       MigrateWallpaperInfoDoesntHappenWhenSyncedInfoAlreadyExists) {
  SetBypassDecode();

  WallpaperInfo local_info = WallpaperInfo(OnlineWallpaperParams(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId,
      std::vector<OnlineWallpaperVariant>()));
  WallpaperInfo synced_info = WallpaperInfo(OnlineWallpaperParams(
      kAccountId1, kAssetId2, GURL(kDummyUrl2),
      TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId,
      std::vector<OnlineWallpaperVariant>()));
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  // Synced info should be the same if local is the same age.
  EXPECT_TRUE(synced_info.MatchesSelection(info));
}

TEST_F(WallpaperControllerTest,
       ActiveUserPrefServiceChangedSyncedInfoHandledLocally) {
  CacheOnlineWallpaper(kDummyUrl);

  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  synced_info.unit_id = kUnitId;
  synced_info.collection_id = TestWallpaperControllerClient::kDummyCollectionId;
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  WallpaperInfo local_info = InfoWithType(WallpaperType::kThirdParty);
  local_info.date = DayBeforeYesterdayish();
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  client_.ResetCounts();

  controller_->OnActiveUserPrefServiceChanged(
      GetProfilePrefService(kAccountId1));
  RunAllTasksUntilIdle();
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kOnline, actual_info.type);
}

TEST_F(WallpaperControllerTest, ActiveUserPrefServiceChanged_SyncDisabled) {
  CacheOnlineWallpaper(kDummyUrl);
  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  synced_info.unit_id = kUnitId;
  synced_info.collection_id = TestWallpaperControllerClient::kDummyCollectionId;
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  WallpaperInfo local_info = InfoWithType(WallpaperType::kThirdParty);
  local_info.date = DayBeforeYesterdayish();
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  client_.ResetCounts();

  client_.set_wallpaper_sync_enabled(false);

  controller_->OnActiveUserPrefServiceChanged(
      GetProfilePrefService(kAccountId1));
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kThirdParty, actual_info.type);
}

TEST_F(WallpaperControllerTest, HandleWallpaperInfoSyncedLocalIsPolicy) {
  CacheOnlineWallpaper(kDummyUrl);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1,
                                       InfoWithType(WallpaperType::kPolicy));

  SimulateUserLogin(kAccountId1);
  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_NE(WallpaperType::kOnline, actual_info.type);
}

TEST_F(WallpaperControllerTest,
       HandleWallpaperInfoSyncedLocalIsThirdPartyAndOlder) {
  CacheOnlineWallpaper(kDummyUrl);

  WallpaperInfo local_info = InfoWithType(WallpaperType::kThirdParty);
  local_info.date = DayBeforeYesterdayish();
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  SimulateUserLogin(kAccountId1);
  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  synced_info.unit_id = kUnitId;
  synced_info.collection_id = TestWallpaperControllerClient::kDummyCollectionId;
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kOnline, actual_info.type);
}

TEST_F(WallpaperControllerTest,
       HandleWallpaperInfoSyncedLocalIsThirdPartyAndNewer) {
  CacheOnlineWallpaper(kDummyUrl);
  pref_manager_->SetLocalWallpaperInfo(
      kAccountId1, InfoWithType(WallpaperType::kThirdParty));

  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, DayBeforeYesterdayish()};
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  SimulateUserLogin(kAccountId1);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kThirdParty, actual_info.type);
}

TEST_F(WallpaperControllerTest, HandleWallpaperInfoSyncedOnline) {
  CacheOnlineWallpaper(kDummyUrl);

  // Attempt to set an online wallpaper without providing the image data. Verify
  // it succeeds this time because |SetOnlineWallpaper| has saved the file.
  ClearWallpaperCount();
  WallpaperInfo info = WallpaperInfo(OnlineWallpaperParams(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId,
      std::vector<OnlineWallpaperVariant>()));
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, info);

  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
}

TEST_F(WallpaperControllerTest, HandleWallpaperInfoSyncedInactiveUser) {
  CacheOnlineWallpaper(kDummyUrl);

  // Make kAccountId1 the inactive user.
  SimulateUserLogin(kAccountId2);

  // Attempt to set an online wallpaper without providing the image data. Verify
  // it succeeds this time because |SetOnlineWallpaper| has saved the file.
  ClearWallpaperCount();
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1,
                                        InfoWithType(WallpaperType::kOnline));
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_NE(controller_->GetWallpaperType(), WallpaperType::kOnline);
}

TEST_F(WallpaperControllerTest, UpdateDailyRefreshWallpaper) {
  std::string expected{"fun_collection"};
  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kDaily, DayBeforeYesterdayish()};
  info.unit_id = kUnitId;
  info.collection_id = expected;
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaperForTesting();
  EXPECT_EQ(expected, client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_F(WallpaperControllerTest, UpdateDailyRefreshWallpaperCalledOnLogin) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = WallpaperInfo(OnlineWallpaperParams(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
      /*from_user=*/false,
      /*daily_refresh_enabled=*/true, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>()));
  info.date = DayBeforeYesterdayish();
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  ClearLogin();
  SimulateUserLogin(kAccountId1);

  // Info is set as over a day old so we expect one task to run in under an hour
  // (due to fuzzing) then it will idle.
  task_environment()->FastForwardBy(base::Hours(1));
  // Make sure all the tasks such as syncing, setting wallpaper complete.
  RunAllTasksUntilIdle();

  EXPECT_EQ(TestWallpaperControllerClient::kDummyCollectionId,
            client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_F(WallpaperControllerTest, UpdateDailyRefreshWallpaper_NotEnabled) {
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kOnline, DayBeforeYesterdayish()};
  info.collection_id = "fun_collection";
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaperForTesting();
  EXPECT_EQ(std::string(), client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_F(WallpaperControllerTest, UpdateDailyRefreshWallpaper_NoCollectionId) {
  SimulateUserLogin(kAccountId1);
  pref_manager_->SetUserWallpaperInfo(
      kAccountId1,
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER,
                    WallpaperType::kDaily, DayBeforeYesterdayish()));

  controller_->UpdateDailyRefreshWallpaperForTesting();
  EXPECT_EQ(std::string(), client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_F(WallpaperControllerTest,
       UpdateDailyRefreshWallpaper_TimerStartsOnPrefServiceChange) {
  using base::Time;

  SimulateUserLogin(kAccountId1);
  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kDaily,
                        base::Time::Now().LocalMidnight()};
  info.collection_id = "fun_collection";
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->OnActiveUserPrefServiceChanged(
      GetProfilePrefService(kAccountId1));

  Time run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  base::TimeDelta delta = run_time.ToDeltaSinceWindowsEpoch();

  base::TimeDelta update_time =
      Time::Now().LocalMidnight().ToDeltaSinceWindowsEpoch() + base::Days(1);

  ASSERT_GE(delta, update_time - base::Minutes(1));
  ASSERT_LE(delta, update_time + base::Hours(1) + base::Minutes(1));
}

TEST_F(WallpaperControllerTest,
       UpdateDailyRefreshWallpaper_RetryTimerTriggersOnFailedFetchInfo) {
  using base::Time;

  client_.set_fetch_daily_refresh_info_fails(true);

  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kDaily, DayBeforeYesterdayish()};
  info.collection_id = "fun_collection";
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaperForTesting();
  Time run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  base::TimeDelta delay = run_time - Time::Now();

  base::TimeDelta one_hour = base::Hours(1);
  // Lave a little wiggle room.
  ASSERT_GE(delay, one_hour - base::Minutes(1));
  ASSERT_LE(delay, one_hour + base::Minutes(1));
}

TEST_F(WallpaperControllerTest,
       UpdateDailyRefreshWallpaper_RetryTimerTriggersOnFailedFetchData) {
  using base::Time;

  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kDaily, DayBeforeYesterdayish()};
  info.collection_id = "fun_collection";
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  test_wallpaper_image_downloader_->set_image_generator(
      base::BindLambdaForTesting([]() { return gfx::ImageSkia(); }));

  controller_->UpdateDailyRefreshWallpaperForTesting();

  RunAllTasksUntilIdle();

  Time run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  base::TimeDelta delay = run_time - Time::Now();

  base::TimeDelta one_hour = base::Hours(1);
  // Lave a little wiggle room.
  ASSERT_GE(delay, one_hour - base::Minutes(1));
  ASSERT_LE(delay, one_hour + base::Minutes(1));
}

TEST_F(WallpaperControllerTest, MigrateCustomWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(kAccountId1);

  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/false,
                                         base::DoNothing(),
                                         /*file_path=*/"", image);
  RunAllTasksUntilIdle();
  ClearLogin();

  SimulateUserLogin(kAccountId1);
  EXPECT_EQ(kAccountId1, drivefs_delegate_->get_save_wallpaper_account_id());
}

TEST_F(WallpaperControllerTest, OnGoogleDriveMounted) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kCustomized);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  SimulateUserLogin(kAccountId1);
  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_EQ(kAccountId1, drivefs_delegate_->get_save_wallpaper_account_id());
}

TEST_F(WallpaperControllerTest, OnGoogleDriveMounted_WallpaperIsntCustom) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_TRUE(drivefs_delegate_->get_save_wallpaper_account_id().empty());
}

TEST_F(WallpaperControllerTest, OnGoogleDriveMounted_AlreadySynced) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kCustomized);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  SimulateUserLogin(kAccountId1);

  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  controller_->SetDecodedCustomWallpaper(kAccountId1, kFileName1, layout,
                                         /*preview_mode=*/false,
                                         base::DoNothing(),
                                         /*file_path=*/"", image);
  RunAllTasksUntilIdle();

  drivefs_delegate_->Reset();

  // Should not reupload image if it has already been synced.
  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_FALSE(drivefs_delegate_->get_save_wallpaper_account_id().is_valid());
}

TEST_F(WallpaperControllerTest, OnGoogleDriveMounted_OldLocalInfo) {
  WallpaperInfo local_info =
      WallpaperInfo("a_url", WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kCustomized, DayBeforeYesterdayish());
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  WallpaperInfo synced_info = WallpaperInfo(
      "b_url", WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kCustomized,
      base::Time::Now().LocalMidnight());
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  SimulateUserLogin(kAccountId1);

  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_FALSE(drivefs_delegate_->get_save_wallpaper_account_id().is_valid());
}

TEST_F(WallpaperControllerTest, OnGoogleDriveMounted_NewLocalInfo) {
  WallpaperInfo local_info = WallpaperInfo(
      "a_url", WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kCustomized,
      base::Time::Now().LocalMidnight());
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  WallpaperInfo synced_info =
      WallpaperInfo("b_url", WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kCustomized, DayBeforeYesterdayish());
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  SimulateUserLogin(kAccountId1);

  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_EQ(kAccountId1, drivefs_delegate_->get_save_wallpaper_account_id());
}

TEST_F(WallpaperControllerTest,
       SetDailyRefreshCollectionId_UpdatesDailyRefreshTimer) {
  using base::Time;

  pref_manager_->SetUserWallpaperInfo(
      kAccountId1,
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER,
                    WallpaperType::kOnline, DayBeforeYesterdayish()));

  std::string collection_id = "fun_collection";
  controller_->SetDailyRefreshCollectionId(kAccountId1, collection_id);
  WallpaperInfo expected = {std::string(), WALLPAPER_LAYOUT_CENTER,
                            WallpaperType::kDaily, DayBeforeYesterdayish()};
  expected.collection_id = collection_id;

  WallpaperInfo actual;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual);
  // Type should be `WallpaperType::kDaily` now, and collection_id should be
  // updated.
  EXPECT_TRUE(actual.MatchesSelection(expected));
  EXPECT_EQ(collection_id,
            controller_->GetDailyRefreshCollectionId(kAccountId1));

  Time run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  base::TimeDelta delay = run_time - Time::Now();
  base::TimeDelta one_day = base::Days(1);
  // Leave a little wiggle room, as well as account for the hour fuzzing that
  // we do.
  EXPECT_GE(delay, one_day - base::Minutes(1));
  EXPECT_LE(delay, one_day + base::Minutes(61));
}

TEST_F(WallpaperControllerTest, SetDailyRefreshCollectionId_Empty) {
  std::string collection_id = "fun_collection";
  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kDaily, DayBeforeYesterdayish()};
  info.collection_id = collection_id;
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->SetDailyRefreshCollectionId(kAccountId1, std::string());
  WallpaperInfo expected = {std::string(), WALLPAPER_LAYOUT_CENTER,
                            WallpaperType::kOnline, DayBeforeYesterdayish()};
  expected.collection_id = collection_id;

  WallpaperInfo actual;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual);
  // Type should be `WallpaperType::kOnline` now, and collection_id should be
  // `WallpaperType::EMPTY`.
  EXPECT_TRUE(actual.MatchesSelection(expected));
  EXPECT_EQ(std::string(),
            controller_->GetDailyRefreshCollectionId(kAccountId1));
}

// WallpaperType should not change with an empty collection id if the previous
// WallpaperType isn't |WallpaperType::kDaily|.
TEST_F(WallpaperControllerTest,
       SetDailyRefreshCollectionId_Empty_NotTypeDaily) {
  pref_manager_->SetUserWallpaperInfo(
      kAccountId1,
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER,
                    WallpaperType::kCustomized, DayBeforeYesterdayish()));

  controller_->SetDailyRefreshCollectionId(kAccountId1, std::string());
  WallpaperInfo expected =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER,
                    WallpaperType::kCustomized, DayBeforeYesterdayish());

  WallpaperInfo actual;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual);
  EXPECT_TRUE(actual.MatchesSelection(expected));
  EXPECT_EQ(std::string(),
            controller_->GetDailyRefreshCollectionId(kAccountId1));
}

TEST_F(WallpaperControllerTest, UpdateWallpaperOnScheduleCheckpointChanged) {
  SimulateUserLogin(kAccountId1);

  // Enable dark mode by default.
  Shell::Get()->dark_light_mode_controller()->SetDarkModeEnabledForTest(true);

  auto run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  // Use dark mode wallpaper initially.
  controller_->SetOnlineWallpaper(
      params, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);

  // Switch to light mode and simulate schedule checkpoint change to reflect
  // light mode.
  EXPECT_TRUE(Shell::Get()->dark_light_mode_controller()->IsDarkModeEnabled());
  Shell::Get()->dark_light_mode_controller()->ToggleColorMode();
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  WallpaperInfo expected = WallpaperInfo(OnlineWallpaperParams(
      kAccountId1, kAssetId2, GURL(kDummyUrl2),
      TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
      /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId, variants));
  WallpaperInfo actual;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual));
  EXPECT_TRUE(actual.MatchesAsset(expected));
}

TEST_F(WallpaperControllerTest,
       DoesNotUpdateWallpaperOnColorModeChangedWithNoVariants) {
  SimulateUserLogin(kAccountId1);

  auto run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  controller_->SetOnlineWallpaper(
      params, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(1, GetWallpaperCount());

  // Toggles color mode a couple times. Wallpaper count shouldn't change.
  Shell::Get()->dark_light_mode_controller()->ToggleColorMode();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());

  Shell::Get()->dark_light_mode_controller()->ToggleColorMode();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest,
       UpdateDailyWallpaperVariantOnColorModeChanged_RefreshTimerDoesntReset) {
  using base::Time;

  SimulateUserLogin(kAccountId1);
  // Resets the count as user will start with a default image after login.
  ClearWallpaperCount();

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/true, kUnitId, variants);
  const WallpaperInfo info = WallpaperInfo(params);
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  // Set a new daily wallpaper.
  controller_->UpdateDailyRefreshWallpaperForTesting();
  RunAllTasksUntilIdle();

  Time run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  base::TimeDelta delay = run_time - Time::Now();
  base::TimeDelta one_day = base::Days(1);
  // Leave a little wiggle room, as well as account for the hour fuzzing that
  // we do.
  EXPECT_GE(delay, one_day - base::Minutes(1));
  EXPECT_LE(delay, one_day + base::Minutes(61));

  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDaily);

  // Attempt a system's color mode change.
  Shell::Get()->dark_light_mode_controller()->ToggleColorMode();
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  // Expect the refresh timer doesn't reset.
  EXPECT_EQ(
      run_time,
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time());

  WallpaperInfo actual;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual));
  EXPECT_TRUE(actual.MatchesSelection(info));
}

TEST_F(WallpaperControllerTest,
       UpdateWallpaperInfoWithOnlineWallpaperVariants) {
  SimulateUserLogin(kAccountId1);

  // auto run_loop = std::make_unique<base::RunLoop>();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);

  pref_manager_->SetUserWallpaperInfo(kAccountId1, WallpaperInfo(params));
  WallpaperInfo expected = WallpaperInfo(params);
  WallpaperInfo actual;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual);
  EXPECT_TRUE(actual.MatchesSelection(expected));
}

TEST_F(WallpaperControllerTest, SetOnlineWallpaperWithoutInternet) {
  SetBypassDecode();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);
  SimulateUserLogin(kAccountId1);

  // Set an online wallpaper with image data. Verify that the wallpaper is set
  // successfully.
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  ClearWallpaperCount();
  controller_->SetOnlineWallpaper(params, base::DoNothing());
  RunAllTasksUntilIdle();
  ASSERT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);

  // Change the on-screen wallpaper to a different one. (Otherwise the
  // subsequent calls will be no-op since we intentionally prevent reloading the
  // same wallpaper.)
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, base::DoNothing(),
      /*file_path=*/"", CreateImage(640, 480, kWallpaperColor));
  RunAllTasksUntilIdle();
  ASSERT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kCustomized);

  // Attempt to set the same online wallpaper without internet. Verify it
  // still succeeds because the previous call to |SetOnlineWallpaper()| has
  // saved the file.
  test_wallpaper_image_downloader_->set_image_generator(
      base::BindLambdaForTesting([]() { return gfx::ImageSkia(); }));
  ClearWallpaperCount();
  base::RunLoop run_loop;
  controller_->SetOnlineWallpaper(
      params, base::BindLambdaForTesting([&run_loop](bool file_exists) {
        EXPECT_TRUE(file_exists);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
}

TEST_F(WallpaperControllerTest,
       HandleWallpaperInfoSyncedForDarkLightWallpapers_NotSynced) {
  SimulateUserLogin(kAccountId1);
  CacheOnlineWallpaper(kDummyUrl);
  ClearWallpaperCount();

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  // Force local info to not have a unit_id.
  WallpaperInfo local_info = WallpaperInfo(params);
  local_info.unit_id = absl::nullopt;
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  const OnlineWallpaperParams& params2 =
      OnlineWallpaperParams(kAccountId1, kAssetId2, GURL(kDummyUrl2),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  // synced info tracks dark variant.
  const WallpaperInfo& synced_info = WallpaperInfo(params2);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(synced_info));
  // Verify the wallpaper is set.
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest,
       HandleWallpaperInfoSyncedForDarkLightWallpapers_AlreadySynced) {
  SimulateUserLogin(kAccountId1);
  CacheOnlineWallpaper(kDummyUrl);
  ClearWallpaperCount();

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  // local info tracks light variant.
  const WallpaperInfo& local_info = WallpaperInfo(params);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  const OnlineWallpaperParams& params2 =
      OnlineWallpaperParams(kAccountId1, kAssetId2, GURL(kDummyUrl2),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  // synced info tracks dark variant.
  const WallpaperInfo& synced_info = WallpaperInfo(params2);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(local_info.MatchesSelection(synced_info));
  EXPECT_TRUE(local_info.MatchesSelection(actual_info));
  // Verify the wallpaper is not set again.
  EXPECT_EQ(0, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, WallpaperCustomization_Used) {
  // Reset to login screen.
  GetSessionControllerClient()->RequestSignOut();

  // Emulate login screen behavior.
  controller_->ShowSigninWallpaper();
  // Let the task queue run so that we run `ShowWallpaperImage()`.
  task_environment()->RunUntilIdle();

  std::pair<const base::FilePath, const base::FilePath> paths =
      CreateCustomizationWallpapers();
  ASSERT_FALSE(paths.first.empty());
  ASSERT_FALSE(paths.second.empty());

  SetBypassDecode();
  controller_->SetCustomizedDefaultWallpaperPaths(paths.first, paths.second);
  task_environment()->RunUntilIdle();

  // Verify that the customized wallpaper is in use.
  EXPECT_THAT(GetCurrentWallpaperInfo().location,
              testing::EndsWith(kCustomizationSmallWallpaperName));
}

TEST_F(WallpaperControllerTest, WallpaperCustomization_UnusedForNonDefault) {
  SetBypassDecode();
  SimulateUserLogin(kAccountId1);

  // Set wallpaper to something a user may have chose.
  controller_->SetOnlineWallpaper(
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            /*collection_id=*/std::string(),
                            WALLPAPER_LAYOUT_CENTER,
                            /*preview_mode=*/false, /*from_user=*/false,
                            /*daily_refresh_enabled=*/false, kUnitId,
                            /*variants=*/std::vector<OnlineWallpaperVariant>()),
      base::DoNothing());
  // Let the task queue run so that we run `ShowWallpaperImage()`.
  task_environment()->RunUntilIdle();

  // Simulate wallpaper customization retrieval completing after login.
  std::pair<const base::FilePath, const base::FilePath> paths =
      CreateCustomizationWallpapers();
  ASSERT_FALSE(paths.first.empty());
  ASSERT_FALSE(paths.second.empty());

  controller_->SetCustomizedDefaultWallpaperPaths(paths.first, paths.second);
  task_environment()->RunUntilIdle();

  // Verify that we still use the online wallpaper. i.e. did not switch to
  // default.
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
}

TEST_F(WallpaperControllerTest, TimeOfDayWallpapers_NotSyncedIn) {
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId3, GURL(kDummyUrl3),
                        backdrop::Image::IMAGE_TYPE_MORNING_MODE);
  variants.emplace_back(kAssetId4, GURL(kDummyUrl4),
                        backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  WallpaperInfo local_info = WallpaperInfo(params);
  local_info.unit_id = kUnitId;
  local_info.date = DayBeforeYesterdayish();
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  // synced info tracks a different wallpaper.
  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(local_info));
  // Verify that no new wallpaper is set.
  EXPECT_EQ(0, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, TimeOfDayWallpapers_NotSyncedOut) {
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();

  // synced info tracks a different wallpaper.
  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  synced_info.date = DayBeforeYesterdayish();
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId3, GURL(kDummyUrl3),
                        backdrop::Image::IMAGE_TYPE_MORNING_MODE);
  variants.emplace_back(kAssetId4, GURL(kDummyUrl4),
                        backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE);
  const OnlineWallpaperParams& params =
      OnlineWallpaperParams(kAccountId1, kAssetId, GURL(kDummyUrl),
                            TestWallpaperControllerClient::kDummyCollectionId,
                            WALLPAPER_LAYOUT_CENTER_CROPPED,
                            /*preview_mode=*/false, /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId, variants);
  WallpaperInfo local_info = WallpaperInfo(params);
  local_info.unit_id = kUnitId;
  pref_manager_->SetUserWallpaperInfo(kAccountId1, local_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(synced_info));
}

class WallpaperControllerGooglePhotosWallpaperTest
    : public WallpaperControllerTest,
      public testing::WithParamInterface<bool> {
 public:
  WallpaperControllerGooglePhotosWallpaperTest() = default;

  WallpaperControllerGooglePhotosWallpaperTest(
      const WallpaperControllerGooglePhotosWallpaperTest&) = delete;
  WallpaperControllerGooglePhotosWallpaperTest& operator=(
      const WallpaperControllerGooglePhotosWallpaperTest&) = delete;

  ~WallpaperControllerGooglePhotosWallpaperTest() override = default;

  void WaitForWallpaperCount(int count) {
    base::RunLoop run_loop;
    base::RepeatingTimer repeating_timer;
    repeating_timer.Start(FROM_HERE, base::Milliseconds(10),
                          base::BindLambdaForTesting([&]() {
                            if (GetWallpaperCount() >= count) {
                              repeating_timer.Stop();
                              run_loop.Quit();
                            }
                          }));
    run_loop.Run();
  }
};

TEST_F(WallpaperControllerGooglePhotosWallpaperTest, SetGooglePhotosWallpaper) {
  SimulateUserLogin(kAccountId1);

  // First set the wallpaper to an Online one so we can tell for sure if setting
  // a Google Photos wallpaper has failed.
  base::test::TestFuture<bool> online_future;
  controller_->SetOnlineWallpaper(
      {kAccountId1,
       kAssetId,
       GURL(kDummyUrl),
       TestWallpaperControllerClient::kDummyCollectionId,
       WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false,
       /*from_user=*/true,
       /*daily_refresh_enabled=*/false,
       kUnitId,
       {}},
      online_future.GetCallback());
  ASSERT_TRUE(online_future.Wait());
  ASSERT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);

  // Now attempt setting a Google Photos wallpaper.
  ClearWallpaperCount();
  int expected_wallpaper_count = 0;
  ASSERT_EQ(expected_wallpaper_count, GetWallpaperCount());
  GooglePhotosWallpaperParams params(kAccountId1, kFakeGooglePhotosPhotoId,
                                     /*daily_refresh_enabled=*/false,
                                     WallpaperLayout::WALLPAPER_LAYOUT_STRETCH,
                                     /*preview_mode=*/false, "dedup_key");

  controller_->SetGooglePhotosWallpaper(params, base::DoNothing());
  ++expected_wallpaper_count;

  WaitForWallpaperCount(expected_wallpaper_count);

  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnceGooglePhotos);

  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(params);
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       SetGooglePhotosWallpaperFails) {
  SimulateUserLogin(kAccountId1);

  // First set the wallpaper to an Online one so we can tell for sure if setting
  // a Google Photos wallpaper has failed.
  base::test::TestFuture<bool> online_future;
  OnlineWallpaperParams online_params(
      {kAccountId1,
       kAssetId,
       GURL(kDummyUrl),
       TestWallpaperControllerClient::kDummyCollectionId,
       WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false,
       /*from_user=*/true,
       /*daily_refresh_enabled=*/false,
       kUnitId,
       {}});
  controller_->SetOnlineWallpaper(online_params, online_future.GetCallback());
  ASSERT_TRUE(online_future.Wait());
  ASSERT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);

  // Attempt to set a Google Photos wallpaper with the client set to fail to
  // fetch the Google Photos photo data.
  client_.set_fetch_google_photos_photo_fails(true);
  ClearWallpaperCount();
  ASSERT_EQ(0, GetWallpaperCount());
  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosPhotoId, false,
       WallpaperLayout::WALLPAPER_LAYOUT_STRETCH, false, "dedup_key"},
      google_photos_future.GetCallback());
  EXPECT_FALSE(google_photos_future.Get());
  EXPECT_NE(controller_->GetWallpaperType(), WallpaperType::kOnceGooglePhotos);

  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(online_params);
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       RetryTimerTriggersOnFailedFetchPhotoForStalenessCheck) {
  using base::Time;

  SimulateUserLogin(kAccountId1);

  GooglePhotosWallpaperParams params(kAccountId1, kFakeGooglePhotosPhotoId,
                                     /*daily_refresh_enabled=*/false,
                                     WallpaperLayout::WALLPAPER_LAYOUT_STRETCH,
                                     /*preview_mode=*/false,
                                     /*dedup_key=*/absl::nullopt);
  controller_->SetGooglePhotosWallpaper(params, base::DoNothing());
  task_environment()->RunUntilIdle();

  Time run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  base::TimeDelta delay = run_time - Time::Now();

  base::TimeDelta one_day = base::Days(1);
  // Leave a little wiggle room, as well as account for the hour fuzzing that
  // we do.
  EXPECT_GE(delay, one_day - base::Minutes(1));
  EXPECT_LE(delay, one_day + base::Minutes(61));

  client_.set_fetch_google_photos_photo_fails(true);

  // Trigger Google Photos wallpaper cache check.
  controller_->OnActiveUserSessionChanged(kAccountId1);

  run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  delay = run_time - Time::Now();

  base::TimeDelta one_hour = base::Hours(1);

  // The cache check does not happen when the feature is disabled, since the
  // local `WallpaperInfo` is rejected.
  // Leave a little wiggle room.
  EXPECT_GE(delay, one_hour - base::Minutes(1));
  EXPECT_LE(delay, one_hour + base::Minutes(1));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       ResetToDefaultForDeletedPhotoOnStalenessCheck) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = {kFakeGooglePhotosPhotoId, WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kOnceGooglePhotos,
                        DayBeforeYesterdayish()};
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  client_.set_google_photo_has_been_deleted(true);
  // Trigger Google Photos wallpaper cache check.
  controller_->OnActiveUserSessionChanged(kAccountId1);
  WaitForWallpaperCount(1);

  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       GooglePhotosAreCachedOnDisk) {
  SimulateUserLogin(kAccountId1);

  base::test::TestFuture<bool> google_photos_future;

  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosPhotoId, /*daily_refresh_enabled=*/false,
       WALLPAPER_LAYOUT_STRETCH,
       /*preview_mode=*/false, "dedup_key"},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  base::FilePath saved_wallpaper = online_wallpaper_dir_.GetPath()
                                       .Append("google_photos/")
                                       .Append(kAccountId1.GetAccountIdKey())
                                       .Append(kFakeGooglePhotosPhotoId);
  ASSERT_TRUE(base::PathExists(saved_wallpaper));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       GooglePhotosAreCachedInMemory) {
  SimulateUserLogin(kAccountId1);

  base::FilePath path;
  EXPECT_FALSE(controller_->GetPathFromCache(kAccountId1, &path));
  gfx::ImageSkia cached_wallpaper;
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));

  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosPhotoId, /*daily_refresh_enabled=*/false,
       WALLPAPER_LAYOUT_STRETCH,
       /*preview_mode=*/false, "dedup_key"},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  // We store an empty path for Google Photos wallpapers in the in-memory cache
  // because storing the real path correctly would require updating the cache
  // after the asynchronous save operation, and we have no use for it anyway.
  EXPECT_TRUE(controller_->GetPathFromCache(kAccountId1, &path));
  EXPECT_TRUE(path.empty());
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(kAccountId1, &cached_wallpaper));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       GooglePhotosAreReadFromCache) {
  SimulateUserLogin(kAccountId1);

  base::test::TestFuture<bool> google_photos_future;

  GooglePhotosWallpaperParams params({kAccountId1, kFakeGooglePhotosPhotoId,
                                      /*daily_refresh_enabled=*/false,
                                      WALLPAPER_LAYOUT_STRETCH,
                                      /*preview_mode=*/false, "dedup_key"});
  controller_->SetGooglePhotosWallpaper(params,
                                        google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  // When Google Photos is disabled, the wallpaper will not be in disk cache,
  // so it will attempt to read from disk, fail to find it, and then reset to
  // the default wallpaper.
  const size_t expected_decodes = 0;
  const WallpaperType expected_type = WallpaperType::kOnceGooglePhotos;

  EXPECT_EQ(expected_decodes, GetDecodeFilePaths().size());
  EXPECT_EQ(expected_type, controller_->GetWallpaperType());
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest, ConfirmPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Set a Google Photos wallpaper for the user and enable preview. Verify that
  // the wallpaper is a Google Photos image if the feature is enabled.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_STRETCH;
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ClearWallpaperCount();
  std::string photo_id = "foobar";
  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, photo_id, /*daily_refresh_enabled=*/false, layout,
       /*preview_mode=*/true, "dedup_key"},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnceGooglePhotos);

  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Preview.Show", 1);

  // Now confirm the preview wallpaper, verify that there's no wallpaper
  // change because the wallpaper is already shown.
  ClearWallpaperCount();
  controller_->ConfirmPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 0);
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnceGooglePhotos);

  // Verify that the user wallpaper info is now updated to the Google Photos
  // wallpaper info.
  WallpaperInfo google_photos_wallpaper_info(photo_id, layout,
                                             WallpaperType::kOnceGooglePhotos,
                                             base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(google_photos_wallpaper_info));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest, CancelPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Set a Google Photos wallpaper for the user and enable preview. Verify that
  // the wallpaper is a Google Photos image if the feature is enabled.
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ClearWallpaperCount();
  std::string photo_id = "foobar";
  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, photo_id, /*daily_refresh_enabled=*/false,
       WALLPAPER_LAYOUT_STRETCH, /*preview_mode=*/true, "dedup_key"},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnceGooglePhotos);

  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Preview.Show", 1);

  // Now cancel the preview. Verify the wallpaper changes back to the default
  // and the user wallpaper info remains unchanged.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       WallpaperSyncedDuringPreview) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Set a Google Photos wallpaper for the user and enable preview. Verify that
  // the wallpaper is a Google Photos image if the feature is enabled.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_STRETCH;
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ClearWallpaperCount();
  std::string photo_id = "foobar";
  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, photo_id, /*daily_refresh_enabled=*/false, layout,
       /*preview_mode=*/true, "dedup_key"},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnceGooglePhotos);

  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(default_wallpaper_info));
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Preview.Show", 1);

  // Now set a custom wallpaper for the user and disable preview (this happens
  // if a custom wallpaper set on another device is being synced). Verify
  // there's no wallpaper change since preview mode shouldn't be interrupted.
  gfx::ImageSkia synced_custom_wallpaper =
      CreateImage(640, 480, kWallpaperColor);
  ClearWallpaperCount();
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, layout,
      /*preview_mode=*/false, base::DoNothing(),
      /*file_path=*/"", synced_custom_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 0);
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnceGooglePhotos);

  // However, the user wallpaper info should already be updated to the new
  // info.
  WallpaperInfo synced_custom_wallpaper_info(
      base::FilePath(kWallpaperFilesId1).Append(kFileName1).value(), layout,
      WallpaperType::kCustomized, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(synced_custom_wallpaper_info));

  // Now cancel the preview. Verify the synced custom wallpaper is shown
  // instead of the initial default wallpaper, and the user wallpaper info is
  // still correct.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(GetWallpaperCount(), 1);
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kCustomized);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(
      user_wallpaper_info.MatchesSelection(synced_custom_wallpaper_info));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       UpdateGooglePhotosDailyRefreshWallpaper) {
  // The `TestWallpaperControllerClient` sends back the reversed
  // `collection_id` when asked to fetch a daily photo.
  std::string expected_photo_id = kFakeGooglePhotosAlbumId;
  std::reverse(expected_photo_id.begin(), expected_photo_id.end());

  SimulateUserLogin(kAccountId1);

  GooglePhotosWallpaperParams params(
      kAccountId1, kFakeGooglePhotosAlbumId,
      /*daily_refresh_enabled=*/true, WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*dedup_key=*/absl::nullopt);
  WallpaperInfo info(params);
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaperForTesting();
  RunAllTasksUntilIdle();

  WallpaperInfo expected_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &expected_info));
  EXPECT_EQ(expected_photo_id, expected_info.location);
  EXPECT_EQ(kFakeGooglePhotosAlbumId, expected_info.collection_id);
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       DailyRefreshTimerStartsForDailyGooglePhotos) {
  SimulateUserLogin(kAccountId1);

  GooglePhotosWallpaperParams params(
      kAccountId1, kFakeGooglePhotosAlbumId,
      /*daily_refresh_enabled=*/true, WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*dedup_key=*/absl::nullopt);
  WallpaperInfo info(params);
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaperForTesting();
  RunAllTasksUntilIdle();
  auto& timer = controller_->GetUpdateWallpaperTimerForTesting();
  base::TimeDelta run_time =
      timer.desired_run_time().ToDeltaSinceWindowsEpoch();

  base::TimeDelta update_time =
      (base::Time::Now() + base::Days(1)).ToDeltaSinceWindowsEpoch();

  EXPECT_GE(run_time, update_time - base::Minutes(1));
  EXPECT_LE(run_time, update_time + base::Minutes(61));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       DailyRefreshRetryTimerStartsOnFailedFetch) {
  SimulateUserLogin(kAccountId1);

  GooglePhotosWallpaperParams params(
      kAccountId1, kFakeGooglePhotosAlbumId,
      /*daily_refresh_enabled=*/true, WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*dedup_key=*/absl::nullopt);
  WallpaperInfo info(params);
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  client_.set_fetch_google_photos_photo_fails(true);
  controller_->UpdateDailyRefreshWallpaperForTesting();
  RunAllTasksUntilIdle();

  base::TimeDelta run_time = controller_->GetUpdateWallpaperTimerForTesting()
                                 .desired_run_time()
                                 .ToDeltaSinceWindowsEpoch();

  base::TimeDelta update_time =
      (base::Time::Now() + base::Hours(1)).ToDeltaSinceWindowsEpoch();

  EXPECT_GE(run_time, update_time - base::Minutes(1));
  EXPECT_LE(run_time, update_time + base::Minutes(1));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       EmptyDailyGooglePhotosAlbumsDoNothing) {
  SimulateUserLogin(kAccountId1);

  GooglePhotosWallpaperParams daily_google_photos_params(
      kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
      WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
      /*dedup_key=*/absl::nullopt);
  OnlineWallpaperParams online_params(
      kAccountId1, kAssetId, GURL(kDummyUrl),
      TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/std::vector<OnlineWallpaperVariant>());

  WallpaperInfo online_info(online_params);
  pref_manager_->SetUserWallpaperInfo(kAccountId1, online_info);

  client_.set_fetch_google_photos_photo_fails(true);
  controller_->SetGooglePhotosWallpaper(daily_google_photos_params,
                                        base::DoNothing());
  RunAllTasksUntilIdle();

  WallpaperInfo current_info;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_TRUE(online_info.MatchesSelection(current_info));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       ResetToDefaultForDeletedDailyGooglePhotosAlbums) {
  SimulateUserLogin(kAccountId1);

  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
       WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/absl::nullopt},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  WallpaperInfo current_info;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_EQ(WallpaperType::kDailyGooglePhotos, current_info.type);

  // This makes the test fetch in `client_` return a null photo, but a
  // successful call, which is the sign for a deleted or empty album.
  client_.set_google_photo_has_been_deleted(true);

  controller_->UpdateDailyRefreshWallpaperForTesting();
  RunAllTasksUntilIdle();

  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_EQ(WallpaperType::kDefault, current_info.type);
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       DailyGooglePhotosAreCached) {
  SimulateUserLogin(kAccountId1);
  // The `TestWallpaperControllerClient` sends back the reversed
  // `collection_id` when asked to fetch a daily photo.
  std::string expected_photo_id = kFakeGooglePhotosAlbumId;
  std::reverse(expected_photo_id.begin(), expected_photo_id.end());

  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
       WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/absl::nullopt},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  base::FilePath saved_wallpaper = online_wallpaper_dir_.GetPath()
                                       .Append("google_photos/")
                                       .Append(kAccountId1.GetAccountIdKey())
                                       .Append(expected_photo_id);
  ASSERT_TRUE(base::PathExists(saved_wallpaper));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       ResetToDefaultWhenLoadingInvalidWallpaper) {
  SimulateUserLogin(kAccountId1);

  const WallpaperType type = WallpaperType::kCount;

  WallpaperInfo info = {kFakeGooglePhotosPhotoId, WALLPAPER_LAYOUT_CENTER, type,
                        base::Time::Now()};
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();

  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       SetGooglePhotosDailyRefreshAlbumId_UpdatesDailyRefreshTimer) {
  using base::Time;

  pref_manager_->SetUserWallpaperInfo(
      kAccountId1,
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER,
                    WallpaperType::kOnline, DayBeforeYesterdayish()));

  std::string album_id = "fun_album";
  controller_->SetGooglePhotosDailyRefreshAlbumId(kAccountId1, album_id);
  WallpaperInfo expected = {std::string(), WALLPAPER_LAYOUT_CENTER,
                            WallpaperType::kDailyGooglePhotos,
                            DayBeforeYesterdayish()};
  expected.collection_id = album_id;

  WallpaperInfo actual;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual);
  // Type should be `WallpaperType::kDailyGooglePhotos` now, and collection_id
  // should be updated.
  EXPECT_TRUE(actual.MatchesSelection(expected));
  EXPECT_EQ(album_id,
            controller_->GetGooglePhotosDailyRefreshAlbumId(kAccountId1));
  Time run_time =
      controller_->GetUpdateWallpaperTimerForTesting().desired_run_time();
  base::TimeDelta delay = run_time - Time::Now();
  base::TimeDelta one_day = base::Days(1);
  // Leave a little wiggle room, as well as account for the hour fuzzing that
  // we do.
  EXPECT_GE(delay, one_day - base::Minutes(1));
  EXPECT_LE(delay, one_day + base::Minutes(61));
}

TEST_F(WallpaperControllerGooglePhotosWallpaperTest,
       ResetToDefaultForDisabledGooglePhotosIntegrationPolicyOnStalenessCheck) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = {kFakeGooglePhotosPhotoId, WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kOnceGooglePhotos,
                        DayBeforeYesterdayish()};
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  client_.set_wallpaper_google_photos_integration_enabled_for_account_id(
      kAccountId1, false);

  // Trigger Google Photos wallpaper cache check.
  controller_->OnActiveUserSessionChanged(kAccountId1);
  WaitForWallpaperCount(1);

  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
}

TEST_F(
    WallpaperControllerGooglePhotosWallpaperTest,
    ResetToDefaultForDisabledGooglePhotosIntegrationPolicyDailyGooglePhotosAlbums) {
  SimulateUserLogin(kAccountId1);

  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
       WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/absl::nullopt},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  WallpaperInfo current_info;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_EQ(WallpaperType::kDailyGooglePhotos, current_info.type);

  // This makes the test fetch in `client_` return a null photo, but a
  // successful call, which is the sign for the
  // WallpaperGooglePhotosIntegrationEnabled policy disabled, or a deleted or
  // empty album.
  client_.set_wallpaper_google_photos_integration_enabled_for_account_id(
      kAccountId1, false);

  controller_->UpdateDailyRefreshWallpaperForTesting();
  RunAllTasksUntilIdle();

  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_EQ(WallpaperType::kDefault, current_info.type);
}

}  // namespace ash
