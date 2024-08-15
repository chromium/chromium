// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/geolocation/geolocation_controller_test_util.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/test_sea_pen_wallpaper_manager_session_delegate.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "ash/wallpaper/test_wallpaper_drivefs_delegate.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_blur_manager.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_daily_refresh_scheduler.h"
#include "ash/wallpaper/wallpaper_metrics_manager.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/wallpaper/wallpaper_time_of_day_scheduler.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/version.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
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
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"
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

// For checking that the wallpaper changes at approximately the correct time
// when the "auto" schedule is enabled. The sunrise/set times specified in
// `WallpaperControllerAutoScheduleTest` are just approximate and do not occur
// exactly on the hour specified.
MATCHER_P(WallpaperChangeTimeNear, hours_elapsed_since_test_start, "") {
  static constexpr base::TimeDelta kTolerance = base::Minutes(5);
  base::TimeDelta expected_duration_since_test_start =
      base::Hours(hours_elapsed_since_test_start);
  return expected_duration_since_test_start - kTolerance <= arg &&
         arg <= expected_duration_since_test_start + kTolerance;
}

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
      WallpaperControllerImpl::GetCustomWallpaperDir(kSmallWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath large_wallpaper_dir =
      WallpaperControllerImpl::GetCustomWallpaperDir(kLargeWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath original_wallpaper_dir =
      WallpaperControllerImpl::GetCustomWallpaperDir(kOriginalWallpaperSubDir)
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
  void OnDailyRefreshCheckpointChanged() override {
    ++daily_refresh_checkpoint_count_;
  }

  int colors_changed_count() const { return colors_changed_count_; }
  int blur_changed_count() const { return blur_changed_count_; }
  int first_shown_count() const { return first_shown_count_; }
  int wallpaper_changed_count() const { return wallpaper_changed_count_; }
  int daily_refresh_checkpoint_count() const {
    return daily_refresh_checkpoint_count_;
  }
  bool is_in_wallpaper_preview() const { return is_in_wallpaper_preview_; }

  void ClearDailyRefreshCheckpointCount() {
    daily_refresh_checkpoint_count_ = 0;
  }

 private:
  raw_ptr<WallpaperController> controller_;

  base::RepeatingClosure resize_callback_;
  base::RepeatingClosure colors_calculated_callback_;

  int colors_changed_count_ = 0;
  int blur_changed_count_ = 0;
  int first_shown_count_ = 0;
  int wallpaper_changed_count_ = 0;
  int daily_refresh_checkpoint_count_ = 0;
  bool is_in_wallpaper_preview_ = false;
};

// Runs until the next time the wallpaper changes.
class WallpaperChangedBarrier : public WallpaperControllerObserver {
 public:
  WallpaperChangedBarrier(WallpaperController* controller,
                          base::test::TaskEnvironment* task_environment)
      : task_environment_(task_environment) {
    CHECK(task_environment_);
    controller_observation_.Observe(controller);
  }
  WallpaperChangedBarrier(const WallpaperChangedBarrier&) = delete;
  WallpaperChangedBarrier& operator=(const WallpaperChangedBarrier&) = delete;
  ~WallpaperChangedBarrier() override = default;

  // WallpaperControllerObserver:
  void OnWallpaperChanged() override { wallpaper_changed_ = true; }

  bool RunUntilNextWallpaperChange() {
    wallpaper_changed_ = false;
    while (!wallpaper_changed_) {
      RunAllTasksUntilIdle();
      base::TimeDelta delay_until_next_task =
          task_environment_->NextMainThreadPendingTaskDelay();
      if (delay_until_next_task == base::TimeDelta::Max()) {
        // Technically, a delayed task on a different thread than "main" could
        // trigger a wallpaper change but that is currently not the case.
        return false;
      }
      task_environment_->FastForwardBy(delay_until_next_task);
    }
    return true;
  }

 private:
  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      controller_observation_{this};
  const raw_ptr<base::test::TaskEnvironment> task_environment_;
  bool wallpaper_changed_ = false;
};

// Returns the image in `backdrop_image_data` whose `image_url` matches `url`,
// or nullptr if no match is found.
const backdrop::Image* GetImageMatchingUrl(
    const GURL& url,
    const std::vector<backdrop::Image>& backdrop_image_data) {
  for (const backdrop::Image& image : backdrop_image_data) {
    if (image.image_url() == url.spec()) {
      return &image;
    }
  }
  return nullptr;
}

// Returns the time of day wallpapers in order of light, morning, late
// afternoon, and dark.
std::vector<backdrop::Image> TimeOfDayImageSet() {
  const std::vector<backdrop::Image_ImageType> image_types = {
      backdrop::Image::IMAGE_TYPE_LIGHT_MODE,
      backdrop::Image::IMAGE_TYPE_MORNING_MODE,
      backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE,
      backdrop::Image::IMAGE_TYPE_DARK_MODE};

  std::vector<backdrop::Image> images;
  for (size_t i = 0; i < image_types.size(); ++i) {
    const uint64_t asset_id = i + 99;
    const std::string url =
        base::StringPrintf("https://preferred_wallpaper/images/%zu", asset_id);
    backdrop::Image image;
    image.set_asset_id(asset_id);
    image.set_unit_id(wallpaper_constants::kDefaultTimeOfDayWallpaperUnitId);
    image.set_image_type(image_types[i]);
    image.set_image_url(url);
    images.push_back(image);
  }
  return images;
}

// Returns a collection of images with randomized asset ids.
std::vector<backdrop::Image> ImageSet() {
  const size_t image_size = 10;
  const auto base_asset_id = rand() % 100;
  std::vector<backdrop::Image> images;
  for (size_t i = 0; i < image_size; ++i) {
    const uint64_t asset_id = i + base_asset_id;
    const std::string url =
        base::StringPrintf("https://preferred_wallpaper/images/%zu", asset_id);
    backdrop::Image image;
    image.set_asset_id(asset_id);
    image.set_unit_id(asset_id);
    image.set_image_type(backdrop::Image::IMAGE_TYPE_UNKNOWN);
    image.set_image_url(url);
    images.push_back(image);
  }
  return images;
}

}  // namespace

class WallpaperControllerTestBase : public AshTestBase {
 public:
  WallpaperControllerTestBase()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  WallpaperControllerTestBase(const WallpaperControllerTestBase&) = delete;
  WallpaperControllerTestBase& operator=(const WallpaperControllerTestBase&) =
      delete;

  void SetUp() override {
    auto pref_manager = WallpaperPrefManager::Create(local_state());
    pref_manager_ = pref_manager.get();
    // Override the pref manager and image downloader that will be used to
    // construct the WallpaperController.
    WallpaperControllerImpl::SetWallpaperPrefManagerForTesting(
        std::move(pref_manager));

    WallpaperControllerImpl::SetWallpaperImageDownloaderForTesting(
        std::make_unique<TestWallpaperImageDownloader>());

    AshTestBase::SetUp();

    SeaPenWallpaperManager::GetInstance()->SetSessionDelegateForTesting(
        std::make_unique<TestSeaPenWallpaperManagerSessionDelegate>());

    TestSessionControllerClient* const client = GetSessionControllerClient();
    client->ProvidePrefServiceForUser(kAccountId1);
    client->ProvidePrefServiceForUser(kAccountId2);
    client->ProvidePrefServiceForUser(
        AccountId::FromUserEmail(user_manager::kGuestUserName));
    client->ProvidePrefServiceForUser(kChildAccountId);

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

    CreateDefaultWallpapers();
  }

  void TearDown() override {
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
    base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
        kSmallWallpaperSubDir, wallpaper_files_id, file_name);
    base::FilePath large_wallpaper_path = GetCustomWallpaperPath(
        kLargeWallpaperSubDir, wallpaper_files_id, file_name);

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

  // Initializes default wallpaper paths "*default_*file" and writes JPEG
  // wallpaper images to them. Called during SetUp() to mimic production
  // behavior, which expects default wallpapers to always exist.
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
      bool preview_mode,
      bool from_user,
      uint64_t unit_id,
      WallpaperController::SetWallpaperCallback callback) {
    const OnlineWallpaperVariant variant(asset_id, GURL(url),
                                         backdrop::Image::IMAGE_TYPE_UNKNOWN);
    const OnlineWallpaperParams params = {
        account_id,   collection_id, layout,
        preview_mode, from_user,     /*daily_refresh_enabled=*/false,
        unit_id,      {variant}};
    controller_->OnOnlineWallpaperDecoded(account_id, preview_mode,
                                          WallpaperInfo(params, variant),
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
    SimulateUserLogin(kAccountId1);
    ClearWallpaperCount();
    controller_->SetOnlineWallpaper(
        OnlineWallpaperParams(
            kAccountId1,
            /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
            /*preview_mode=*/false, /*from_user=*/false,
            /*daily_refresh_enabled=*/false, kUnitId,
            /*variants=*/
            {{kAssetId, GURL(path), backdrop::Image::IMAGE_TYPE_UNKNOWN}}),
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

  void SetSeaPenWallpaper(const AccountId& account_id,
                          SkColor color,
                          uint32_t id,
                          bool preview_mode,
                          gfx::ImageSkia* image) {
    TestWallpaperControllerObserver observer(controller_);
    std::string jpg_bytes = CreateEncodedImageForTesting(
        {1, 1}, color, data_decoder::mojom::ImageCodec::kDefault, image);
    ASSERT_TRUE(!jpg_bytes.empty());

    base::test::TestFuture<bool> save_sea_pen_image_future;
    auto* sea_pen_wallpaper_manager = SeaPenWallpaperManager::GetInstance();
    sea_pen_wallpaper_manager->SaveSeaPenImage(
        account_id, {std::move(jpg_bytes), id},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("search_query"),
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());

    base::test::TestFuture<bool> set_wallpaper_future;
    controller_->SetSeaPenWallpaper(account_id, id, preview_mode,
                                    set_wallpaper_future.GetCallback());

    EXPECT_TRUE(set_wallpaper_future.Take());
    EXPECT_EQ(1, observer.wallpaper_changed_count());
    histogram_tester().ExpectUniqueSample("Ash.Wallpaper.SeaPen.Result2",
                                          SetWallpaperResult::kSuccess, 1);
  }

  TestWallpaperImageDownloader* test_wallpaper_image_downloader() {
    return static_cast<TestWallpaperImageDownloader*>(
        controller_->wallpaper_image_downloader_for_testing());
  }

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

  // Returns the last modified time of a file. Returns the old last modified
  // time if the process fails.
  base::Time GetLastModifiedTime(const base::FilePath& path) {
    base::File::Info info;
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (file.GetInfo(&info)) {
      return info.last_modified;
    }
    return base::Time();
  }

  raw_ptr<WallpaperControllerImpl, DanglingUntriaged> controller_;
  raw_ptr<WallpaperPrefManager, DanglingUntriaged> pref_manager_ =
      nullptr;  // owned by controller

  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir online_wallpaper_dir_;
  base::ScopedTempDir custom_wallpaper_dir_;
  base::ScopedTempDir default_wallpaper_dir_;
  base::ScopedTempDir customization_wallpaper_dir_;
  base::HistogramTester histogram_tester_;

  TestWallpaperControllerClient client_;
  raw_ptr<TestWallpaperDriveFsDelegate, DanglingUntriaged> drivefs_delegate_;

  const AccountId kChildAccountId =
      AccountId::FromUserEmailGaiaId(kChildEmail, kChildEmail);

 private:
  InProcessDataDecoder decoder_;
  base::Time mock_clock_origin_;
};

// All possible feature combinations that can occur in the real world.
enum class TimeOfDayFeatureCombination { kDisabled, kTimeOfDay };

class WallpaperControllerTest
    : public WallpaperControllerTestBase,
      public testing::WithParamInterface<TimeOfDayFeatureCombination> {
 public:
  WallpaperControllerTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    switch (GetParam()) {
      case TimeOfDayFeatureCombination::kDisabled:
        disabled_features = personalization_app::GetTimeOfDayDisabledFeatures();
        break;
      case TimeOfDayFeatureCombination::kTimeOfDay:
        enabled_features = personalization_app::GetTimeOfDayEnabledFeatures();
        break;
    }
    enabled_features.push_back(features::kSeaPen);
    enabled_features.push_back(features::kFeatureManagementSeaPen);
    enabled_features.push_back(features::kSeaPenDemoMode);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  WallpaperControllerTest(const WallpaperControllerTest&) = delete;
  WallpaperControllerTest& operator=(const WallpaperControllerTest&) = delete;

  ~WallpaperControllerTest() override = default;

  bool IsTimeOfDayEnabled() const {
    switch (GetParam()) {
      case TimeOfDayFeatureCombination::kDisabled:
        return false;
      case TimeOfDayFeatureCombination::kTimeOfDay:
        return true;
    }
  }

  // Populate meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      switch (info.param) {
        case TimeOfDayFeatureCombination::kDisabled:
          return "TimeOfDayOff";
        case TimeOfDayFeatureCombination::kTimeOfDay:
          return "TimeOfDayOn";
      }
    }
  };

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// For tests that use the "auto" D/L mode setting and need a definitive
// geoposition/date with known sunrise/sunset times.
class WallpaperControllerAutoScheduleTest : public WallpaperControllerTest,
                                            public ScheduledFeature::Clock {
 protected:
  // San Jose. Sunrise is approximately 7:00 AM, and sunset is approximately
  // 7:00 PM PDT on `kTestDateMidnight`. Test starts at `kTestDateMidnight`
  // by default.
  static constexpr SimpleGeoposition kSanJoseGeoposition = {37.335480,
                                                            -121.893028};
  static constexpr char kTestDateMidnight[] = "26 Sep 2023 00:00:00 PDT";
  static constexpr char kPDTTimezone[] = "America/Los_Angeles";

  WallpaperControllerAutoScheduleTest()
      : task_environment_(task_environment()), timezone_pdt_(kPDTTimezone) {}

  void SetUp() override {
    ASSERT_TRUE(timezone_pdt_.is_success());

    WallpaperControllerTest::SetUp();
    task_environment_start_time_ = task_environment()->GetMockClock()->Now();
    SetSimulatedStartTime(GetTestDateMidnight());

    // Set fixed geoposition for testing.
    scoped_refptr<TestGeolocationUrlLoaderFactory>
        geolocation_url_loader_factory =
            base::MakeRefCounted<TestGeolocationUrlLoaderFactory>();
    geolocation_url_loader_factory->SetValidPosition(
        kSanJoseGeoposition.latitude, kSanJoseGeoposition.longitude, Now());
    SimpleGeolocationProvider::GetInstance()
        ->SetSharedUrlLoaderFactoryForTesting(
            std::move(geolocation_url_loader_factory));

    GeopositionResponsesWaiter waiter(Shell::Get()->geolocation_controller());
    waiter.Wait();
  }

  // base::Clock:
  base::Time Now() const override {
    base::TimeDelta test_time_elapsed =
        task_environment_->GetMockClock()->Now() - task_environment_start_time_;
    return simulated_start_time_ + test_time_elapsed;
  }

  // base::TickClock:
  base::TimeTicks NowTicks() const override {
    return task_environment_->NowTicks();
  }

  base::Time GetTestDateMidnight() {
    base::Time time;
    CHECK(base::Time::FromString(kTestDateMidnight, &time));
    return time;
  }

  void SetSimulatedStartTime(base::Time simulated_start_time) {
    // Turn "auto" schedule off first to kill any internal timers within these
    // objects before passing them a new clock.
    WallpaperTimeOfDayScheduler& time_of_day_scheduler =
        *Shell::Get()
             ->wallpaper_controller()
             ->time_of_day_scheduler_for_testing();
    Shell::Get()->dark_light_mode_controller()->SetAutoScheduleEnabled(false);
    time_of_day_scheduler.SetScheduleType(ScheduleType::kNone);

    simulated_start_time_ = simulated_start_time;
    Shell::Get()->geolocation_controller()->SetClockForTesting(this);
    Shell::Get()->dark_light_mode_controller()->SetClockForTesting(this);
    time_of_day_scheduler.SetClockForTesting(this);

    Shell::Get()->dark_light_mode_controller()->SetAutoScheduleEnabled(true);
    time_of_day_scheduler.SetScheduleType(ScheduleType::kSunsetToSunrise);
  }

  const raw_ptr<base::test::TaskEnvironment> task_environment_;
  const calendar_test_utils::ScopedLibcTimeZone timezone_pdt_;
  base::Time simulated_start_time_;
  base::Time task_environment_start_time_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    WallpaperControllerTest,
    ::testing::Values(TimeOfDayFeatureCombination::kDisabled,
                      TimeOfDayFeatureCombination::kTimeOfDay),
    WallpaperControllerTest::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    WallpaperControllerAutoScheduleTest,
    ::testing::Values(TimeOfDayFeatureCombination::kDisabled,
                      TimeOfDayFeatureCombination::kTimeOfDay),
    WallpaperControllerTest::PrintToStringParamName());

TEST_P(WallpaperControllerTest, Client) {
  base::FilePath empty_path;
  controller_->Init(empty_path, empty_path, empty_path, empty_path);

  EXPECT_EQ(0u, client_.open_count());
  controller_->OpenWallpaperPickerIfAllowed();
  EXPECT_EQ(1u, client_.open_count());
}

TEST_P(WallpaperControllerTest, BasicReparenting) {
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

TEST_P(WallpaperControllerTest, SwitchWallpapersWhenNewWallpaperAnimationEnds) {
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
TEST_P(WallpaperControllerTest, WallpaperMovementDuringUnlock) {
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

  const bool forest_enabled = features::IsForestFeatureEnabled();

  // In this state we have a wallpaper views stored in
  // LockScreenWallpaperContainer.
  WallpaperWidgetController* widget_controller =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->wallpaper_widget_controller();
  EXPECT_TRUE(widget_controller->IsAnimating());
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(1, ChildCountForContainer(kLockScreenWallpaperId));
  if (forest_enabled) {
    // There must be four layers: shield, underlay, original and old layers.
    ASSERT_EQ(4u, wallpaper_view()->layer()->parent()->children().size());
  } else {
    // There must be three layers: shield, original and old layers.
    ASSERT_EQ(3u, wallpaper_view()->layer()->parent()->children().size());
  }

  // Before the wallpaper's animation completes, user unlocks the screen, which
  // moves the wallpaper to the back.
  controller->OnSessionStateChanged(session_manager::SessionState::ACTIVE);

  // Ensure that widget has moved.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  // The shield layer is gone during an active session.
  if (forest_enabled) {
    ASSERT_EQ(3u, wallpaper_view()->layer()->parent()->children().size());
  } else {
    ASSERT_EQ(2u, wallpaper_view()->layer()->parent()->children().size());
  }
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));

  // Finish the new wallpaper animation.
  RunDesktopControllerAnimation();

  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  if (forest_enabled) {
    // Now there is one wallpaper and two layers: underlay and original.
    ASSERT_EQ(2u, wallpaper_view()->layer()->parent()->children().size());
  } else {
    // Now there is one wallpaper and the original layer.
    ASSERT_EQ(1u, wallpaper_view()->layer()->parent()->children().size());
  }
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));
}

// Test for crbug.com/156542. Animating wallpaper should immediately finish
// animation and replace current wallpaper before next animation starts.
TEST_P(WallpaperControllerTest, ChangeWallpaperQuick) {
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

TEST_P(WallpaperControllerTest, ResizeCustomWallpaper) {
  UpdateDisplay("320x200");

  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);

  // Set the image as custom wallpaper, wait for the resize to finish, and check
  // that the resized image is the expected size.
  controller_->ShowWallpaperImage(
      image, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
      /*preview_mode=*/false, /*is_override=*/false);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(controller_->GetWallpaper()),
                                        gfx::Image(image)));
  RunAllTasksUntilIdle();
  gfx::ImageSkia resized_image = controller_->GetWallpaper();
  EXPECT_FALSE(gfx::test::AreImagesEqual(
      gfx::Image(controller_->GetWallpaper()), gfx::Image(image)));
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
TEST_P(WallpaperControllerTest, DontScaleWallpaperWithCenterLayout) {
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

TEST_P(WallpaperControllerTest, ShouldCalculateColorsBasedOnImage) {
  EnableShelfColoring();
  EXPECT_TRUE(ShouldCalculateColors());

  controller_->CreateEmptyWallpaperForTesting();
  EXPECT_FALSE(ShouldCalculateColors());
}

TEST_P(WallpaperControllerTest, ShouldCalculateColorsBasedOnSessionState) {
  EnableShelfColoring();
  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::HIDDEN);

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

TEST_P(WallpaperControllerTest, ShouldCalculateColorsBasedOnLoginDisplayState) {
  EnableShelfColoring();
  SetSessionState(SessionState::LOGIN_PRIMARY);

  // Cover login screen
  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::HIDDEN);
  EXPECT_FALSE(ShouldCalculateColors());
  // Cover OOBE enterprise enrollment flow
  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShouldCalculateColors());
}

TEST_P(WallpaperControllerTest, ColorsCalculatedForMostRecentWallpaper) {
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

TEST_P(WallpaperControllerTest, SaveCelebiColor) {
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

TEST_P(WallpaperControllerTest,
       GetCachedWallpaperColorForUser_WithCelebiColor) {
  // Cache some wallpapers and store that in the local prefs. Otherwise, we
  // can't cache colors.
  base::FilePath relative_path = PrecacheWallpapers(kAccountId1);
  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  info.location = relative_path.value();
  ASSERT_TRUE(pref_manager_->SetLocalWallpaperInfo(kAccountId1, info));

  // Store colors in local prefs simulating cache behavior.
  pref_manager_->CacheCelebiColor(relative_path.value(), kWallpaperColor);

  // Reset to login screen.
  GetSessionControllerClient()->RequestSignOut();

  // User's wallpaper colors are accessible from login screen.
  EXPECT_EQ(kWallpaperColor, controller_->GetCachedWallpaperColorForUser(
                                 kAccountId1, /* use_k_means= */ false));
}

TEST_P(WallpaperControllerTest,
       GetCachedWallpaperColorForUser_WithKMeansColor) {
  // Cache some wallpapers and store that in the local prefs. Otherwise, we
  // can't cache colors.
  base::FilePath relative_path = PrecacheWallpapers(kAccountId1);
  WallpaperInfo info = InfoWithType(WallpaperType::kCustomized);
  info.location = relative_path.value();
  ASSERT_TRUE(pref_manager_->SetLocalWallpaperInfo(kAccountId1, info));

  // Store colors in local prefs simulating cache behavior.
  pref_manager_->CacheKMeanColor(relative_path.value(), kWallpaperColor);

  // Reset to login screen.
  GetSessionControllerClient()->RequestSignOut();

  // User's wallpaper colors are accessible from login screen.
  EXPECT_EQ(kWallpaperColor, controller_->GetCachedWallpaperColorForUser(
                                 kAccountId1, /* use_k_means= */ true));
}

TEST_P(WallpaperControllerTest, EnableShelfColoringNotifiesObservers) {
  TestWallpaperControllerObserver observer(controller_);
  EXPECT_EQ(0, observer.colors_changed_count());

  // Enable shelf coloring will set a customized wallpaper image and change
  // session state to ACTIVE, which will trigger wallpaper colors calculation.
  EnableShelfColoring();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.colors_changed_count());
}

TEST_P(WallpaperControllerTest,
       OnWallpaperColorsChangedAlwaysCalledOnFirstUpdate) {
  TestWallpaperControllerObserver observer(controller_);
  controller_->ShowUserWallpaper(kAccountId1, user_manager::UserType::kRegular);
  task_environment()->RunUntilIdle();

  // Even though the wallpaper color is invalid, observers should still be
  // notified for the first update.
  EXPECT_EQ(observer.colors_changed_count(), 1);

  controller_->ShowUserWallpaper(kAccountId2, user_manager::UserType::kRegular);
  task_environment()->RunUntilIdle();

  // Observers should not be notified after the first update if the colors do
  // not change.
  EXPECT_EQ(observer.colors_changed_count(), 1);
}

TEST_P(WallpaperControllerTest,
       UpdatePrimaryUserWallpaperWhileSecondUserActive) {
  WallpaperInfo wallpaper_info;

  SimulateUserLogin(kAccountId1);

  // Set an online wallpaper with image data. Verify that the wallpaper is set
  // successfully.
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1,
      /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/
      {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}});
  controller_->SetOnlineWallpaper(params, base::DoNothing());
  RunAllTasksUntilIdle();
  // Verify that the user wallpaper info is updated.
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(params, params.variants.front());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));

  // Log in |kUser2|, and set another online wallpaper for |kUser1|. Verify that
  // the on-screen wallpaper doesn't change since |kUser1| is not active, but
  // wallpaper info is updated properly.
  SimulateUserLogin(kAccountId2);
  ClearWallpaperCount();
  const OnlineWallpaperParams& new_params = OnlineWallpaperParams(
      kAccountId1,
      /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId2,
      /*variants=*/
      {{kAssetId2, GURL(kDummyUrl2), backdrop::Image::IMAGE_TYPE_UNKNOWN}});
  controller_->SetOnlineWallpaper(new_params, base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info_2(new_params,
                                          new_params.variants.front());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info_2));
}

TEST_P(WallpaperControllerTest, SetOnlineWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
  SimulateUserLogin(kAccountId1);

  // Verify that calling |SetOnlineWallpaper| will download the image data if it
  // does not exist. Verify that the wallpaper is set successfully.
  auto run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/
      {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}});
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
  WallpaperInfo expected_wallpaper_info(params, params.variants.front());
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

TEST_P(WallpaperControllerTest,
       SetOnlineWallpaper_FiresResizedSignalWhenSettingTheSameWallpaper) {
  TestWallpaperControllerObserver observer(controller_);
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
  SimulateUserLogin(kAccountId1);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/
      {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}});
  {
    // Verify that calling |SetOnlineWallpaper| will download the image data if
    // it does not exist. Verify that the wallpaper is set successfully.
    base::RunLoop run_loop;
    ClearWallpaperCount();
    base::RunLoop resized_loop;
    observer.SetOnResizeCallback(resized_loop.QuitClosure());

    controller_->SetOnlineWallpaper(
        params, base::BindLambdaForTesting(
                    [quit = run_loop.QuitClosure()](bool success) {
                      EXPECT_TRUE(success);
                      std::move(quit).Run();
                    }));
    run_loop.Run();
    resized_loop.Run();
    EXPECT_EQ(1, GetWallpaperCount());
    EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
  }

  {
    // Verifies setting the same wallpaper still results in `OnWallpaperResized`
    // being fired.
    base::RunLoop run_loop;
    base::RunLoop resized_loop;
    observer.SetOnResizeCallback(resized_loop.QuitClosure());
    controller_->SetOnlineWallpaper(
        params, base::BindLambdaForTesting(
                    [quit = run_loop.QuitClosure()](bool success) {
                      EXPECT_TRUE(success);
                      std::move(quit).Run();
                    }));
    run_loop.Run();
    resized_loop.Run();
  }
}

TEST_P(WallpaperControllerTest, SetTimeOfDayWallpaper) {
  if (!IsTimeOfDayEnabled()) {
    return;
  }
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);
  SimulateUserLogin(kAccountId1);

  // Verify that calling |SetTimeOfDayWallpaper| will download the image
  // data if it does not exist. Verify that the wallpaper is set successfully.
  base::RunLoop run_loop;
  ClearWallpaperCount();
  controller_->SetTimeOfDayWallpaper(
      kAccountId1,
      base::BindLambdaForTesting([quit = run_loop.QuitClosure()](bool success) {
        EXPECT_TRUE(success);
        std::move(quit).Run();
      }));
  run_loop.Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
  // Verify that the user wallpaper info is updated.
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_EQ(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
            wallpaper_info.collection_id);
  EXPECT_EQ(WallpaperType::kOnline, wallpaper_info.type);
  // Verify that the any of the wallpaper variant is available offline, and the
  // returned file name should not contain the small wallpaper suffix.
  //
  // The ThreadPool must be flushed to ensure that the online wallpaper is saved
  // to disc before checking the test expectation below. Ideally, we'd wait for
  // an explicit event, but the production code does not need this and it's not
  // worthwhile to add something to the API just for tests.
  RunAllTasksUntilIdle();
  EXPECT_TRUE(base::PathExists(online_wallpaper_dir_.GetPath().Append(
      GURL(images[1].image_url()).ExtractFileName())));
}

TEST_P(WallpaperControllerTest,
       ActiveUserPrefServiceChanged_SetTimeOfDayWallpaper) {
  if (!IsTimeOfDayEnabled()) {
    return;
  }
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kDefault);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);
  SetSessionState(SessionState::OOBE);
  // Log in and trigger `OnActiveUserPrefServiceChange`.
  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kOnline, actual_info.type);
  EXPECT_EQ(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
            actual_info.collection_id);
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.IsSetToTimeOfDayAfterOobe",
                                      1);
}

TEST_P(WallpaperControllerTest,
       TimeOfDayWallpaper_ReplacedByUserWallpaper_DuringOobe) {
  if (!IsTimeOfDayEnabled()) {
    return;
  }
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kDefault);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);
  SetSessionState(SessionState::OOBE);
  // Log in and trigger `OnActiveUserPrefServiceChange`.
  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kOnline, actual_info.type);
  EXPECT_EQ(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
            actual_info.collection_id);

  // Keep OOBE state.
  SetSessionState(SessionState::OOBE);
  OnlineWallpaperVariant variant(kAssetId, GURL(kDummyUrl),
                                 backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo synced_info = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER_CROPPED,
          /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId, {variant}),
      variant);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(TestWallpaperControllerClient::kDummyCollectionId,
            actual_info.collection_id);
  EXPECT_TRUE(base::PathExists(online_wallpaper_dir_.GetPath().Append(
      GURL(kDummyUrl).ExtractFileName())));
}

TEST_P(WallpaperControllerTest,
       ActiveUserPrefServiceChanged_OOBEForSecondUser_SetTimeOfDayWallpaper) {
  if (!IsTimeOfDayEnabled()) {
    return;
  }
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kDefault);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);
  SetSessionState(SessionState::LOGIN_PRIMARY);
  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::GAIA_SIGNIN);
  // Log in and trigger `OnActiveUserPrefServiceChange`.
  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kOnline, actual_info.type);
  EXPECT_EQ(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
            actual_info.collection_id);
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.IsSetToTimeOfDayAfterOobe",
                                      1);
}

TEST_P(
    WallpaperControllerTest,
    ActiveUserPrefServiceChanged_OOBEForSecondUser_SetPolicyWallpaper_TimeOfDayEnabled) {
  if (!IsTimeOfDayEnabled()) {
    return;
  }
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kDefault);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);
  SetSessionState(SessionState::LOGIN_PRIMARY);
  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      OobeDialogState::GAIA_SIGNIN);
  // Log in and trigger `OnActiveUserPrefServiceChange`.
  SimulateUserLogin(kAccountId1);
  controller_->SetPolicyWallpaper(
      kAccountId1, user_manager::UserType::kRegular,
      CreateEncodedImageForTesting(gfx::Size(10, 10)));
  RunAllTasksUntilIdle();
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  WallpaperInfo policy_wallpaper_info(base::FilePath(kWallpaperFilesId1)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED,
                                      WallpaperType::kPolicy,
                                      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(actual_info.MatchesSelection(policy_wallpaper_info));
  EXPECT_TRUE(controller_->IsWallpaperControlledByPolicy(kAccountId1));
}

TEST_P(WallpaperControllerTest,
       ActiveUserPrefServiceChanged_NonOOBE_SetTimeOfDayWallpaper) {
  if (!IsTimeOfDayEnabled()) {
    return;
  }
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);
  WallpaperInfo local_info = InfoWithType(WallpaperType::kDefault);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);
  // Log in and trigger `OnActiveUserPrefServiceChange`.
  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(local_info.MatchesAsset(actual_info));
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.IsSetToTimeOfDayAfterOobe",
                                      0);
}

TEST_P(WallpaperControllerTest, SetAndRemovePolicyWallpaper) {
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
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);

  // Set a policy wallpaper. Verify that the user becomes policy controlled and
  // the wallpaper info is updated.
  ClearWallpaperCount();
  controller_->SetPolicyWallpaper(
      kAccountId1, user_manager::UserType::kRegular,
      CreateEncodedImageForTesting(gfx::Size(10, 10)));
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
TEST_P(WallpaperControllerTest, ShowUserWallpaper_OriginalFallback) {
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
TEST_P(WallpaperControllerTest, ShowUserWallpaper_MissingFile) {
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
  controller_->RemoveUserWallpaper(kAccountId1, base::DoNothing());
  WaitUntilCustomWallpapersDeleted(kAccountId1);
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

TEST_P(WallpaperControllerTest, RemovePolicyWallpaperNoOp) {
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

TEST_P(WallpaperControllerTest, SetThirdPartyWallpaper) {
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

TEST_P(WallpaperControllerTest, SetThirdPartyWallpaper_NonactiveUser) {
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

TEST_P(WallpaperControllerTest, SetThirdPartyWallpaper_PolicyWallpaper) {
  SimulateUserLogin(kAccountId2);
  WallpaperInfo wallpaper_info;
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia third_party_wallpaper = CreateImage(640, 480, kWallpaperColor);
  // Set a policy wallpaper for |kUser2|. Verify that |kUser2| becomes policy
  // controlled.
  controller_->SetPolicyWallpaper(
      kAccountId2, user_manager::UserType::kRegular,
      CreateEncodedImageForTesting(gfx::Size(10, 10)));
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

TEST_P(WallpaperControllerTest, SetSeaPenWallpaper) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo wallpaper_info;
  ASSERT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  gfx::ImageSkia expected_image;
  SetSeaPenWallpaper(kAccountId1, SK_ColorGREEN, /*id=*/777u,
                     /*preview_mode=*/false, &expected_image);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_EQ(WallpaperType::kSeaPen, wallpaper_info.type);
  EXPECT_EQ("777", wallpaper_info.location);
  EXPECT_TRUE(wallpaper_info.user_file_path.empty());

  // Use `AreBitmapsClose` because jpg encoding/decoding can alter the color
  // channels +- 1.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      *expected_image.bitmap(), *controller_->GetWallpaperImage().bitmap(),
      /*max_deviation=*/1));

  base::FileEnumerator file_enumerator(online_wallpaper_dir_.GetPath(),
                                       /*recursive=*/true,
                                       base::FileEnumerator::FileType::FILES);

  std::vector<base::FilePath> wallpaper_files;
  for (auto path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    wallpaper_files.push_back(path);
  }

  // One SeaPen image file saved to global wallpaper directory for account.
  EXPECT_EQ(std::vector<base::FilePath>(
                {base::FilePath(online_wallpaper_dir_.GetPath())
                     .Append(wallpaper_constants::kSeaPenWallpaperDirName)
                     .Append(kAccountId1.GetAccountIdKey())
                     .Append("777")
                     .AddExtension(".jpg")}),
            wallpaper_files);
}

TEST_P(WallpaperControllerTest,
       SeaPenWallpaperRemovedAfterSettingAnotherWallpaperType) {
  const auto global_sea_pen_dir =
      online_wallpaper_dir_.GetPath().Append("sea_pen").Append(
          kAccountId1.GetAccountIdKey());

  SimulateUserLogin(kAccountId1);

  WallpaperInfo wallpaper_info;
  ASSERT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  {
    // Sets a sea pen wallpaper.
    gfx::ImageSkia expected_image;
    SetSeaPenWallpaper(kAccountId1, SK_ColorGREEN, /*id=*/848u,
                       /*preview_mode=*/false, &expected_image);
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_EQ(WallpaperType::kSeaPen, wallpaper_info.type);
    EXPECT_EQ("848", wallpaper_info.location);
    EXPECT_TRUE(wallpaper_info.user_file_path.empty());
    // Expects the sea pen wallpaper is saved to the global SeaPen directory.
    ASSERT_TRUE(
        base::PathExists(global_sea_pen_dir.Append(wallpaper_info.location)
                             .ReplaceExtension(".jpg")));
  }

  {
    // Sets an online wallpaper.
    base::RunLoop run_loop;
    const OnlineWallpaperParams& params = OnlineWallpaperParams(
        kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
        WALLPAPER_LAYOUT_CENTER_CROPPED,
        /*preview_mode=*/false, /*from_user=*/true,
        /*daily_refresh_enabled=*/false, kUnitId,
        /*variants=*/
        {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}});
    controller_->SetOnlineWallpaper(
        params, base::BindLambdaForTesting(
                    [quit = run_loop.QuitClosure()](bool success) {
                      EXPECT_TRUE(success);
                      std::move(quit).Run();
                    }));
    run_loop.Run();
    EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
  }

  // Waits for clean up tasks to finish.
  RunAllTasksUntilIdle();

  // Expects the sea pen wallpaper is removed from the global SeaPen directory.
  ASSERT_FALSE(
      base::PathExists(global_sea_pen_dir.Append(wallpaper_info.location)
                           .ReplaceExtension(".jpg")));
}

TEST_P(WallpaperControllerTest, ShowSeaPenWallpaperOnLogin) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo wallpaper_info;
  ASSERT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  gfx::ImageSkia expected_image;
  SetSeaPenWallpaper(kAccountId1, SK_ColorBLUE, 888u, /*preview_mode=*/false,
                     &expected_image);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_EQ(WallpaperType::kSeaPen, wallpaper_info.type);
  EXPECT_EQ("888", wallpaper_info.location);
  EXPECT_TRUE(wallpaper_info.user_file_path.empty());

  // Simulates device reboot.
  controller_->ReloadWallpaperForTesting(/*clear_cache=*/true);
  ClearWallpaper();
  ClearLogin();
  SimulateUserLogin(kAccountId1);
  const AccountId active_account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  controller_->ShowUserWallpaper(active_account_id);
  RunAllTasksUntilIdle();

  WallpaperInfo new_wallpaper_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(active_account_id,
                                                  &new_wallpaper_info));
  EXPECT_EQ(WallpaperType::kSeaPen, new_wallpaper_info.type);
  EXPECT_TRUE(wallpaper_info.MatchesAsset(new_wallpaper_info));

  // Use `AreBitmapsClose` because jpg encoding/decoding can alter the color
  // channels +- 1.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      *expected_image.bitmap(), *controller_->GetWallpaperImage().bitmap(),
      /*max_deviation=*/1));
}

TEST_P(WallpaperControllerTest, LoadsSeaPenWallpaperWithInvalidUserFilePath) {
  // info.user_file_path should be ignored, but older versions may have invalid
  // strings in it. Write an older WallpaperInfo to prefs.
  ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(
      kAccountId1, WallpaperInfo("1", WALLPAPER_LAYOUT_CENTER_CROPPED,
                                 WallpaperType::kSeaPen, base::Time::Now(),
                                 "invalid_user_file_path.jpg")));

  gfx::ImageSkia created_image;
  {
    // Write a corresponding jpg to disk in the correct place.
    std::string jpg_bytes = CreateEncodedImageForTesting(
        {1, 1}, SK_ColorBLUE, data_decoder::mojom::ImageCodec::kDefault,
        &created_image);
    ASSERT_FALSE(jpg_bytes.empty());

    base::test::TestFuture<bool> save_sea_pen_image_future;
    SeaPenWallpaperManager::GetInstance()->SaveSeaPenImage(
        kAccountId1, {std::move(jpg_bytes), 1u},
        personalization_app::mojom::SeaPenQuery::NewTextQuery("search_query"),
        save_sea_pen_image_future.GetCallback());

    ASSERT_TRUE(save_sea_pen_image_future.Get());
  }

  {
    // Log in.
    SimulateUserLogin(kAccountId1);
    controller_->ShowUserWallpaper(kAccountId1);
    RunAllTasksUntilIdle();
  }

  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      *created_image.bitmap(), *controller_->GetWallpaperImage().bitmap(),
      /*max_deviation=*/1));
}

// TODO(crbug.com/41484478): Flaky on linux-chromeos-rel.
TEST_P(WallpaperControllerTest, DISABLED_SetSeaPenWallpaperFromFile) {
  SimulateUserLogin(kAccountId1);
  TestWallpaperControllerObserver observer(controller_);

  WallpaperInfo wallpaper_info;
  ASSERT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  gfx::ImageSkia expected_image;
  std::string jpg_bytes = CreateEncodedImageForTesting(
      {1, 1}, SK_ColorGREEN, data_decoder::mojom::ImageCodec::kDefault,
      &expected_image);
  ASSERT_TRUE(!jpg_bytes.empty());

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = scoped_temp_dir.GetPath().Append("111.jpg");
  ASSERT_TRUE(base::WriteFile(file_path, jpg_bytes));
  // Updates the last modified time for the file.
  ASSERT_TRUE(base::TouchFile(file_path, base::Time::Now() - base::Minutes(5),
                              base::Time::Now() - base::Minutes(5)));
  base::Time old_last_modified_time = GetLastModifiedTime(file_path);

  base::test::TestFuture<bool> set_wallpaper_future;
  controller_->SetSeaPenWallpaper(kAccountId1, 111u, /*preview_mode=*/false,
                                  set_wallpaper_future.GetCallback());

  EXPECT_TRUE(set_wallpaper_future.Take());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_EQ(WallpaperType::kSeaPen, wallpaper_info.type);
  EXPECT_EQ(1, observer.wallpaper_changed_count());
  histogram_tester().ExpectUniqueSample("Ash.Wallpaper.SeaPen.Result2",
                                        SetWallpaperResult::kSuccess, 1);
  // Use `AreBitmapsClose` because jpg encoding/decoding can alter the color
  // channels +- 1.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      *expected_image.bitmap(), *controller_->GetWallpaperImage().bitmap(),
      /*max_deviation=*/1));

  // Last Modified Time should be updated to current time.
  EXPECT_TRUE(GetLastModifiedTime(file_path) > old_last_modified_time);
}

TEST_P(WallpaperControllerTest, CancelSetSeaPenWallpaperInTabletMode) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo wallpaper_info;
  ASSERT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  gfx::ImageSkia expected_image;
  TestWallpaperControllerObserver observer(controller_);
  SetSeaPenWallpaper(kAccountId1, SK_ColorBLUE, /*id=*/777u,
                     /*preview_mode=*/true, &expected_image);
  RunAllTasksUntilIdle();

  EXPECT_TRUE(observer.is_in_wallpaper_preview());

  controller_->CancelPreviewWallpaper();

  EXPECT_FALSE(observer.is_in_wallpaper_preview());
  ASSERT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
}

TEST_P(WallpaperControllerTest, ConfirmSetSeaPenWallpaperInTabletMode) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo wallpaper_info;
  ASSERT_FALSE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));

  gfx::ImageSkia expected_image;
  TestWallpaperControllerObserver observer(controller_);
  SetSeaPenWallpaper(kAccountId1, SK_ColorGREEN, /*id=*/777u,
                     /*preview_mode=*/true, &expected_image);
  RunAllTasksUntilIdle();

  EXPECT_TRUE(observer.is_in_wallpaper_preview());

  controller_->ConfirmPreviewWallpaper();

  EXPECT_FALSE(observer.is_in_wallpaper_preview());
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  EXPECT_EQ(WallpaperType::kSeaPen, wallpaper_info.type);
  EXPECT_EQ("777", wallpaper_info.location);
  EXPECT_TRUE(wallpaper_info.user_file_path.empty());

  // Use `AreBitmapsClose` because jpg encoding/decoding can alter the color
  // channels +- 1.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      *expected_image.bitmap(), *controller_->GetWallpaperImage().bitmap(),
      /*max_deviation=*/1));

  base::FileEnumerator file_enumerator(online_wallpaper_dir_.GetPath(),
                                       /*recursive=*/true,
                                       base::FileEnumerator::FileType::FILES);

  std::vector<base::FilePath> wallpaper_files;
  for (auto path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    wallpaper_files.push_back(path);
  }

  // One SeaPen image file saved to global wallpaper directory for account.
  EXPECT_EQ(std::vector<base::FilePath>(
                {base::FilePath(online_wallpaper_dir_.GetPath())
                     .Append(wallpaper_constants::kSeaPenWallpaperDirName)
                     .Append(kAccountId1.GetAccountIdKey())
                     .Append("777")
                     .AddExtension(".jpg")}),
            wallpaper_files);
}

TEST_P(WallpaperControllerTest, SeaPenMigrateFiles) {
  constexpr std::array<uint32_t, 2> kImageIds = {888, 999};

  const auto global_sea_pen_dir =
      online_wallpaper_dir_.GetPath()
          .Append(wallpaper_constants::kSeaPenWallpaperDirName)
          .Append(kAccountId1.GetAccountIdKey());
  ASSERT_TRUE(base::CreateDirectory(global_sea_pen_dir));

  {
    // Write files to the global SeaPen directory.
    for (const auto id : kImageIds) {
      ResizeAndSaveWallpaper(
          gfx::test::CreateImageSkia(2),
          global_sea_pen_dir.Append(base::NumberToString(id))
              .AddExtension(".jpg"),
          WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED, {2, 2},
          QueryDictToXmpString(SeaPenQueryToDict(
              personalization_app::mojom::SeaPenQuery::NewTextQuery(
                  "testing query"))));
    }

    // Set the first one as the user's wallpaper info.
    ASSERT_TRUE(pref_manager_->SetUserWallpaperInfo(
        kAccountId1, WallpaperInfo(base::NumberToString(kImageIds.front()),
                                   WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kSeaPen, base::Time::Now())));
  }

  {
    // SeaPenWallpaperManager sees no files since they are not yet migrated.
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    SeaPenWallpaperManager::GetInstance()->GetImageIds(
        kAccountId1, get_image_ids_future.GetCallback());
    ASSERT_TRUE(get_image_ids_future.Get().empty());
  }

  ASSERT_TRUE(
      SeaPenWallpaperManager::GetInstance()->ShouldMigrate(kAccountId1));

  PrefChangeRegistrar pref_change_registrar;
  auto* pref_service = SeaPenWallpaperManager::GetInstance()
                           ->session_delegate_for_testing()
                           ->GetPrefService(kAccountId1);
  pref_change_registrar.Init(pref_service);
  base::test::RepeatingTestFuture<const std::string&> pref_changed_future;
  pref_change_registrar.Add(prefs::kWallpaperSeaPenMigrationStatus,
                            pref_changed_future.GetCallback());

  SimulateUserLogin(kAccountId1);

  {
    // Writes kCrashed first.
    ASSERT_EQ(prefs::kWallpaperSeaPenMigrationStatus,
              pref_changed_future.Take());
    EXPECT_FALSE(
        SeaPenWallpaperManager::GetInstance()->ShouldMigrate(kAccountId1));
    EXPECT_EQ(
        SeaPenWallpaperManager::MigrationStatus::kCrashed,
        static_cast<SeaPenWallpaperManager::MigrationStatus>(
            pref_service->GetInteger(prefs::kWallpaperSeaPenMigrationStatus)));

    // Performs migration and then writes kSuccess.
    ASSERT_EQ(prefs::kWallpaperSeaPenMigrationStatus,
              pref_changed_future.Take());
    EXPECT_FALSE(
        SeaPenWallpaperManager::GetInstance()->ShouldMigrate(kAccountId1));
    EXPECT_EQ(
        SeaPenWallpaperManager::MigrationStatus::kSuccess,
        static_cast<SeaPenWallpaperManager::MigrationStatus>(
            pref_service->GetInteger(prefs::kWallpaperSeaPenMigrationStatus)));
  }

  {
    // SeaPenWallpaperManager sees files since they have been migrated;
    base::test::TestFuture<const std::vector<uint32_t>&> get_image_ids_future;
    SeaPenWallpaperManager::GetInstance()->GetImageIds(
        kAccountId1, get_image_ids_future.GetCallback());
    EXPECT_THAT(get_image_ids_future.Get(),
                testing::UnorderedElementsAreArray(kImageIds));
  }

  RunAllTasksUntilIdle();

  {
    // The active wallpaper is copied back to the global directory.
    EXPECT_TRUE(base::PathExists(
        global_sea_pen_dir.Append(base::NumberToString(kImageIds.front()))
            .AddExtension(".jpg")));

    // The inactive file is not copied back.
    EXPECT_FALSE(base::PathExists(
        global_sea_pen_dir.Append(base::NumberToString(kImageIds.back()))
            .AddExtension(".jpg")));
  }
}

TEST_P(WallpaperControllerTest, SetSeaPenWallpaperForPublicAccount) {
  ClearLogin();

  const AccountId account_id = AccountId::FromUserEmail("public_session");
  SimulateUserLogin(account_id, user_manager::UserType::kPublicAccount);

  gfx::ImageSkia expected_image;
  SetSeaPenWallpaper(account_id, SK_ColorBLUE, 12345u, /*preview_mode=*/false,
                     &expected_image);

  WallpaperInfo wallpaper_info;
  ASSERT_TRUE(pref_manager_->GetUserWallpaperInfo(account_id, &wallpaper_info));
  EXPECT_EQ(WallpaperType::kSeaPen, wallpaper_info.type);
  EXPECT_EQ("12345", wallpaper_info.location);

  // Use `AreBitmapsClose` because jpg encoding/decoding can alter the color
  // channels +- 1.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      *expected_image.bitmap(), *controller_->GetWallpaperImage().bitmap(),
      /*max_deviation=*/1));

  base::FileEnumerator file_enumerator(online_wallpaper_dir_.GetPath(),
                                       /*recursive=*/true,
                                       base::FileEnumerator::FileType::FILES);

  std::vector<base::FilePath> wallpaper_files;
  for (auto path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    wallpaper_files.push_back(path);
  }

  // No wallpaper files saved to global wallpaper directory for public account.
  EXPECT_TRUE(wallpaper_files.empty());
}

TEST_P(WallpaperControllerTest, SetDefaultWallpaperForRegularAccount) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(kAccountId1);
  // Called to make sure `WallpaperControllerImpl::current_user_` is properly
  // set.
  controller_->ShowUserWallpaper(kAccountId1);

  // First, simulate setting a user custom wallpaper.
  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"", image);
  RunAllTasksUntilIdle();
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

  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"", image);
  RunAllTasksUntilIdle();
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

  controller_->SetDecodedCustomWallpaper(
      kAccountId1, kFileName1, WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::DoNothing(), /*file_path=*/"", image);
  RunAllTasksUntilIdle();
  // Verify that when screen is rotated, |SetDefaultWallpaper| removes the
  // previously set custom wallpaper info, and the small default wallpaper is
  // set successfully from the cache.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(kAccountId1, true /*show_wallpaper=*/,
                                   base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(0u, GetDecodeFilePaths().size());

  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  // The user wallpaper info has been reset to the default value.
  EXPECT_TRUE(wallpaper_info.MatchesSelection(default_wallpaper_info));
}

TEST_P(WallpaperControllerTest, SetDefaultWallpaperForChildAccount) {
  SimulateUserLogin(kChildAccountId, user_manager::UserType::kChild);

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
TEST_P(WallpaperControllerTest,
       SetDefaultWallpaperForGuestSessionUnaffectedByWallpaperPolicy) {
  // Simulate the login screen.
  ClearLogin();
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
  SimulateUserLogin(guest_id, user_manager::UserType::kGuest);
  controller_->SetDefaultWallpaper(guest_id, /*show_wallpaper=*/true,
                                   base::DoNothing());
  RunAllTasksUntilIdle();

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
  controller_->SetPolicyWallpaper(
      kAccountId1, user_manager::UserType::kRegular,
      CreateEncodedImageForTesting(gfx::Size(10, 10)));
  RunAllTasksUntilIdle();
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

TEST_P(WallpaperControllerTest, SetDefaultWallpaperForGuestSessionAndPreview) {
  const AccountId guest_id =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  controller_->ShowUserWallpaper(guest_id);
  SimulateUserLogin(guest_id, user_manager::UserType::kGuest);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_EQ(wallpaper_info.type, WallpaperType::kDefault);
}

TEST_P(WallpaperControllerTest, SetDefaultWallpaperForGuestSession) {
  // First, simulate setting a custom wallpaper for a regular user.
  SimulateUserLogin(kAccountId1);
  CreateAndSaveWallpapers(kAccountId1);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo default_wallpaper_info(
      std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kDefault,
      base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  const AccountId guest_id =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  SimulateUserLogin(guest_id, user_manager::UserType::kGuest);

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

TEST_P(WallpaperControllerTest, SetDefaultWallpaperCallbackTiming) {
  SimulateUserLogin(kAccountId1);

  // First, simulate setting a user custom wallpaper.
  CreateAndSaveWallpapers(kAccountId1);
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

TEST_P(WallpaperControllerTest, IgnoreWallpaperRequestInKioskMode) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin("kiosk", user_manager::UserType::kKioskApp);

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
      OnlineWallpaperParams(
          kAccountId1,
          /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER,
          /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId,
          /*variants=*/
          {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}}),
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

TEST_P(WallpaperControllerTest, IgnoreWallpaperRequestWhenPolicyIsEnforced) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(kAccountId1);

  // Set a policy wallpaper for the user. Verify the user is policy controlled.
  controller_->SetPolicyWallpaper(
      kAccountId1, user_manager::UserType::kRegular,
      CreateEncodedImageForTesting(gfx::Size(10, 10)));
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
    base::RunLoop run_loop;
    ClearWallpaperCount();
    controller_->SetCustomWallpaper(
        kAccountId1, base::FilePath(kFileName1), WALLPAPER_LAYOUT_CENTER,
        /*preview_mode=*/false,
        base::BindLambdaForTesting(
            [quit = run_loop.QuitClosure()](bool success) {
              EXPECT_FALSE(success);
              std::move(quit).Run();
            }));
    run_loop.Run();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }

  {
    // Verify that |SetOnlineWallpaper| doesn't set wallpaper when
    // policy is enforced, and the user wallpaper info is not updated.
    base::RunLoop run_loop;
    ClearWallpaperCount();
    controller_->SetOnlineWallpaper(
        OnlineWallpaperParams(
            kAccountId1,
            /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
            /*preview_mode=*/false, /*from_user=*/false,
            /*daily_refresh_enabled=*/false, kUnitId,
            /*variants=*/
            {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}}),
        base::BindLambdaForTesting(
            [quit = run_loop.QuitClosure()](bool success) {
              EXPECT_FALSE(success);
              std::move(quit).Run();
            }));
    run_loop.Run();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }

  {
    // Verify that |SetOnlineWallpaper| doesn't set wallpaper when policy is
    // enforced, and the user wallpaper info is not updated.
    base::RunLoop run_loop;
    ClearWallpaperCount();
    controller_->SetOnlineWallpaper(
        OnlineWallpaperParams(
            kAccountId1,
            /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
            /*preview_mode=*/false, /*from_user=*/false,
            /*daily_refresh_enabled=*/false, kUnitId,
            /*variants=*/
            {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}}),
        base::BindLambdaForTesting(
            [quit = run_loop.QuitClosure()](bool success) {
              EXPECT_FALSE(success);
              std::move(quit).Run();
            }));
    run_loop.Run();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }

  {
    // Verify that |SetDefaultWallpaper| doesn't set wallpaper when policy is
    // enforced, and the user wallpaper info is not updated.
    base::RunLoop run_loop;
    ClearWallpaperCount();
    controller_->SetDefaultWallpaper(
        kAccountId1, true /*show_wallpaper=*/,
        base::BindLambdaForTesting(
            [quit = run_loop.QuitClosure()](bool success) {
              EXPECT_FALSE(success);
              std::move(quit).Run();
            }));
    run_loop.Run();
    EXPECT_EQ(0, GetWallpaperCount());
    EXPECT_TRUE(
        pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
    EXPECT_TRUE(wallpaper_info.MatchesSelection(policy_wallpaper_info));
  }
}

TEST_P(WallpaperControllerTest, VerifyWallpaperCache) {
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
      OnlineWallpaperParams(
          kAccountId1,
          /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
          /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId,
          /*variants=*/
          {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}}),
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
TEST_P(WallpaperControllerTest, ShowCustomWallpaperWithCorrectResolution) {
  const base::FilePath small_custom_wallpaper_path = GetCustomWallpaperPath(
      kSmallWallpaperSubDir, kWallpaperFilesId1, kFileName1);
  const base::FilePath large_custom_wallpaper_path = GetCustomWallpaperPath(
      kLargeWallpaperSubDir, kWallpaperFilesId1, kFileName1);

  CreateAndSaveWallpapers(kAccountId1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(kAccountId1);
  RunAllTasksUntilIdle();
  // Display is initialized to 800x600. The small resolution custom wallpaper is
  // expected.
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(small_custom_wallpaper_path, GetDecodeFilePaths()[0]);

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
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(large_custom_wallpaper_path, GetDecodeFilePaths()[0]);

  // Detach the secondary display.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  // Hook up the 3000x2000 display again. Test for crbug/165788.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,3000x2000");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(large_custom_wallpaper_path, GetDecodeFilePaths()[0]);
}

// After the display is rotated, the sign in wallpaper should be kept. Test for
// crbug.com/794725.
TEST_P(WallpaperControllerTest, SigninWallpaperIsKeptAfterRotation) {
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
  // still be set from cache, instead of a custom wallpaper.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
  ASSERT_EQ(0u, GetDecodeFilePaths().size());
}

// Display size change should trigger wallpaper reload.
TEST_P(WallpaperControllerTest, ReloadWallpaper) {
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

TEST_P(WallpaperControllerTest, UpdateCurrentWallpaperLayout) {
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
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1,
      /*collection_id=*/std::string(), layout,
      /*preview_mode=*/false, /*from_user=*/false,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/
      {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}});
  controller_->SetOnlineWallpaper(params, base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info));
  WallpaperInfo expected_online_wallpaper_info(params, params.variants.front());
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
TEST_P(WallpaperControllerTest, RemoveUserWithCustomWallpaper) {
  SimulateUserLogin(kAccountId1);
  base::FilePath small_wallpaper_path_1 = GetCustomWallpaperPath(
      kSmallWallpaperSubDir, kWallpaperFilesId1, kFileName1);

  // Set a custom wallpaper for |kUser1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(kAccountId1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a custom wallpaper for the user.
  SimulateUserLogin(kAccountId2);
  base::FilePath small_wallpaper_path_2 = GetCustomWallpaperPath(
      kSmallWallpaperSubDir, kWallpaperFilesId2, GetDummyFileName(kAccountId2));
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
TEST_P(WallpaperControllerTest, RemoveUserWithDefaultWallpaper) {
  SimulateUserLogin(kAccountId1);
  base::FilePath small_wallpaper_path_1 = GetCustomWallpaperPath(
      kSmallWallpaperSubDir, kWallpaperFilesId1, kFileName1);
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
TEST_P(WallpaperControllerTest, RemoveUserWallpaperOnRemoveCallbackCalled) {
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

TEST_P(WallpaperControllerTest, IsActiveUserWallpaperControlledByPolicy) {
  // Simulate the login screen. Verify that it returns false since there's no
  // active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  SimulateUserLogin(kAccountId1);
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());
  // Set a policy wallpaper for the active user. Verify that the active user
  // becomes policy controlled.
  controller_->SetPolicyWallpaper(
      kAccountId1, user_manager::UserType::kRegular,
      CreateEncodedImageForTesting(gfx::Size(10, 10)));
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Switch the active user. Verify the active user is not policy controlled.
  SimulateUserLogin(kAccountId2);
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Logs out. Verify that it returns false since there's no active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());
}

TEST_P(WallpaperControllerTest,
       IsManagedGuestSessionWallpaperControlledByPolicy) {
  // Simulate the login screen. Verify that it returns false since there's no
  // active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Set a policy wallpaper for the managed guest session. Verify that the
  // managed guest session becomes policy controlled.
  controller_->SetPolicyWallpaper(
      kAccountId1, user_manager::UserType::kPublicAccount,
      CreateEncodedImageForTesting(gfx::Size(10, 10)));
  SimulateUserLogin(kAccountId1, user_manager::UserType::kPublicAccount);
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

TEST_P(WallpaperControllerTest, WallpaperBlur) {
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

TEST_P(WallpaperControllerTest, WallpaperBlurDuringLockScreenTransition) {
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

  const bool forest_enabled = features::IsForestFeatureEnabled();
  if (forest_enabled) {
    // There are three layers: underlay, original and old layers.
    ASSERT_EQ(3u, wallpaper_view()->layer()->parent()->children().size());
    EXPECT_EQ(ui::LAYER_SOLID_COLOR,
              wallpaper_view()->layer()->parent()->children()[0]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[1]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[2]->type());
  } else {
    ASSERT_EQ(2u, wallpaper_view()->layer()->parent()->children().size());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[0]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[1]->type());
  }

  // Simulate lock and unlock sequence.
  controller_->UpdateWallpaperBlurForLockState(true);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(1, observer.blur_changed_count());

  SetSessionState(SessionState::LOCKED);
  EXPECT_TRUE(controller_->IsWallpaperBlurredForLockState());
  if (forest_enabled) {
    // There are four layers: shield, underlay, original and old layers.
    ASSERT_EQ(4u, wallpaper_view()->layer()->parent()->children().size());
    EXPECT_EQ(ui::LAYER_SOLID_COLOR,
              wallpaper_view()->layer()->parent()->children()[0]->type());
    EXPECT_EQ(ui::LAYER_SOLID_COLOR,
              wallpaper_view()->layer()->parent()->children()[1]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[2]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[3]->type());
  } else {
    ASSERT_EQ(3u, wallpaper_view()->layer()->parent()->children().size());
    EXPECT_EQ(ui::LAYER_SOLID_COLOR,
              wallpaper_view()->layer()->parent()->children()[0]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[1]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[2]->type());
  }

  // Change of state to ACTIVE triggers post lock animation and
  // UpdateWallpaperBlur(false)
  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurredForLockState());
  EXPECT_EQ(2, observer.blur_changed_count());
  if (forest_enabled) {
    // There are three layers: underlay, original and old layers.
    ASSERT_EQ(3u, wallpaper_view()->layer()->parent()->children().size());
    EXPECT_EQ(ui::LAYER_SOLID_COLOR,
              wallpaper_view()->layer()->parent()->children()[0]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[1]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[2]->type());
  } else {
    ASSERT_EQ(2u, wallpaper_view()->layer()->parent()->children().size());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[0]->type());
    EXPECT_EQ(ui::LAYER_TEXTURED,
              wallpaper_view()->layer()->parent()->children()[1]->type());
  }
}

TEST_P(WallpaperControllerTest, LockDuringOverview) {
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

TEST_P(WallpaperControllerTest, DontLeakShieldView) {
  SetSessionState(SessionState::LOCKED);
  views::View* shield_view = wallpaper_view()->shield_view_for_testing();
  ASSERT_TRUE(shield_view);
  views::ViewTracker view_tracker(shield_view);
  SetSessionState(SessionState::ACTIVE);
  EXPECT_EQ(nullptr, wallpaper_view()->shield_view_for_testing());
  EXPECT_EQ(nullptr, view_tracker.view());
}

TEST_P(WallpaperControllerTest, OnlyShowDevicePolicyWallpaperOnLoginScreen) {
  // Make sure the device policy path exists so decoding succeeds.
  ASSERT_TRUE(WriteJPEGFile(user_data_dir_.GetPath().Append(
                                base::FilePath(kDefaultSmallWallpaperName)),
                            /*width=*/2, /*height=*/2, kWallpaperColor));
  // Verify the device policy wallpaper is shown on login screen.
  SetSessionState(SessionState::LOGIN_PRIMARY);
  controller_->SetDevicePolicyWallpaperPath(user_data_dir_.GetPath().Append(
      base::FilePath(kDefaultSmallWallpaperName)));
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

TEST_P(WallpaperControllerTest, ShouldShowInitialAnimationAfterBoot) {
  CreateAndSaveWallpapers(kChildAccountId);

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

TEST_P(WallpaperControllerTest, ShouldNotShowInitialAnimationAfterSignOut) {
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

TEST_P(WallpaperControllerTest, ClosePreviewWallpaperOnOverviewStart) {
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
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
}

TEST_P(WallpaperControllerTest, ClosePreviewWallpaperOnWindowCycleStart) {
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

TEST_P(WallpaperControllerTest,
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

TEST_P(WallpaperControllerTest, ConfirmPreviewWallpaper) {
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
      /*preview_mode=*/true, /*from_user=*/true, kUnitId,
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
  TestWallpaperControllerObserver observer(controller_);
  run_loop = std::make_unique<base::RunLoop>();
  observer.SetOnResizeCallback(run_loop->QuitClosure());
  SetOnlineWallpaperFromImage(
      kAccountId1, kAssetId, online_wallpaper, kDummyUrl,
      TestWallpaperControllerClient::kDummyCollectionId, layout,
      /*preview_mode=*/true, /*from_user=*/true, kUnitId,
      base::BindLambdaForTesting([](bool success) { EXPECT_TRUE(success); }));
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
  OnlineWallpaperVariant variant(kAssetId, GURL(kDummyUrl),
                                 backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo online_wallpaper_info(
      OnlineWallpaperParams(kAccountId1,
                            TestWallpaperControllerClient::kDummyCollectionId,
                            layout,
                            /*preview_mode=*/false,
                            /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId,
                            /*variants=*/{variant}),
      variant);
  EXPECT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &user_wallpaper_info));
  EXPECT_TRUE(user_wallpaper_info.MatchesSelection(online_wallpaper_info));
}

TEST_P(WallpaperControllerTest, CancelPreviewWallpaper) {
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
      /*preview_mode=*/true, /*from_user=*/true, kUnitId, base::DoNothing());
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

TEST_P(WallpaperControllerTest, WallpaperSyncedDuringPreview) {
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
      /*preview_mode=*/true, /*from_user=*/true, kUnitId, base::DoNothing());
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
      /*preview_mode=*/false,
      /*from_user=*/true, kUnitId, base::DoNothing());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // However, the user wallpaper info should already be updated to the new info.
  OnlineWallpaperVariant variant(kAssetId, GURL(kDummyUrl2),
                                 backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo synced_online_wallpaper_info = WallpaperInfo(
      OnlineWallpaperParams(kAccountId1,
                            TestWallpaperControllerClient::kDummyCollectionId,
                            layout,
                            /*preview_mode=*/false,
                            /*from_user=*/true,
                            /*daily_refresh_enabled=*/false, kUnitId,
                            /*variants=*/{variant}),
      variant);
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

TEST_P(WallpaperControllerTest, AddFirstWallpaperAnimationEndCallback) {
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

TEST_P(WallpaperControllerTest, ShowOneShotWallpaper) {
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

TEST_P(WallpaperControllerTest, OnFirstWallpaperShown) {
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
TEST_P(WallpaperControllerTest, ShowWallpaperForEphemeralUser) {
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
    : public WallpaperControllerTestBase,
      public testing::WithParamInterface</*BootAnimation*/ bool> {
 public:
  WallpaperControllerOobeWallpaperTest() {
    const bool boot_animation = GetParam();
    scoped_feature_list_.InitWithFeatureStates({
        {features::kFeatureManagementOobeSimon, boot_animation},
    });
  }
  ~WallpaperControllerOobeWallpaperTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WallpaperControllerOobeWallpaperTest,
                         /*BootAnimation*/ testing::Bool());

TEST_P(WallpaperControllerOobeWallpaperTest, ShowOobeWallpaper) {
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
    : public WallpaperControllerTestBase,
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
    scoped_feature_list_.InitWithFeatures(
        personalization_app::GetTimeOfDayEnabledFeatures(), {});
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

TEST_P(WallpaperControllerTest, NoAnimationForNewRootWindowWhenLocked) {
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

TEST_P(WallpaperControllerTest, SetCustomWallpaper) {
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

TEST_P(WallpaperControllerTest, OldOnlineInfoSynced_Discarded) {
  // Create a dictionary that looks like the preference from crrev.com/a040384.
  // DO NOT CHANGE as there are preferences like this in production.
  base::Value::Dict wallpaper_info_dict;
  wallpaper_info_dict.Set(
      WallpaperInfo::kNewWallpaperDateNodeName,
      base::NumberToString(
          base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds()));
  wallpaper_info_dict.Set(WallpaperInfo::kNewWallpaperLocationNodeName,
                          "location");
  wallpaper_info_dict.Set(WallpaperInfo::kNewWallpaperUserFilePathNodeName,
                          "user_file_path");
  wallpaper_info_dict.Set(WallpaperInfo::kNewWallpaperLayoutNodeName,
                          WallpaperLayout::WALLPAPER_LAYOUT_CENTER);
  wallpaper_info_dict.Set(WallpaperInfo::kNewWallpaperTypeNodeName,
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

TEST_P(WallpaperControllerTest, MigrateWallpaperInfo_Online) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kOnline);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, expected_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  EXPECT_TRUE(info.MatchesSelection(expected_info));
}

TEST_P(WallpaperControllerTest, MigrateWallpaperInfoCustomized) {
  WallpaperInfo expected_info = InfoWithType(WallpaperType::kCustomized);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, expected_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  EXPECT_TRUE(info.MatchesSelection(expected_info));
}

TEST_P(WallpaperControllerTest, MigrateWallpaperInfoDaily) {
  OnlineWallpaperVariant variant(kAssetId, GURL(kDummyUrl),
                                 backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo expected_info = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId, {variant}),
      variant);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, expected_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  EXPECT_TRUE(info.MatchesSelection(expected_info));
}

TEST_P(WallpaperControllerTest,
       MigrateWallpaperInfoDoesntHappenWhenSyncedInfoAlreadyExists) {
  OnlineWallpaperVariant local_variant(kAssetId, GURL(kDummyUrl),
                                       backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo local_info = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId, {local_variant}),
      local_variant);
  OnlineWallpaperVariant synced_variant(kAssetId2, GURL(kDummyUrl2),
                                        backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo synced_info = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId, {synced_variant}),
      synced_variant);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &info));
  // Synced info should be the same if local is the same age.
  EXPECT_TRUE(synced_info.MatchesSelection(info));
}

TEST_P(WallpaperControllerTest,
       ActiveUserPrefServiceChangedSyncedInfoHandledLocally) {
  CacheOnlineWallpaper(kDummyUrl);

  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  synced_info.unit_id = kUnitId;
  synced_info.collection_id = TestWallpaperControllerClient::kDummyCollectionId;
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  WallpaperInfo local_info = InfoWithType(WallpaperType::kCustomized);
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

TEST_P(WallpaperControllerTest, ActiveUserPrefServiceChanged_SyncDisabled) {
  CacheOnlineWallpaper(kDummyUrl);
  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, base::Time::Now()};
  synced_info.unit_id = kUnitId;
  synced_info.collection_id = TestWallpaperControllerClient::kDummyCollectionId;
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  WallpaperInfo local_info = InfoWithType(WallpaperType::kDefault);
  local_info.date = DayBeforeYesterdayish();
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  client_.ResetCounts();

  client_.set_wallpaper_sync_enabled(false);

  controller_->OnActiveUserPrefServiceChanged(
      GetProfilePrefService(kAccountId1));
  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kDefault, actual_info.type);
}

TEST_P(WallpaperControllerTest, HandleWallpaperInfoSyncedLocalIsPolicy) {
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

TEST_P(WallpaperControllerTest,
       HandleWallpaperInfoSyncedLocalIsCustomizedAndOlder) {
  CacheOnlineWallpaper(kDummyUrl);

  WallpaperInfo local_info = InfoWithType(WallpaperType::kCustomized);
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

TEST_P(WallpaperControllerTest,
       HandleWallpaperInfoSyncedLocalIsCustomizedAndNewer) {
  CacheOnlineWallpaper(kDummyUrl);
  pref_manager_->SetLocalWallpaperInfo(
      kAccountId1, InfoWithType(WallpaperType::kCustomized));

  WallpaperInfo synced_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kOnline, DayBeforeYesterdayish()};
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  SimulateUserLogin(kAccountId1);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_EQ(WallpaperType::kCustomized, actual_info.type);
}

TEST_P(WallpaperControllerTest, HandleWallpaperInfoSyncedOnline) {
  CacheOnlineWallpaper(kDummyUrl);

  // Attempt to set an online wallpaper without providing the image data. Verify
  // it succeeds this time because |SetOnlineWallpaper| has saved the file.
  ClearWallpaperCount();
  OnlineWallpaperVariant variant(kAssetId, GURL(kDummyUrl),
                                 backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo info = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER, /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId, {variant}),
      variant);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, info);

  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kOnline);
}

TEST_P(WallpaperControllerTest, HandleWallpaperInfoSyncedInactiveUser) {
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

TEST_P(WallpaperControllerTest, UpdateDailyRefreshWallpaper) {
  std::string expected{"fun_collection"};
  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kDaily, DayBeforeYesterdayish()};
  info.unit_id = kUnitId;
  info.collection_id = expected;
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaper();
  EXPECT_EQ(expected, client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_P(WallpaperControllerTest, UpdateDailyRefreshWallpaperCalledOnLogin) {
  SimulateUserLogin(kAccountId1);

  OnlineWallpaperVariant variant(kAssetId, GURL(kDummyUrl),
                                 backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo info = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
          /*from_user=*/false,
          /*daily_refresh_enabled=*/true, kUnitId, {variant}),
      variant);
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

TEST_P(WallpaperControllerTest, UpdateDailyRefreshWallpaper_NotEnabled) {
  SimulateUserLogin(kAccountId1);
  WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kOnline, DayBeforeYesterdayish()};
  info.collection_id = "fun_collection";
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaper();
  EXPECT_EQ(std::string(), client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_P(WallpaperControllerTest, UpdateDailyRefreshWallpaper_NoCollectionId) {
  SimulateUserLogin(kAccountId1);
  pref_manager_->SetUserWallpaperInfo(
      kAccountId1,
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER,
                    WallpaperType::kDaily, DayBeforeYesterdayish()));

  controller_->UpdateDailyRefreshWallpaper();
  EXPECT_EQ(std::string(), client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_P(WallpaperControllerTest, MigrateCustomWallpaper) {
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

TEST_P(WallpaperControllerTest, OnGoogleDriveMounted) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kCustomized);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  SimulateUserLogin(kAccountId1);
  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_EQ(kAccountId1, drivefs_delegate_->get_save_wallpaper_account_id());
}

TEST_P(WallpaperControllerTest, OnGoogleDriveMounted_WallpaperIsntCustom) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_TRUE(drivefs_delegate_->get_save_wallpaper_account_id().empty());
}

TEST_P(WallpaperControllerTest, OnGoogleDriveMounted_AlreadySynced) {
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

TEST_P(WallpaperControllerTest, OnGoogleDriveMounted_OldLocalInfo) {
  WallpaperInfo local_info = WallpaperInfo(
      "a_url", WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kCustomized,
      DayBeforeYesterdayish(), "/test/a_url");
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  WallpaperInfo synced_info = WallpaperInfo(
      "b_url", WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kCustomized,
      base::Time::Now().LocalMidnight(), "/test/b_url");
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  SimulateUserLogin(kAccountId1);

  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_FALSE(drivefs_delegate_->get_save_wallpaper_account_id().is_valid());
}

TEST_P(WallpaperControllerTest, OnGoogleDriveMounted_NewLocalInfo) {
  WallpaperInfo local_info = WallpaperInfo(
      "a_url", WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kCustomized,
      base::Time::Now().LocalMidnight(), "/test/a_url");
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  WallpaperInfo synced_info = WallpaperInfo(
      "b_url", WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kCustomized,
      DayBeforeYesterdayish(), "/test/b_url");
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  SimulateUserLogin(kAccountId1);

  controller_->SyncLocalAndRemotePrefs(kAccountId1);
  EXPECT_EQ(kAccountId1, drivefs_delegate_->get_save_wallpaper_account_id());
}

TEST_P(WallpaperControllerTest, SetDailyRefreshCollectionId_Empty) {
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
TEST_P(WallpaperControllerTest,
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

TEST_P(WallpaperControllerTest, UpdateWallpaperOnScheduleCheckpointChanged) {
  for (const bool is_guest : {false, true}) {
    if (is_guest) {
      SimulateGuestLogin();
    } else {
      SimulateUserLogin(kAccountId1);
    }

    const AccountId active_account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();
    // Enable dark mode by default.
    Shell::Get()->dark_light_mode_controller()->SetAutoScheduleEnabled(false);
    Shell::Get()->dark_light_mode_controller()->SetDarkModeEnabledForTest(true);

    auto run_loop = std::make_unique<base::RunLoop>();
    ClearWallpaperCount();
    std::vector<OnlineWallpaperVariant> variants;
    variants.emplace_back(kAssetId, GURL(kDummyUrl),
                          backdrop::Image::IMAGE_TYPE_DARK_MODE);
    variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                          backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
    const OnlineWallpaperParams& params = OnlineWallpaperParams(
        active_account_id, TestWallpaperControllerClient::kDummyCollectionId,
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
    WallpaperInfo actual;
    ASSERT_TRUE(
        pref_manager_->GetUserWallpaperInfo(active_account_id, &actual));
    EXPECT_EQ(actual.location, kDummyUrl);
    base::Time original_timestamp = actual.date;

    task_environment()->FastForwardBy(base::Hours(1));

    // Switch to light mode and simulate schedule checkpoint change to reflect
    // light mode.
    EXPECT_TRUE(
        Shell::Get()->dark_light_mode_controller()->IsDarkModeEnabled());
    Shell::Get()->dark_light_mode_controller()->ToggleColorMode();
    RunAllTasksUntilIdle();
    EXPECT_EQ(2, GetWallpaperCount());
    ASSERT_TRUE(
        pref_manager_->GetUserWallpaperInfo(active_account_id, &actual));
    EXPECT_EQ(actual.location, kDummyUrl2);
    // The wallpaper in pref should still match what was originally selected.
    // However, the |date| should not be affected by the dark -> light change.
    EXPECT_TRUE(
        actual.MatchesSelection(WallpaperInfo(params, variants.back())));
    EXPECT_EQ(actual.date, original_timestamp);
  }
}

TEST_P(WallpaperControllerAutoScheduleTest, UpdateWallpaperOnAutoColorMode) {
  base::expected<base::Time, GeolocationController::SunRiseSetError>
      sunrise_time = Shell::Get()->geolocation_controller()->GetSunriseTime();
  base::expected<base::Time, GeolocationController::SunRiseSetError>
      sunset_time = Shell::Get()->geolocation_controller()->GetSunsetTime();
  ASSERT_TRUE(sunrise_time.has_value());
  ASSERT_TRUE(sunset_time.has_value());

  SetSimulatedStartTime(sunrise_time.value());
  SimulateUserLogin(kAccountId1);

  ClearWallpaperCount();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/true, kUnitId, variants);
  base::test::TestFuture<bool> future;
  controller_->SetOnlineWallpaper(params, future.GetCallback());
  ASSERT_TRUE(future.Get());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDaily);
  WallpaperInfo actual;
  ASSERT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual));
  EXPECT_EQ(actual.location, kDummyUrl2);
  base::Time original_timestamp = actual.date;

  // Forward time to trigger checkpoints.
  ASSERT_GT(sunset_time.value(), Now());
  task_environment()->FastForwardBy(sunset_time.value() - Now());
  RunAllTasksUntilIdle();

  WallpaperInfo expected = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
          /*from_user=*/true,
          /*daily_refresh_enabled=*/true, kUnitId, variants),
      variants.back());

  ASSERT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual));
  EXPECT_EQ(actual.location, kDummyUrl);
  // The wallpaper in pref should still match what was originally selected.
  // However, the |date| should not be affected by the dark -> light change.
  EXPECT_TRUE(actual.MatchesSelection(expected));
  EXPECT_EQ(actual.date, original_timestamp);
}

TEST_P(WallpaperControllerAutoScheduleTest,
       UpdateTimeOfDayWallpaperWithAutoColorModeOff) {
  static constexpr gfx::Size kTestImageSize = gfx::Size(100, 100);
  static constexpr SkColor kSunriseImageColor = SK_ColorRED;
  static constexpr SkColor kMorningImageColor = SK_ColorGREEN;
  static constexpr SkColor kLateAfternoonImageColor = SK_ColorBLUE;
  static constexpr SkColor kSunsetImageColor = SK_ColorYELLOW;

  if (!IsTimeOfDayEnabled()) {
    return;
  }

  const auto backdrop_image_data = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        backdrop_image_data);
  const base::flat_map<backdrop::Image_ImageType, gfx::ImageSkia> test_images =
      {{backdrop::Image::IMAGE_TYPE_LIGHT_MODE,
        CreateSolidColorTestImage(kTestImageSize, kSunriseImageColor)},
       {backdrop::Image::IMAGE_TYPE_MORNING_MODE,
        CreateSolidColorTestImage(kTestImageSize, kMorningImageColor)},
       {backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE,
        CreateSolidColorTestImage(kTestImageSize, kLateAfternoonImageColor)},
       {backdrop::Image::IMAGE_TYPE_DARK_MODE,
        CreateSolidColorTestImage(kTestImageSize, kSunsetImageColor)}};
  test_wallpaper_image_downloader()->set_image_generator(
      base::BindLambdaForTesting([backdrop_image_data,
                                  test_images](const GURL& url) {
        const backdrop::Image* match_found =
            GetImageMatchingUrl(url, backdrop_image_data);
        return match_found
                   ? test_images.at(match_found->image_type())
                   : CreateSolidColorTestImage(kTestImageSize, SK_ColorBLACK);
      }));

  SimulateUserLogin(kAccountId1);
  Shell::Get()->dark_light_mode_controller()->SetAutoScheduleEnabled(false);

  OnlineWallpaperParams params(
      kAccountId1, wallpaper_constants::kTimeOfDayWallpaperCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false,
      wallpaper_constants::kDefaultTimeOfDayWallpaperUnitId, /*variants=*/{});
  for (const backdrop::Image& backdrop_image : backdrop_image_data) {
    params.variants.emplace_back(backdrop_image.asset_id(),
                                 GURL(backdrop_image.image_url()),
                                 backdrop_image.image_type());
  }

  const auto wallpaper_has_color = [this](SkColor color) {
    return gfx::test::AreImagesClose(
        gfx::Image(controller_->GetWallpaper()),
        gfx::Image(CreateSolidColorTestImage(controller_->GetWallpaper().size(),
                                             color)),
        /*max_deviation=*/1);
  };
  // Midnight
  base::test::TestFuture<bool> future;
  controller_->SetOnlineWallpaper(params, future.GetCallback());
  ASSERT_TRUE(future.Get());
  EXPECT_TRUE(wallpaper_has_color(kSunsetImageColor));

  WallpaperChangedBarrier barrier(controller_, task_environment());
  // Sunrise. 7 AM.
  ASSERT_TRUE(barrier.RunUntilNextWallpaperChange());
  EXPECT_THAT(Now() - simulated_start_time_, WallpaperChangeTimeNear(7));
  EXPECT_TRUE(wallpaper_has_color(kSunriseImageColor));
  // Morning. 11 AM.
  ASSERT_TRUE(barrier.RunUntilNextWallpaperChange());
  EXPECT_THAT(Now() - simulated_start_time_, WallpaperChangeTimeNear(11));
  EXPECT_TRUE(wallpaper_has_color(kMorningImageColor));
  // Sunrise. 5 PM.
  ASSERT_TRUE(barrier.RunUntilNextWallpaperChange());
  EXPECT_THAT(Now() - simulated_start_time_, WallpaperChangeTimeNear(17));
  EXPECT_TRUE(wallpaper_has_color(kLateAfternoonImageColor));
  // Sunrise. 7 PM.
  ASSERT_TRUE(barrier.RunUntilNextWallpaperChange());
  EXPECT_THAT(Now() - simulated_start_time_, WallpaperChangeTimeNear(19));
  EXPECT_TRUE(wallpaper_has_color(kSunsetImageColor));
}

TEST_P(WallpaperControllerTest,
       UpdateWallpaperOnScheduleCheckpointChanged_WithReplacedAsset) {
  SimulateUserLogin(kAccountId1);

  // Enable dark mode by default.
  Shell::Get()->dark_light_mode_controller()->SetAutoScheduleEnabled(false);
  Shell::Get()->dark_light_mode_controller()->SetDarkModeEnabledForTest(true);

  auto run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
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

  // Simulate a wallpaper changes from the server by changing one of the
  // variant's url.
  WallpaperInfo local_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &local_info));
  std::vector<OnlineWallpaperVariant> updated_variants;
  const std::string updated_light_url = "https://light/new_light_url.jpg";
  updated_variants.emplace_back(kAssetId, GURL(kDummyUrl),
                                backdrop::Image::IMAGE_TYPE_DARK_MODE);
  updated_variants.emplace_back(kAssetId2, GURL(updated_light_url),
                                backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  local_info.variants = updated_variants;
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(kAccountId1, local_info));

  // Switch to light mode and simulate schedule checkpoint change to reflect
  // light mode.
  EXPECT_TRUE(Shell::Get()->dark_light_mode_controller()->IsDarkModeEnabled());
  Shell::Get()->dark_light_mode_controller()->ToggleColorMode();
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  WallpaperInfo expected = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
          /*from_user=*/true,
          /*daily_refresh_enabled=*/false, kUnitId, updated_variants),
      updated_variants.back());
  WallpaperInfo actual;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual));
  EXPECT_TRUE(actual.MatchesAsset(expected));
  // Verifies the new asset is downloaded and saved to disk.
  EXPECT_TRUE(base::PathExists(online_wallpaper_dir_.GetPath().Append(
      GURL(updated_light_url).ExtractFileName())));
}

TEST_P(WallpaperControllerTest,
       DoesNotCrashOnScheduleCheckpointChangedWhenDownloadFails) {
  SimulateUserLogin(kAccountId1);

  // Enable dark mode by default.
  Shell::Get()->dark_light_mode_controller()->SetAutoScheduleEnabled(false);
  Shell::Get()->dark_light_mode_controller()->SetDarkModeEnabledForTest(true);

  auto run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
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

  ClearWallpaperCount();
  // Simulate a wallpaper changes from the server by changing one of the
  // variant's url.
  WallpaperInfo local_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &local_info));
  std::vector<OnlineWallpaperVariant> updated_variants;
  const std::string updated_light_url = "https://light/new_light_url.jpg";
  updated_variants.emplace_back(kAssetId, GURL(kDummyUrl),
                                backdrop::Image::IMAGE_TYPE_DARK_MODE);
  updated_variants.emplace_back(kAssetId2, GURL(updated_light_url),
                                backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  local_info.variants = updated_variants;
  EXPECT_TRUE(pref_manager_->SetUserWallpaperInfo(kAccountId1, local_info));
  // Simulate a failure in image downloading.
  test_wallpaper_image_downloader()->set_image_generator(
      base::BindLambdaForTesting([](const GURL&) { return gfx::ImageSkia(); }));

  // Switch to light mode and simulate schedule checkpoint change to reflect
  // light mode.
  EXPECT_TRUE(Shell::Get()->dark_light_mode_controller()->IsDarkModeEnabled());
  Shell::Get()->dark_light_mode_controller()->ToggleColorMode();
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  WallpaperInfo expected = WallpaperInfo(params, updated_variants.front());
  WallpaperInfo actual;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual));
  EXPECT_EQ(actual.asset_id, expected.asset_id);
  EXPECT_EQ(actual.location, expected.location);
  // Assets aren't matched due to updated variant asset.
  EXPECT_FALSE(actual.MatchesAsset(expected));
}

TEST_P(WallpaperControllerTest,
       DoesNotUpdateWallpaperOnColorModeChangedWithNoVariants) {
  SimulateUserLogin(kAccountId1);

  auto run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId,
      {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}});
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

TEST_P(WallpaperControllerTest,
       UpdateWallpaperInfoWithOnlineWallpaperVariants) {
  SimulateUserLogin(kAccountId1);

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId, variants);

  pref_manager_->SetUserWallpaperInfo(kAccountId1,
                                      WallpaperInfo(params, variants.front()));
  WallpaperInfo expected = WallpaperInfo(params, variants.front());
  WallpaperInfo actual;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual);
  EXPECT_TRUE(actual.MatchesSelection(expected));
}

TEST_P(WallpaperControllerTest, SetOnlineWallpaperWithoutInternet) {
  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);
  SimulateUserLogin(kAccountId1);

  // Set an online wallpaper with image data. Verify that the wallpaper is set
  // successfully.
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
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
  test_wallpaper_image_downloader()->set_image_generator(
      base::BindLambdaForTesting([](const GURL&) { return gfx::ImageSkia(); }));
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

TEST_P(WallpaperControllerTest,
       HandleWallpaperInfoSyncedForDarkLightWallpapers_NotSynced) {
  SimulateUserLogin(kAccountId1);
  CacheOnlineWallpaper(kDummyUrl);
  ClearWallpaperCount();

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId, variants);
  // Force local info to not have a unit_id.
  WallpaperInfo local_info = WallpaperInfo(params, variants.front());
  local_info.unit_id = std::nullopt;
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  // synced info tracks dark variant.
  const WallpaperInfo& synced_info = WallpaperInfo(params, variants.back());
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(synced_info));
  // Verify the wallpaper is set.
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_P(WallpaperControllerTest,
       HandleWallpaperInfoSyncedForDarkLightWallpapers_AlreadySynced) {
  SimulateUserLogin(kAccountId1);
  CacheOnlineWallpaper(kDummyUrl);
  ClearWallpaperCount();

  std::vector<OnlineWallpaperVariant> variants;
  variants.emplace_back(kAssetId, GURL(kDummyUrl),
                        backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  variants.emplace_back(kAssetId2, GURL(kDummyUrl2),
                        backdrop::Image::IMAGE_TYPE_DARK_MODE);
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId, variants);
  // local info tracks light variant.
  const WallpaperInfo& local_info = WallpaperInfo(params, variants.front());
  pref_manager_->SetLocalWallpaperInfo(kAccountId1, local_info);

  // synced info tracks dark variant.
  const WallpaperInfo& synced_info = WallpaperInfo(params, variants.back());
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(local_info.MatchesSelection(synced_info));
  EXPECT_TRUE(local_info.MatchesSelection(actual_info));
  // Verify the wallpaper is not set again.
  EXPECT_EQ(0, GetWallpaperCount());
}

TEST_P(WallpaperControllerTest, WallpaperCustomization_Used) {
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

  controller_->SetCustomizedDefaultWallpaperPaths(paths.first, paths.second);
  task_environment()->RunUntilIdle();

  // Verify that the customized wallpaper is in use.
  EXPECT_THAT(GetCurrentWallpaperInfo().location,
              testing::EndsWith(kCustomizationSmallWallpaperName));
}

TEST_P(WallpaperControllerTest, WallpaperCustomization_UnusedForNonDefault) {
  SimulateUserLogin(kAccountId1);

  // Set wallpaper to something a user may have chose.
  controller_->SetOnlineWallpaper(
      OnlineWallpaperParams(
          kAccountId1,
          /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
          /*preview_mode=*/false, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, kUnitId,
          /*variants=*/
          {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}}),
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

TEST_P(WallpaperControllerTest, TimeOfDayWallpapers_NotSyncedIn) {
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
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId, variants);
  WallpaperInfo local_info = WallpaperInfo(params, variants.front());
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

TEST_P(WallpaperControllerTest, TimeOfDayWallpapers_NotSyncedOut) {
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
  const OnlineWallpaperParams& params = OnlineWallpaperParams(
      kAccountId1, wallpaper_constants::kTimeOfDayWallpaperCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId, variants);
  WallpaperInfo local_info = WallpaperInfo(params, variants.front());
  local_info.unit_id = kUnitId;
  pref_manager_->SetUserWallpaperInfo(kAccountId1, local_info);
  RunAllTasksUntilIdle();

  WallpaperInfo actual_info;
  EXPECT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &actual_info));
  EXPECT_TRUE(actual_info.MatchesSelection(synced_info));
}

TEST_P(WallpaperControllerTest, SetGooglePhotosWallpaper) {
  SimulateUserLogin(kAccountId1);

  // First set the wallpaper to an Online one so we can tell for sure if setting
  // a Google Photos wallpaper has failed.
  base::test::TestFuture<bool> online_future;
  controller_->SetOnlineWallpaper(
      {kAccountId1,
       TestWallpaperControllerClient::kDummyCollectionId,
       WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false,
       /*from_user=*/true,
       /*daily_refresh_enabled=*/false,
       kUnitId,
       {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}}},
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

TEST_P(WallpaperControllerTest, SetGooglePhotosWallpaperFails) {
  SimulateUserLogin(kAccountId1);

  // First set the wallpaper to an Online one so we can tell for sure if setting
  // a Google Photos wallpaper has failed.
  base::test::TestFuture<bool> online_future;
  OnlineWallpaperParams online_params(
      {kAccountId1,
       TestWallpaperControllerClient::kDummyCollectionId,
       WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false,
       /*from_user=*/true,
       /*daily_refresh_enabled=*/false,
       kUnitId,
       {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}}});
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
  WallpaperInfo expected_wallpaper_info(online_params,
                                        online_params.variants.front());
  EXPECT_TRUE(wallpaper_info.MatchesSelection(expected_wallpaper_info));
}

TEST_P(WallpaperControllerTest, ResetToDefaultForDeletedPhotoOnStalenessCheck) {
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

TEST_P(WallpaperControllerTest, HandleSyncDeletedGooglePhotosPhoto) {
  WallpaperInfo local_info = InfoWithType(WallpaperType::kOnline);
  local_info.date -= base::Days(2);
  pref_manager_->SetUserWallpaperInfo(kAccountId1, local_info);

  WallpaperInfo synced_info = InfoWithType(WallpaperType::kOnceGooglePhotos);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  // Just started and still loading wallpaper.
  ASSERT_FALSE(controller_->HasShownAnyWallpaper());
  ASSERT_THAT(client_.fetch_google_photos_photo_id(), testing::IsEmpty());
  client_.set_google_photo_has_been_deleted(true);

  SimulateUserLogin(kAccountId1);
  EXPECT_EQ(synced_info.location, client_.fetch_google_photos_photo_id());
  RunAllTasksUntilIdle();

  WallpaperInfo final_local_info;
  ASSERT_TRUE(
      pref_manager_->GetLocalWallpaperInfo(kAccountId1, &final_local_info));

  EXPECT_TRUE(final_local_info.MatchesAsset(local_info));
  histogram_tester().ExpectUniqueSample(
      "Ash.Wallpaper.OnceGooglePhotos.Result2",
      SetWallpaperResult::kFileNotFound, 1);
}

TEST_P(WallpaperControllerTest, GooglePhotosAreCachedOnDisk) {
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

TEST_P(WallpaperControllerTest, GooglePhotosAreCachedInMemory) {
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

TEST_P(WallpaperControllerTest, GooglePhotosAreReadFromCache) {
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

TEST_P(WallpaperControllerTest, ConfirmGooglePhotosPreviewWallpaper) {
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

TEST_P(WallpaperControllerTest, CancelGooglePhotosPreviewWallpaper) {
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

TEST_P(WallpaperControllerTest, GooglePhotosWallpaperSyncedDuringPreview) {
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

TEST_P(WallpaperControllerTest, UpdateGooglePhotosDailyRefreshWallpaper) {
  // The `TestWallpaperControllerClient` sends back the reversed
  // `collection_id` when asked to fetch a daily photo.
  std::string expected_photo_id = kFakeGooglePhotosAlbumId;
  std::reverse(expected_photo_id.begin(), expected_photo_id.end());

  SimulateUserLogin(kAccountId1);

  GooglePhotosWallpaperParams params(
      kAccountId1, kFakeGooglePhotosAlbumId,
      /*daily_refresh_enabled=*/true, WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*dedup_key=*/std::nullopt);
  WallpaperInfo info(params);
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  controller_->UpdateDailyRefreshWallpaper();
  RunAllTasksUntilIdle();

  WallpaperInfo expected_info;
  EXPECT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &expected_info));
  EXPECT_EQ(expected_photo_id, expected_info.location);
  EXPECT_EQ(kFakeGooglePhotosAlbumId, expected_info.collection_id);
}

TEST_P(WallpaperControllerTest, EmptyDailyGooglePhotosAlbumsDoNothing) {
  SimulateUserLogin(kAccountId1);

  GooglePhotosWallpaperParams daily_google_photos_params(
      kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
      WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
      /*dedup_key=*/std::nullopt);
  OnlineWallpaperParams online_params(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId,
      /*variants=*/
      {{kAssetId, GURL(kDummyUrl), backdrop::Image::IMAGE_TYPE_UNKNOWN}});

  WallpaperInfo online_info(online_params, online_params.variants.front());
  pref_manager_->SetUserWallpaperInfo(kAccountId1, online_info);

  client_.set_fetch_google_photos_photo_fails(true);
  controller_->SetGooglePhotosWallpaper(daily_google_photos_params,
                                        base::DoNothing());
  RunAllTasksUntilIdle();

  WallpaperInfo current_info;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_TRUE(online_info.MatchesSelection(current_info));
}

TEST_P(WallpaperControllerTest,
       ResetToDefaultForDeletedDailyGooglePhotosAlbums) {
  SimulateUserLogin(kAccountId1);

  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
       WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/std::nullopt},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  WallpaperInfo current_info;
  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_EQ(WallpaperType::kDailyGooglePhotos, current_info.type);

  // This makes the test fetch in `client_` return a null photo, but a
  // successful call, which is the sign for a deleted or empty album.
  client_.set_google_photo_has_been_deleted(true);

  controller_->UpdateDailyRefreshWallpaper();
  RunAllTasksUntilIdle();

  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_EQ(WallpaperType::kDefault, current_info.type);
}

TEST_P(WallpaperControllerTest, DailyGooglePhotosAreCached) {
  SimulateUserLogin(kAccountId1);
  // The `TestWallpaperControllerClient` sends back the reversed
  // `collection_id` when asked to fetch a daily photo.
  std::string expected_photo_id = kFakeGooglePhotosAlbumId;
  std::reverse(expected_photo_id.begin(), expected_photo_id.end());

  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
       WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/std::nullopt},
      google_photos_future.GetCallback());
  EXPECT_TRUE(google_photos_future.Get());
  RunAllTasksUntilIdle();

  base::FilePath saved_wallpaper = online_wallpaper_dir_.GetPath()
                                       .Append("google_photos/")
                                       .Append(kAccountId1.GetAccountIdKey())
                                       .Append(expected_photo_id);
  ASSERT_TRUE(base::PathExists(saved_wallpaper));
}

TEST_P(WallpaperControllerTest,
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

TEST_P(
    WallpaperControllerTest,
    ResetToDefaultForDisabledGooglePhotosIntegrationPolicyDailyGooglePhotosAlbums) {
  SimulateUserLogin(kAccountId1);

  base::test::TestFuture<bool> google_photos_future;
  controller_->SetGooglePhotosWallpaper(
      {kAccountId1, kFakeGooglePhotosAlbumId, /*daily_refresh_enabled=*/true,
       WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/std::nullopt},
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

  controller_->UpdateDailyRefreshWallpaper();
  RunAllTasksUntilIdle();

  pref_manager_->GetUserWallpaperInfo(kAccountId1, &current_info);

  EXPECT_EQ(WallpaperType::kDefault, current_info.type);
}

class WallpaperControllerDailyRefreshSchedulerTest
    : public WallpaperControllerTest,
      public ScheduledFeature::Clock {
 public:
  WallpaperControllerDailyRefreshSchedulerTest() {
    base::Time start_time = base::Time::Now();
    clock_.SetNow(start_time);
    tick_clock_.SetNowTicks(base::TimeTicks() + (start_time - base::Time()));
  }

  void SetUp() override {
    WallpaperControllerTest::SetUp();

    auto daily_refresh_scheduler =
        controller_->daily_refresh_scheduler_for_testing();
    // Disable any running timers to set a fake clock.
    daily_refresh_scheduler->SetScheduleType(ScheduleType::kNone);
    daily_refresh_scheduler->SetClockForTesting(this);
    daily_refresh_scheduler->SetScheduleType(ScheduleType::kCustom);
  }

  void TearDown() override { WallpaperControllerTest::TearDown(); }

  // ScheduledFeature::Clock:
  base::Time Now() const override { return clock_.Now(); }

  base::TimeTicks NowTicks() const override { return tick_clock_.NowTicks(); }

  // Returns whether the total triggered a checkpoint change. This method only
  // triggers the checkpoints and does not run any tasks.
  bool AdvanceClock(base::TimeDelta total) {
    const auto advance_time = [this](base::TimeDelta advancement) {
      clock_.Advance(advancement);
      tick_clock_.Advance(advancement);
    };

    bool checkpoint_reached = false;
    auto* timer = Shell::Get()
                      ->wallpaper_controller()
                      ->daily_refresh_scheduler_for_testing()
                      ->timer();
    while (total.is_positive()) {
      base::TimeDelta advance_increment;
      if (timer->IsRunning() &&
          timer->desired_run_time() <= NowTicks() + total) {
        // Emulates the internal timer firing at its scheduled time.
        advance_increment = timer->desired_run_time() - NowTicks();
        advance_time(advance_increment);
        timer->FireNow();
        checkpoint_reached = true;
      } else {
        advance_increment = total;
        advance_time(advance_increment);
      }
      CHECK_LE(advance_increment, total);
      total -= advance_increment;
    }
    return checkpoint_reached;
  }

 private:
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    WallpaperControllerDailyRefreshSchedulerTest,
    ::testing::Values(TimeOfDayFeatureCombination::kDisabled,
                      TimeOfDayFeatureCombination::kTimeOfDay),
    WallpaperControllerTest::PrintToStringParamName());

TEST_P(WallpaperControllerDailyRefreshSchedulerTest,
       OnCheckpointChanged_WallpaperDailyRefreshScheduler) {
  TestWallpaperControllerObserver observer(controller_);
  EXPECT_EQ(0, observer.daily_refresh_checkpoint_count());
  // User's wallpaper info should exist.
  pref_manager_->SetUserWallpaperInfo(kAccountId1,
                                      InfoWithType(WallpaperType::kDefault));
  SimulateUserLogin(kAccountId1);
  // Clears signal on login.
  observer.ClearDailyRefreshCheckpointCount();
  EXPECT_TRUE(AdvanceClock(base::Days(1)));
  // Expect that 2 signals are sent every day by WallpaperDailyRefreshScheduler.
  EXPECT_EQ(2, observer.daily_refresh_checkpoint_count());
}

TEST_P(WallpaperControllerDailyRefreshSchedulerTest,
       OnCheckpointChanged_CalledOnLogin) {
  TestWallpaperControllerObserver observer(controller_);
  EXPECT_EQ(0, observer.daily_refresh_checkpoint_count());
  // User's wallpaper info should exist.
  pref_manager_->SetUserWallpaperInfo(kAccountId1,
                                      InfoWithType(WallpaperType::kDaily));
  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();
  // Expect that at least one signal is sent on login. Other checkpoints may be
  // due to the randomized start and end check times of the daily refresh
  // scheduler.
  EXPECT_GE(observer.daily_refresh_checkpoint_count(), 1);
}

TEST_P(WallpaperControllerDailyRefreshSchedulerTest,
       SetDailyRefreshCollectionId_UpdatesCheckTimes) {
  auto daily_refresh_scheduler =
      controller_->daily_refresh_scheduler_for_testing();
  auto first_check_time = daily_refresh_scheduler->GetCustomStartTime();
  auto second_check_time = daily_refresh_scheduler->GetCustomEndTime();
  // User's wallpaper info should exist.
  pref_manager_->SetUserWallpaperInfo(kAccountId1,
                                      InfoWithType(WallpaperType::kOnline));
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->SetDailyRefreshCollectionId(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId);
  controller_->UpdateDailyRefreshWallpaper();
  WaitForWallpaperCount(1);
  EXPECT_EQ(TestWallpaperControllerClient::kDummyCollectionId,
            client_.get_fetch_daily_refresh_wallpaper_param());

  WallpaperInfo expected;
  ASSERT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &expected));
  EXPECT_EQ(WallpaperType::kDaily, expected.type);

  // Expect that scheduler check times are updated.
  EXPECT_NE(first_check_time, daily_refresh_scheduler->GetCustomStartTime());
  EXPECT_NE(second_check_time, daily_refresh_scheduler->GetCustomEndTime());
}

TEST_P(WallpaperControllerDailyRefreshSchedulerTest,
       SetGooglePhotosDailyRefreshAlbumId_UpdatesCheckTimes) {
  auto daily_refresh_scheduler =
      controller_->daily_refresh_scheduler_for_testing();
  auto first_check_time = daily_refresh_scheduler->GetCustomStartTime();
  auto second_check_time = daily_refresh_scheduler->GetCustomEndTime();
  // User's wallpaper info should exist.
  pref_manager_->SetUserWallpaperInfo(kAccountId1,
                                      InfoWithType(WallpaperType::kOnline));
  SimulateUserLogin(kAccountId1);
  ClearWallpaperCount();
  controller_->SetGooglePhotosDailyRefreshAlbumId(
      kAccountId1, TestWallpaperControllerClient::kDummyCollectionId);
  controller_->UpdateDailyRefreshWallpaper();
  WaitForWallpaperCount(1);

  WallpaperInfo expected;
  ASSERT_TRUE(pref_manager_->GetUserWallpaperInfo(kAccountId1, &expected));
  EXPECT_EQ(WallpaperType::kDailyGooglePhotos, expected.type);

  // Expect that scheduler check times are updated.
  EXPECT_NE(first_check_time, daily_refresh_scheduler->GetCustomStartTime());
  EXPECT_NE(second_check_time, daily_refresh_scheduler->GetCustomEndTime());
}

TEST_P(WallpaperControllerDailyRefreshSchedulerTest,
       UpdateDailyRefreshWallpaper_OnLogin) {
  SimulateUserLogin(kAccountId1);

  OnlineWallpaperVariant variant(kAssetId, GURL(kDummyUrl),
                                 backdrop::Image::IMAGE_TYPE_UNKNOWN);
  WallpaperInfo info = WallpaperInfo(
      OnlineWallpaperParams(
          kAccountId1, TestWallpaperControllerClient::kDummyCollectionId,
          WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false,
          /*from_user=*/false,
          /*daily_refresh_enabled=*/true, kUnitId,
          /*variants=*/{variant}),
      variant);
  info.date = DayBeforeYesterdayish();
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);

  ClearLogin();
  SimulateUserLogin(kAccountId1);

  // Info is set as over a day old so we expect one task to run in under an hour
  // (due to fuzzing) then it will idle.
  AdvanceClock(base::Hours(1));
  // Make sure all the tasks such as syncing, setting wallpaper complete.
  RunAllTasksUntilIdle();

  EXPECT_EQ(TestWallpaperControllerClient::kDummyCollectionId,
            client_.get_fetch_daily_refresh_wallpaper_param());
}

TEST_P(WallpaperControllerDailyRefreshSchedulerTest,
       UpdateDailyRefreshWallpaper_OnCheckpointChanged) {
  auto images = ImageSet();
  std::string collection_id{"my_wallpaper_collection"};
  client_.AddCollection(collection_id, images);

  // User's wallpaper info should exist.
  pref_manager_->SetUserWallpaperInfo(kAccountId1,
                                      InfoWithType(WallpaperType::kDaily));
  SimulateUserLogin(kAccountId1);

  base::RunLoop run_loop;
  ClearWallpaperCount();
  controller_->SetDailyRefreshCollectionId(kAccountId1, collection_id);
  controller_->UpdateDailyRefreshWallpaper(
      base::BindLambdaForTesting([quit = run_loop.QuitClosure()](bool success) {
        EXPECT_TRUE(success);
        std::move(quit).Run();
      }));
  run_loop.Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDaily);
  WallpaperInfo wallpaper_info_1;
  ASSERT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info_1));
  EXPECT_EQ(collection_id, wallpaper_info_1.collection_id);
  EXPECT_EQ(WallpaperType::kDaily, wallpaper_info_1.type);

  // Forward time to trigger checkpoints.
  EXPECT_TRUE(AdvanceClock(base::Hours(25)));
  RunAllTasksUntilIdle();

  WallpaperInfo wallpaper_info_2;
  ASSERT_TRUE(
      pref_manager_->GetUserWallpaperInfo(kAccountId1, &wallpaper_info_2));
  // Expect a new daily wallpaper is set.
  EXPECT_FALSE(wallpaper_info_1.MatchesSelection(wallpaper_info_2));
  EXPECT_EQ(collection_id, wallpaper_info_2.collection_id);
  EXPECT_EQ(WallpaperType::kDaily, wallpaper_info_2.type);
}

TEST_P(WallpaperControllerDailyRefreshSchedulerTest,
       CheckGooglePhotosStaleness_OnCheckpointChanged) {
  SimulateUserLogin(kAccountId1);

  WallpaperInfo info = {kFakeGooglePhotosPhotoId, WALLPAPER_LAYOUT_CENTER,
                        WallpaperType::kOnceGooglePhotos,
                        DayBeforeYesterdayish()};
  pref_manager_->SetUserWallpaperInfo(kAccountId1, info);
  client_.set_google_photo_has_been_deleted(true);

  // Forward time to trigger checkpoints.
  EXPECT_TRUE(AdvanceClock(base::Hours(25)));
  RunAllTasksUntilIdle();

  EXPECT_EQ(controller_->GetWallpaperType(), WallpaperType::kDefault);
}

class WallpaperControllerVersionedWallpaperInfoTest
    : public WallpaperControllerTestBase {
 public:
  WallpaperControllerVersionedWallpaperInfoTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kVersionedWallpaperInfo,
         features::kFeatureManagementTimeOfDayWallpaper},
        {});
  }
  ~WallpaperControllerVersionedWallpaperInfoTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       RecordNotSupportedNoLocationMigrationStatus) {
  WallpaperInfo unmigrated_info = {"", WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kOnline, base::Time::Now()};
  unmigrated_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(local_state(),
                                        prefs::kUserWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  histogram_tester().ExpectBucketCount("Ash.Wallpaper.Online.MigrationStatus",
                                       MigrationStatus::kNotSupportedNoLocation,
                                       1);
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Online.MigrationLatency",
                                      1);
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       RecordNotSupportedNoCollectionMigrationStatus) {
  WallpaperInfo unmigrated_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kOnline, base::Time::Now()};
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(local_state(),
                                        prefs::kUserWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  histogram_tester().ExpectBucketCount(
      "Ash.Wallpaper.Online.MigrationStatus",
      MigrationStatus::kNotSupportedNoCollection, 1);
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Online.MigrationLatency",
                                      1);
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       OnlineWallpaperMigratedSuccessfullyOnLogin) {
  WallpaperInfo unmigrated_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kOnline, base::Time::Now()};
  unmigrated_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(local_state(),
                                        prefs::kUserWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo migrated_info;
  ASSERT_TRUE(
      pref_manager_->GetLocalWallpaperInfo(kAccountId1, &migrated_info));
  EXPECT_TRUE(migrated_info.version.IsValid());
  EXPECT_TRUE(migrated_info.unit_id.has_value());
  EXPECT_FALSE(migrated_info.variants.empty());
  histogram_tester().ExpectBucketCount("Ash.Wallpaper.Online.MigrationStatus",
                                       MigrationStatus::kSucceeded, 1);
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Online.MigrationLatency",
                                      1);
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       GooglePhotosWallpaperMigratedSuccessfullyOnLogin) {
  WallpaperInfo unmigrated_info =
      InfoWithType(WallpaperType::kOnceGooglePhotos);
  unmigrated_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(local_state(),
                                        prefs::kUserWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo migrated_info;
  ASSERT_TRUE(
      pref_manager_->GetLocalWallpaperInfo(kAccountId1, &migrated_info));
  EXPECT_TRUE(migrated_info.version.IsValid());
  EXPECT_EQ(migrated_info.type, WallpaperType::kOnceGooglePhotos);
  histogram_tester().ExpectBucketCount(
      "Ash.Wallpaper.OnceGooglePhotos.MigrationStatus",
      MigrationStatus::kSucceeded, 1);
  histogram_tester().ExpectTotalCount(
      "Ash.Wallpaper.OnceGooglePhotos.MigrationLatency", 1);
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       MigratedLocalWallpaperSyncOutSuccessfully) {
  WallpaperInfo unmigrated_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kOnline, base::Time::Now()};
  unmigrated_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(local_state(),
                                        prefs::kUserWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo synced_info;
  WallpaperInfo local_info;
  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &synced_info));
  ASSERT_TRUE(pref_manager_->GetLocalWallpaperInfo(kAccountId1, &local_info));
  EXPECT_TRUE(synced_info.version.IsValid());
  EXPECT_TRUE(synced_info.unit_id.has_value());
  EXPECT_FALSE(synced_info.variants.empty());
  EXPECT_TRUE(local_info.MatchesAsset(synced_info));
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       LocalWallpaperOverwrittenBySyncedInfoSuccessfully) {
  WallpaperInfo unmigrated_info = {kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kOnline, base::Time::Min()};
  unmigrated_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(local_state(),
                                        prefs::kUserWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  WallpaperInfo synced_info = InfoWithType(WallpaperType::kOnceGooglePhotos);
  pref_manager_->SetSyncedWallpaperInfo(kAccountId1, synced_info);

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo local_info;
  ASSERT_TRUE(pref_manager_->GetLocalWallpaperInfo(kAccountId1, &local_info));
  EXPECT_TRUE(local_info.version.IsValid());
  EXPECT_TRUE(local_info.MatchesAsset(synced_info));
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       UnsuccessfullyMigratedWallpaperDoesNotSyncOut) {
  WallpaperInfo unmigrated_info = {"https://expected_to_fail_url",
                                   WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kOnline, base::Time::Min()};
  unmigrated_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(local_state(),
                                        prefs::kUserWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo synced_info;
  EXPECT_FALSE(
      pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &synced_info));
  histogram_tester().ExpectUniqueSample("Ash.Wallpaper.Online.MigrationStatus",
                                        MigrationStatus::kFailed, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Wallpaper.MigrationFailureReason",
      MigrationFailureReason::kOnlineVariantsFetchFailure, 1);
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       SyncedInOnlineWallpaperMigratedSuccessfully) {
  {
    WallpaperInfo unmigrated_local_info =
        InfoWithType(WallpaperType::kOnceGooglePhotos);
    unmigrated_local_info.version = base::Version();
    ScopedDictPrefUpdate wallpaper_update(local_state(),
                                          prefs::kUserWallpaperInfo);
    wallpaper_update->Set(kAccountId1.GetUserEmail(),
                          unmigrated_local_info.ToDict());
  }
  {
    WallpaperInfo unmigrated_synced_info = {
        kDummyUrl, WALLPAPER_LAYOUT_CENTER_CROPPED, WallpaperType::kOnline,
        base::Time::Now()};
    unmigrated_synced_info.collection_id =
        TestWallpaperControllerClient::kDummyCollectionId;
    unmigrated_synced_info.version = base::Version();
    ScopedDictPrefUpdate wallpaper_update(
        GetProfilePrefService(kAccountId1),
        prefs::kSyncableVersionedWallpaperInfo);
    wallpaper_update->Set(kAccountId1.GetUserEmail(),
                          unmigrated_synced_info.ToDict());
  }

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo migrated_info;
  ASSERT_TRUE(
      pref_manager_->GetLocalWallpaperInfo(kAccountId1, &migrated_info));
  EXPECT_TRUE(migrated_info.version.IsValid());
  EXPECT_TRUE(migrated_info.unit_id.has_value());
  EXPECT_FALSE(migrated_info.variants.empty());
  histogram_tester().ExpectBucketCount("Ash.Wallpaper.Online.MigrationStatus",
                                       MigrationStatus::kSucceeded, 1);
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Online.MigrationLatency",
                                      1);
  histogram_tester().ExpectBucketCount(
      "Ash.Wallpaper.OnceGooglePhotos.MigrationStatus",
      MigrationStatus::kSucceeded, 1);
  histogram_tester().ExpectTotalCount(
      "Ash.Wallpaper.OnceGooglePhotos.MigrationLatency", 1);
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       ShouldNotSyncInForUnsuccessfullyMigratedWallpaper) {
  WallpaperInfo unmigrated_info = {"https://expected_to_fail",
                                   WALLPAPER_LAYOUT_CENTER_CROPPED,
                                   WallpaperType::kOnline, base::Time::Now()};
  unmigrated_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  unmigrated_info.version = base::Version();
  ScopedDictPrefUpdate wallpaper_update(GetProfilePrefService(kAccountId1),
                                        prefs::kSyncableVersionedWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), unmigrated_info.ToDict());

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo migrated_info;
  EXPECT_FALSE(
      pref_manager_->GetLocalWallpaperInfo(kAccountId1, &migrated_info));
  histogram_tester().ExpectBucketCount("Ash.Wallpaper.Online.MigrationStatus",
                                       MigrationStatus::kFailed, 1);
  histogram_tester().ExpectTotalCount("Ash.Wallpaper.Online.MigrationLatency",
                                      1);
}

TEST_F(WallpaperControllerVersionedWallpaperInfoTest,
       ShowPreviouslySyncedWallpaperWhenUserLogsInANewDevice) {
  WallpaperInfo prev_synced_info;
  prev_synced_info.location = kDummyUrl;
  prev_synced_info.layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
  prev_synced_info.type = WallpaperType::kOnline;
  prev_synced_info.collection_id =
      TestWallpaperControllerClient::kDummyCollectionId;
  prev_synced_info.date = base::Time::Now();

  ScopedDictPrefUpdate wallpaper_update(GetProfilePrefService(kAccountId1),
                                        prefs::kSyncableWallpaperInfo);
  wallpaper_update->Set(kAccountId1.GetUserEmail(), prev_synced_info.ToDict());

  ASSERT_TRUE(pref_manager_->GetSyncedWallpaperInfoFromDeprecatedPref(
      kAccountId1, &prev_synced_info));

  SimulateUserLogin(kAccountId1);
  RunAllTasksUntilIdle();

  WallpaperInfo local_info;
  WallpaperInfo synced_info;
  EXPECT_TRUE(pref_manager_->GetLocalWallpaperInfo(kAccountId1, &local_info));
  EXPECT_TRUE(pref_manager_->GetSyncedWallpaperInfo(kAccountId1, &synced_info));
  EXPECT_TRUE(local_info.MatchesAsset(synced_info));
  EXPECT_TRUE(local_info.version.IsValid());
  EXPECT_FALSE(local_info.variants.empty());
  EXPECT_TRUE(local_info.unit_id.has_value());

  // Expects deprecated pref to be cleared.
  EXPECT_FALSE(pref_manager_->GetSyncedWallpaperInfoFromDeprecatedPref(
      kAccountId1, &synced_info));
}

}  // namespace ash
