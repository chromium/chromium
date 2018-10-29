// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller.h"

#include <cmath>
#include <cstdlib>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/bind_test_util.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/views/widget/widget.h"

using session_manager::SessionState;

namespace ash {
namespace {

// Containers IDs used for tests.
constexpr int kWallpaperId = kShellWindowId_WallpaperContainer;
constexpr int kLockScreenWallpaperId =
    kShellWindowId_LockScreenWallpaperContainer;

constexpr char kDefaultSmallWallpaperName[] = "small.jpg";
constexpr char kDefaultLargeWallpaperName[] = "large.jpg";
constexpr char kGuestSmallWallpaperName[] = "guest_small.jpg";
constexpr char kGuestLargeWallpaperName[] = "guest_large.jpg";
constexpr char kChildSmallWallpaperName[] = "child_small.jpg";
constexpr char kChildLargeWallpaperName[] = "child_large.jpg";

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
const AccountId account_id_1 = AccountId::FromUserEmail(kUser1);
const std::string wallpaper_files_id_1 = GetDummyFileId(account_id_1);
const std::string file_name_1 = GetDummyFileName(account_id_1);

constexpr char kUser2[] = "user2@test.com";
const AccountId account_id_2 = AccountId::FromUserEmail(kUser2);
const std::string wallpaper_files_id_2 = GetDummyFileId(account_id_2);
const std::string file_name_2 = GetDummyFileName(account_id_2);

const std::string kDummyUrl = "https://best_wallpaper/1";
const std::string kDummyUrl2 = "https://best_wallpaper/2";

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

// Steps a widget's layer animation until it is completed. Animations must be
// enabled.
void RunAnimationForWidget(views::Widget* widget) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::Layer* layer = widget->GetLayer();
  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    layer->GetAnimator()->Step(step_time +
                               base::TimeDelta::FromMilliseconds(1000));
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

  size_t bytes_written = base::WriteFile(
      path, reinterpret_cast<const char*>(&output[0]), output.size());
  if (bytes_written != output.size()) {
    LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
               << output.size() << " to " << path.value();
    return false;
  }
  return true;
}

// Returns custom wallpaper path. Creates the directory if it doesn't exist.
base::FilePath GetCustomWallpaperPath(const char* sub_dir,
                                      const std::string& wallpaper_files_id,
                                      const std::string& file_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath wallpaper_path = WallpaperController::GetCustomWallpaperPath(
      sub_dir, wallpaper_files_id, file_name);
  if (!base::DirectoryExists(wallpaper_path.DirName()))
    base::CreateDirectory(wallpaper_path.DirName());

  return wallpaper_path;
}

void WaitUntilCustomWallpapersDeleted(const AccountId& account_id) {
  const std::string wallpaper_file_id = GetDummyFileId(account_id);

  base::FilePath small_wallpaper_dir =
      WallpaperController::GetCustomWallpaperDir(
          WallpaperController::kSmallWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath large_wallpaper_dir =
      WallpaperController::GetCustomWallpaperDir(
          WallpaperController::kLargeWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath original_wallpaper_dir =
      WallpaperController::GetCustomWallpaperDir(
          WallpaperController::kOriginalWallpaperSubDir)
          .Append(wallpaper_file_id);

  while (base::PathExists(small_wallpaper_dir) ||
         base::PathExists(large_wallpaper_dir) ||
         base::PathExists(original_wallpaper_dir)) {
  }
}

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}
  ~TaskObserver() override = default;

  // MessageLoop::TaskObserver:
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    processed_ = true;
  }

  // Returns true if any task was processed.
  bool processed() const { return processed_; }

 private:
  bool processed_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

// See content::RunAllTasksUntilIdle().
void RunAllTasksUntilIdle() {
  while (true) {
    TaskObserver task_observer;
    base::MessageLoopCurrent::Get()->AddTaskObserver(&task_observer);
    // May spin message loop.
    base::TaskScheduler::GetInstance()->FlushForTesting();

    base::RunLoop().RunUntilIdle();
    base::MessageLoopCurrent::Get()->RemoveTaskObserver(&task_observer);

    if (!task_observer.processed())
      break;
  }
}

// A test implementation of the WallpaperObserver mojo interface.
class TestWallpaperObserver : public mojom::WallpaperObserver {
 public:
  TestWallpaperObserver() = default;
  ~TestWallpaperObserver() override = default;

  // mojom::WallpaperObserver:
  void OnWallpaperChanged(uint32_t image_id) override {}

  void OnWallpaperColorsChanged(
      const std::vector<SkColor>& prominent_colors) override {
    ++wallpaper_colors_changed_count_;
    if (run_loop_)
      run_loop_->Quit();
  }

  void OnWallpaperBlurChanged(bool blurred) override {}

  int wallpaper_colors_changed_count() const {
    return wallpaper_colors_changed_count_;
  }

  void set_run_loop(base::RunLoop* loop) { run_loop_ = loop; }

 private:
  base::RunLoop* run_loop_ = nullptr;
  int wallpaper_colors_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperObserver);
};

class TestWallpaperControllerObserver : public WallpaperControllerObserver {
 public:
  TestWallpaperControllerObserver() = default;

  void OnWallpaperBlurChanged() override { ++wallpaper_blur_changed_count_; }
  void OnFirstWallpaperShown() override { ++first_wallpaper_shown_count_; }

  void Reset() { wallpaper_blur_changed_count_ = 0; }

  int wallpaper_blur_changed_count_ = 0;
  int first_wallpaper_shown_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWallpaperControllerObserver);
};

}  // namespace

class WallpaperControllerTest : public AshTestBase {
 public:
  WallpaperControllerTest() : controller_(nullptr) {}
  ~WallpaperControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    // Ash shell initialization creates wallpaper. Reset it so we can manually
    // control wallpaper creation and animation in our tests.
    Shell::Get()
        ->GetPrimaryRootWindowController()
        ->wallpaper_widget_controller()
        ->ResetWidgetsForTesting();
    controller_ = Shell::Get()->wallpaper_controller();
    controller_->set_wallpaper_reload_no_delay_for_test();

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(online_wallpaper_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(custom_wallpaper_dir_.CreateUniqueTempDir());
    controller_->InitializePathsForTesting(user_data_dir_.GetPath(),
                                           online_wallpaper_dir_.GetPath(),
                                           custom_wallpaper_dir_.GetPath());
  }

  WallpaperView* wallpaper_view() {
    WallpaperWidgetController* controller =
        Shell::Get()
            ->GetPrimaryRootWindowController()
            ->wallpaper_widget_controller();
    EXPECT_TRUE(controller);
    EXPECT_TRUE(controller->GetAnimatingWidget());
    return static_cast<WallpaperView*>(
        controller->GetAnimatingWidget()->GetContentsView()->child_at(0));
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
  // TODO(bshe): Don't require tests to run animations; it's slow.
  void RunDesktopControllerAnimation() {
    WallpaperWidgetController* controller =
        Shell::Get()
            ->GetPrimaryRootWindowController()
            ->wallpaper_widget_controller();
    ASSERT_TRUE(controller);
    ASSERT_TRUE(controller->GetAnimatingWidget());
    ASSERT_NO_FATAL_FAILURE(
        RunAnimationForWidget(controller->GetAnimatingWidget()));
  }

  // Convenience function to ensure ShouldCalculateColors() returns true.
  void EnableShelfColoring() {
    const gfx::ImageSkia kImage = CreateImage(10, 10, kWallpaperColor);
    controller_->ShowWallpaperImage(
        kImage, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
        false /*preview_mode=*/);
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
    return WallpaperInfo("", layout, DEFAULT,
                         base::Time::Now().LocalMidnight());
  }

  // Helper function to create a new |mojom::WallpaperUserInfoPtr| instance with
  // default values. In addition, clear the wallpaper count and the decoding
  // request list. May be called multiple times for the same |account_id|.
  mojom::WallpaperUserInfoPtr InitializeUser(const AccountId& account_id) {
    mojom::WallpaperUserInfoPtr wallpaper_user_info =
        mojom::WallpaperUserInfo::New();
    wallpaper_user_info->account_id = account_id;
    wallpaper_user_info->type = user_manager::USER_TYPE_REGULAR;
    wallpaper_user_info->is_ephemeral = false;
    wallpaper_user_info->has_gaia_account = true;
    ClearWallpaperCount();
    ClearDecodeFilePaths();

    return wallpaper_user_info;
  }

  // Saves images with different resolution to corresponding paths and saves
  // wallpaper info to local state, so that subsequent calls of |ShowWallpaper|
  // can retrieve the images and info.
  void CreateAndSaveWallpapers(const AccountId& account_id) {
    std::string wallpaper_files_id = GetDummyFileId(account_id);
    std::string file_name = GetDummyFileName(account_id);
    base::FilePath small_wallpaper_path =
        GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                               wallpaper_files_id, file_name);
    base::FilePath large_wallpaper_path =
        GetCustomWallpaperPath(WallpaperController::kLargeWallpaperSubDir,
                               wallpaper_files_id, file_name);

    // Saves the small/large resolution wallpapers to small/large custom
    // wallpaper paths.
    ASSERT_TRUE(WriteJPEGFile(small_wallpaper_path, kSmallWallpaperMaxWidth,
                              kSmallWallpaperMaxHeight,
                              kSmallCustomWallpaperColor));
    ASSERT_TRUE(WriteJPEGFile(large_wallpaper_path, kLargeWallpaperMaxWidth,
                              kLargeWallpaperMaxHeight,
                              kLargeCustomWallpaperColor));

    std::string relative_path =
        base::FilePath(wallpaper_files_id).Append(file_name).value();
    // Saves wallpaper info to local state for user.
    WallpaperInfo info = {relative_path, WALLPAPER_LAYOUT_CENTER_CROPPED,
                          CUSTOMIZED, base::Time::Now().LocalMidnight()};
    ASSERT_TRUE(controller_->SetUserWallpaperInfo(account_id, info,
                                                  false /*is_ephemeral=*/));
  }

  // Simulates setting a custom wallpaper by directly setting the wallpaper
  // info.
  void SimulateSettingCustomWallpaper(const AccountId& account_id) {
    ASSERT_TRUE(controller_->SetUserWallpaperInfo(
        account_id,
        WallpaperInfo("dummy_file_location", WALLPAPER_LAYOUT_CENTER,
                      CUSTOMIZED, base::Time::Now().LocalMidnight()),
        false /*is_ephemeral=*/));
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
    command_line->AppendSwitchASCII(chromeos::switches::kDefaultWallpaperSmall,
                                    small_file.value());
    const base::FilePath large_file =
        default_wallpaper_path.Append(kDefaultLargeWallpaperName);
    command_line->AppendSwitchASCII(chromeos::switches::kDefaultWallpaperLarge,
                                    large_file.value());

    const base::FilePath guest_small_file =
        default_wallpaper_path.Append(kGuestSmallWallpaperName);
    command_line->AppendSwitchASCII(chromeos::switches::kGuestWallpaperSmall,
                                    guest_small_file.value());
    const base::FilePath guest_large_file =
        default_wallpaper_path.Append(kGuestLargeWallpaperName);
    command_line->AppendSwitchASCII(chromeos::switches::kGuestWallpaperLarge,
                                    guest_large_file.value());

    const base::FilePath child_small_file =
        default_wallpaper_path.Append(kChildSmallWallpaperName);
    command_line->AppendSwitchASCII(chromeos::switches::kChildWallpaperSmall,
                                    child_small_file.value());
    const base::FilePath child_large_file =
        default_wallpaper_path.Append(kChildLargeWallpaperName);
    command_line->AppendSwitchASCII(chromeos::switches::kChildWallpaperLarge,
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

  // A helper to test the behavior of setting online wallpaper after the image
  // is decoded. This is needed because image decoding is not supported in unit
  // tests (the connector for the mojo service manager is null).
  void SetOnlineWallpaperFromImage(
      const AccountId& account_id,
      const gfx::ImageSkia& image,
      const std::string& url,
      WallpaperLayout layout,
      bool save_file,
      bool preview_mode,
      WallpaperController::SetOnlineWallpaperFromDataCallback callback) {
    const WallpaperController::OnlineWallpaperParams params = {
        account_id, false /*is_ephemeral=*/, url, layout, preview_mode};
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

  // Wrapper for private IsActiveUserWallpaperControlledByPolicyImpl().
  bool IsActiveUserWallpaperControlledByPolicy() {
    return controller_->IsActiveUserWallpaperControlledByPolicyImpl();
  }

  // Wrapper for private IsDevicePolicyWallpaper().
  bool IsDevicePolicyWallpaper() {
    return controller_->IsDevicePolicyWallpaper();
  }

  int GetWallpaperCount() { return controller_->wallpaper_count_for_testing_; }

  const std::vector<base::FilePath>& GetDecodeFilePaths() {
    return controller_->decode_requests_for_testing_;
  }

  void SetBypassDecode() { controller_->bypass_decode_for_testing_ = true; }

  void ClearWallpaperCount() { controller_->wallpaper_count_for_testing_ = 0; }

  void ClearDecodeFilePaths() {
    controller_->decode_requests_for_testing_.clear();
  }

  void ClearWallpaper() { controller_->current_wallpaper_.reset(); }

  WallpaperController* controller_;  // Not owned.

  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir online_wallpaper_dir_;
  base::ScopedTempDir custom_wallpaper_dir_;
  base::ScopedTempDir default_wallpaper_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerTest);
};

TEST_F(WallpaperControllerTest, BasicReparenting) {
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // Wallpaper view/window exists in the wallpaper container and nothing is in
  // the lock screen wallpaper container.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));

  // Moving wallpaper to lock container should succeed the first time but
  // subsequent calls should do nothing.
  EXPECT_TRUE(controller->MoveToLockedContainer());
  EXPECT_FALSE(controller->MoveToLockedContainer());

  // One window is moved from desktop to lock container.
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(1, ChildCountForContainer(kLockScreenWallpaperId));

  // Moving wallpaper to desktop container should succeed the first time.
  EXPECT_TRUE(controller->MoveToUnlockedContainer());
  EXPECT_FALSE(controller->MoveToUnlockedContainer());

  // One window is moved from lock to desktop container.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));
}

TEST_F(WallpaperControllerTest, SwitchWallpapersWhenNewWallpaperAnimationEnds) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create the wallpaper and its view.
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // The new wallpaper is ready to animate.
  WallpaperWidgetController* widget_controller =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->wallpaper_widget_controller();
  EXPECT_TRUE(widget_controller->GetAnimatingWidget());
  EXPECT_FALSE(widget_controller->GetWidget());

  // Force the animation to play to completion.
  RunDesktopControllerAnimation();
  EXPECT_FALSE(widget_controller->GetAnimatingWidget());
  EXPECT_TRUE(widget_controller->GetWidget());
}

// Test for crbug.com/149043 "Unlock screen, no launcher appears". Ensure we
// move all wallpaper views if there are more than one.
TEST_F(WallpaperControllerTest, WallpaperMovementDuringUnlock) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // User locks the screen, which moves the wallpaper forward.
  controller->MoveToLockedContainer();

  // Suspend/resume cycle causes wallpaper to refresh, loading a new wallpaper
  // that will animate in on top of the old one.
  controller->CreateEmptyWallpaperForTesting();

  // In this state we have two wallpaper views stored in different properties.
  // Both are in the lock screen wallpaper container.
  WallpaperWidgetController* widget_controller =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->wallpaper_widget_controller();
  EXPECT_TRUE(widget_controller->GetAnimatingWidget());
  EXPECT_TRUE(widget_controller->GetWidget());
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(2, ChildCountForContainer(kLockScreenWallpaperId));

  // Before the wallpaper's animation completes, user unlocks the screen, which
  // moves the wallpaper to the back.
  controller->MoveToUnlockedContainer();

  // Ensure both wallpapers have moved.
  EXPECT_EQ(2, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));

  // Finish the new wallpaper animation.
  RunDesktopControllerAnimation();

  // Now there is one wallpaper, in the back.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));
}

// Test for crbug.com/156542. Animating wallpaper should immediately finish
// animation and replace current wallpaper before next animation starts.
TEST_F(WallpaperControllerTest, ChangeWallpaperQuick) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaperForTesting();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // Change to a new wallpaper.
  controller->CreateEmptyWallpaperForTesting();

  WallpaperWidgetController* widget_controller =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->wallpaper_widget_controller();
  views::Widget* animating_widget = widget_controller->GetAnimatingWidget();
  EXPECT_TRUE(animating_widget);
  EXPECT_TRUE(widget_controller->GetWidget());

  // Change to another wallpaper before animation finished.
  controller->CreateEmptyWallpaperForTesting();

  // The animating widget should become active immediately.
  EXPECT_EQ(animating_widget, widget_controller->GetWidget());

  // Cache the new animating widget.
  animating_widget = widget_controller->GetAnimatingWidget();

  // Run wallpaper show animation to completion.
  ASSERT_NO_FATAL_FAILURE(RunAnimationForWidget(animating_widget));

  EXPECT_TRUE(widget_controller->GetWidget());
  EXPECT_FALSE(widget_controller->GetAnimatingWidget());
  // The last animating widget should be active at this point.
  EXPECT_EQ(animating_widget, widget_controller->GetWidget());
}

TEST_F(WallpaperControllerTest, ResizeCustomWallpaper) {
  UpdateDisplay("320x200");

  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);

  // Set the image as custom wallpaper, wait for the resize to finish, and check
  // that the resized image is the expected size.
  controller_->ShowWallpaperImage(image,
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  false /*preview_mode=*/);
  EXPECT_TRUE(image.BackedBySameObjectAs(controller_->GetWallpaper()));
  RunAllTasksUntilIdle();
  gfx::ImageSkia resized_image = controller_->GetWallpaper();
  EXPECT_FALSE(image.BackedBySameObjectAs(resized_image));
  EXPECT_EQ(gfx::Size(320, 200).ToString(), resized_image.size().ToString());

  // Load the original wallpaper again and check that we're still using the
  // previously-resized image instead of doing another resize
  // (http://crbug.com/321402).
  controller_->ShowWallpaperImage(image,
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(resized_image.BackedBySameObjectAs(controller_->GetWallpaper()));
}

TEST_F(WallpaperControllerTest, GetMaxDisplaySize) {
  // Device scale factor shouldn't affect the native size.
  UpdateDisplay("1000x300*2");
  EXPECT_EQ("1000x300",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // Rotated display should return the rotated size.
  UpdateDisplay("1000x300*2/r");
  EXPECT_EQ("300x1000",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // UI Scaling shouldn't affect the native size.
  UpdateDisplay("1000x300*2@1.5");
  EXPECT_EQ("1000x300",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // First display has maximum size.
  UpdateDisplay("400x300,100x100");
  EXPECT_EQ("400x300",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // Second display has maximum size.
  UpdateDisplay("400x300,500x600");
  EXPECT_EQ("500x600",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // Maximum width and height belongs to different displays.
  UpdateDisplay("400x300,100x500");
  EXPECT_EQ("400x500",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());
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
        false /*preview_mode=*/);
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        false /*preview_mode=*/);
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 high resolution"));
    controller_->ShowWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        false /*preview_mode=*/);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        false /*preview_mode=*/);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600/u@1.5");  // 1.5 ui scale
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 high resolution"));
    controller_->ShowWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        false /*preview_mode=*/);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        false /*preview_mode=*/);
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
  EXPECT_FALSE(ShouldCalculateColors());

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

TEST_F(WallpaperControllerTest, MojoWallpaperObserverTest) {
  TestWallpaperObserver observer;
  mojom::WallpaperObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::WallpaperObserver> binding(
      &observer, mojo::MakeRequestAssociatedWithDedicatedPipe(&observer_ptr));
  controller_->AddObserver(observer_ptr.PassInterface());
  controller_->FlushForTesting();

  // Adding an observer fires OnWallpaperColorsChanged() immediately.
  EXPECT_EQ(1, observer.wallpaper_colors_changed_count());

  // Enable shelf coloring will set a customized wallpaper image and change
  // session state to ACTIVE, which will trigger wallpaper colors calculation.
  base::RunLoop run_loop;
  observer.set_run_loop(&run_loop);
  EnableShelfColoring();
  // Color calculation may be asynchronous.
  run_loop.Run();
  // Mojo methods are called after color calculation finishes.
  controller_->FlushForTesting();
  EXPECT_EQ(2, observer.wallpaper_colors_changed_count());
}

TEST_F(WallpaperControllerTest, SetCustomWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(kUser1);

  // Set a custom wallpaper for |kUser1|. Verify the wallpaper is set
  // successfully and wallpaper info is updated.
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  image, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), CUSTOMIZED);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Now set another custom wallpaper for |kUser1|. Verify that the on-screen
  // wallpaper doesn't change since |kUser1| is not active, but wallpaper info
  // is updated properly.
  SimulateUserLogin(kUser2);
  const SkColor custom_wallpaper_color = SK_ColorCYAN;
  image = CreateImage(640, 480, custom_wallpaper_color);
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  image, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Verify the updated wallpaper is shown after |kUser1| becomes active again.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(custom_wallpaper_color, GetWallpaperColor());
}

TEST_F(WallpaperControllerTest, SetOnlineWallpaper) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
  SimulateUserLogin(kUser1);

  // Verify that there's no offline wallpaper available in the beginning.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  controller_->GetOfflineWallpaperList(base::BindLambdaForTesting(
      [&run_loop](const std::vector<std::string>& url_list) {
        EXPECT_TRUE(url_list.empty());
        run_loop->Quit();
      }));
  run_loop->Run();

  // Verify that the attempt to set an online wallpaper without providing image
  // data fails.
  run_loop.reset(new base::RunLoop());
  controller_->SetOnlineWallpaperIfExists(
      InitializeUser(account_id_1), kDummyUrl, layout, false /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool file_exists) {
        EXPECT_FALSE(file_exists);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());

  // Set an online wallpaper with image data. Verify that the wallpaper is set
  // successfully.
  controller_->SetOnlineWallpaperFromData(
      InitializeUser(account_id_1), std::string() /*image_data=*/, kDummyUrl,
      layout, false /*preview_mode=*/,
      WallpaperController::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), ONLINE);
  // Verify that the user wallpaper info is updated.
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo expected_wallpaper_info(kDummyUrl, layout, ONLINE,
                                        base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Change the on-screen wallpaper to a different one. (Otherwise the
  // subsequent calls will be no-op since we intentionally prevent reloading the
  // same wallpaper.)
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1, layout,
      CreateImage(640, 480, kWallpaperColor), false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), CUSTOMIZED);

  // Attempt to set an online wallpaper without providing the image data. Verify
  // it succeeds this time because |SetOnlineWallpaperFromData| has saved the
  // file.
  run_loop.reset(new base::RunLoop());
  controller_->SetOnlineWallpaperIfExists(
      InitializeUser(account_id_1), kDummyUrl, layout, false /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool file_exists) {
        EXPECT_TRUE(file_exists);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), ONLINE);

  // Verify that the wallpaper with |url| is available offline, and the returned
  // file name should not contain the small wallpaper suffix.
  run_loop.reset(new base::RunLoop());
  controller_->GetOfflineWallpaperList(base::BindLambdaForTesting(
      [&run_loop](const std::vector<std::string>& url_list) {
        EXPECT_EQ(1U, url_list.size());
        EXPECT_EQ(GURL(kDummyUrl).ExtractFileName(), url_list[0]);
        run_loop->Quit();
      }));
  run_loop->Run();

  // Log in |kUser2|, and set another online wallpaper for |kUser1|. Verify that
  // the on-screen wallpaper doesn't change since |kUser1| is not active, but
  // wallpaper info is updated properly.
  SimulateUserLogin(kUser2);
  controller_->SetOnlineWallpaperFromData(
      InitializeUser(account_id_1), std::string() /*image_data=*/, kDummyUrl2,
      layout, false /*preview_mode=*/,
      WallpaperController::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo expected_wallpaper_info_2(kDummyUrl2, layout, ONLINE,
                                          base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info_2);
}

TEST_F(WallpaperControllerTest, SetAndRemovePolicyWallpaper) {
  SetBypassDecode();
  // Simulate the login screen.
  ClearLogin();

  // The user starts with no wallpaper info and is not controlled by policy.
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 false /*is_ephemeral=*/));
  EXPECT_FALSE(
      controller_->IsPolicyControlled(account_id_1, false /*is_ephemeral=*/));
  // A default wallpaper is shown for the user.
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);

  // Set a policy wallpaper. Verify that the user becomes policy controlled and
  // the wallpaper info is updated.
  controller_->SetPolicyWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo policy_wallpaper_info(base::FilePath(wallpaper_files_id_1)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED, POLICY,
                                      base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id_1, false /*is_ephemeral=*/));
  // Verify the wallpaper is not updated since the user hasn't logged in.
  EXPECT_EQ(0, GetWallpaperCount());

  // Log in the user. Verify the policy wallpaper is now being shown.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), POLICY);

  // Clear the wallpaper and log out the user. Verify the policy wallpaper is
  // shown in the login screen.
  ClearWallpaper();
  ClearLogin();
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(controller_->GetWallpaperType(), POLICY);
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id_1, false /*is_ephemeral=*/));
  // Remove the policy wallpaper. Verify the wallpaper info is reset to default
  // and the user is no longer policy controlled.
  controller_->RemovePolicyWallpaper(InitializeUser(account_id_1),
                                     wallpaper_files_id_1);
  WaitUntilCustomWallpapersDeleted(account_id_1);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  EXPECT_FALSE(
      controller_->IsPolicyControlled(account_id_1, false /*is_ephemeral=*/));
  // Verify the wallpaper is not updated since the user hasn't logged in (to
  // avoid abrupt wallpaper change in login screen).
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), POLICY);

  // Log in the user. Verify the default wallpaper is now being shown.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
}

TEST_F(WallpaperControllerTest, RemovePolicyWallpaperNoOp) {
  auto verify_custom_wallpaper_info = [&]() {
    EXPECT_EQ(CUSTOMIZED, controller_->GetWallpaperType());
    EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
    WallpaperInfo wallpaper_info;
    EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                  false /*is_ephemeral=*/));
    WallpaperInfo expected_wallpaper_info(
        base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(),
        WALLPAPER_LAYOUT_CENTER, CUSTOMIZED, base::Time::Now().LocalMidnight());
    EXPECT_EQ(expected_wallpaper_info, wallpaper_info);
  };

  // Set a custom wallpaper. Verify the user is not policy controlled and the
  // wallpaper info is correct.
  SimulateUserLogin(kUser1);
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, CreateImage(640, 480, kWallpaperColor),
      false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_FALSE(
      controller_->IsPolicyControlled(account_id_1, false /*is_ephemeral=*/));
  verify_custom_wallpaper_info();

  // Verify RemovePolicyWallpaper() is a no-op when the user doesn't have a
  // policy wallpaper.
  controller_->RemovePolicyWallpaper(InitializeUser(account_id_1),
                                     wallpaper_files_id_1);
  RunAllTasksUntilIdle();
  verify_custom_wallpaper_info();
}

TEST_F(WallpaperControllerTest, SetThirdPartyWallpaper) {
  SetBypassDecode();
  SimulateUserLogin(kUser1);

  // Verify the user starts with no wallpaper info.
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 false /*is_ephemeral=*/));

  // Set a third-party wallpaper for |kUser1|.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia third_party_wallpaper = CreateImage(640, 480, kWallpaperColor);
  bool allowed_to_update_wallpaper = false;
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  controller_->SetThirdPartyWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1, layout,
      third_party_wallpaper,
      base::BindLambdaForTesting([&allowed_to_update_wallpaper, &run_loop](
                                     bool allowed, uint32_t image_id) {
        allowed_to_update_wallpaper = allowed;
        run_loop->Quit();
      }));
  run_loop->Run();
  // Verify the wallpaper is shown.
  EXPECT_EQ(1, GetWallpaperCount());
  // Verify the callback function gets the correct value.
  EXPECT_TRUE(allowed_to_update_wallpaper);
  // Verify the user wallpaper info is updated.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Switch active user to |kUser2|, but set another third-party wallpaper for
  // |kUser1|.
  allowed_to_update_wallpaper = true;
  SimulateUserLogin(kUser2);
  run_loop.reset(new base::RunLoop());
  controller_->SetThirdPartyWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_2, file_name_2, layout,
      third_party_wallpaper,
      base::BindLambdaForTesting([&allowed_to_update_wallpaper, &run_loop](
                                     bool allowed, uint32_t image_id) {
        allowed_to_update_wallpaper = allowed;
        run_loop->Quit();
      }));
  run_loop->Run();
  // Verify the wallpaper is not shown because |kUser1| is not the active user.
  EXPECT_EQ(0, GetWallpaperCount());
  // Verify the callback function gets the correct value.
  EXPECT_FALSE(allowed_to_update_wallpaper);
  // Verify the wallpaper info for |kUser1| is updated, because setting
  // wallpaper is still allowed for non-active users.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo expected_wallpaper_info_2(
      base::FilePath(wallpaper_files_id_2).Append(file_name_2).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info_2);

  // Set a policy wallpaper for |kUser2|. Verify that |kUser2| becomes policy
  // controlled.
  controller_->SetPolicyWallpaper(InitializeUser(account_id_2),
                                  wallpaper_files_id_2,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id_2, false /*is_ephemeral=*/));
  EXPECT_TRUE(IsActiveUserWallpaperControlledByPolicy());

  // Set a third-party wallpaper for |kUser2|.
  allowed_to_update_wallpaper = true;
  run_loop.reset(new base::RunLoop());
  controller_->SetThirdPartyWallpaper(
      InitializeUser(account_id_2), wallpaper_files_id_1, file_name_1, layout,
      third_party_wallpaper,
      base::BindLambdaForTesting([&allowed_to_update_wallpaper, &run_loop](
                                     bool allowed, uint32_t image_id) {
        allowed_to_update_wallpaper = allowed;
        run_loop->Quit();
      }));
  run_loop->Run();
  // Verify the wallpaper is not shown because third-party wallpaper cannot be
  // set for policy controlled users.
  EXPECT_EQ(0, GetWallpaperCount());
  // Verify the callback gets the correct value.
  EXPECT_FALSE(allowed_to_update_wallpaper);
  // Verify |kUser2| is still policy controlled and has the policy wallpaper
  // info.
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id_2, false /*is_ephemeral=*/));
  EXPECT_TRUE(IsActiveUserWallpaperControlledByPolicy());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_2, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo policy_wallpaper_info(base::FilePath(wallpaper_files_id_2)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED, POLICY,
                                      base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForRegularAccount) {
  CreateDefaultWallpapers();
  SimulateUserLogin(kUser1);

  // First, simulate setting a user custom wallpaper.
  SimulateSettingCustomWallpaper(account_id_1);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the large default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id_1);
  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the small default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id_1);
  // Verify that when screen is rotated, |SetDefaultWallpaper| removes the
  // previously set custom wallpaper info, and the small default wallpaper is
  // set successfully with the correct file path.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForChildAccount) {
  CreateDefaultWallpapers();

  const std::string child_email = "child@test.com";
  const AccountId child_account_id = AccountId::FromUserEmail(child_email);
  const std::string child_wallpaper_files_id = GetDummyFileId(child_account_id);
  SimulateUserLogin(child_email);

  // Verify the large child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  mojom::WallpaperUserInfoPtr wallpaper_user_info =
      InitializeUser(child_account_id);
  wallpaper_user_info->type = user_manager::USER_TYPE_CHILD;
  controller_->SetDefaultWallpaper(std::move(wallpaper_user_info),
                                   child_wallpaper_files_id,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kChildLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  // Verify the small child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  wallpaper_user_info = InitializeUser(child_account_id);
  wallpaper_user_info->type = user_manager::USER_TYPE_CHILD;
  controller_->SetDefaultWallpaper(std::move(wallpaper_user_info),
                                   child_wallpaper_files_id,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kChildSmallWallpaperName),
            GetDecodeFilePaths()[0]);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForGuestSession) {
  CreateDefaultWallpapers();

  // First, simulate setting a custom wallpaper for a regular user.
  SimulateUserLogin(kUser1);
  SimulateSettingCustomWallpaper(account_id_1);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  SimulateGuestLogin();

  // Verify that during a guest session, |SetDefaultWallpaper| removes the user
  // custom wallpaper info, but a guest specific wallpaper should be set,
  // instead of the regular default wallpaper.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestSmallWallpaperName),
            GetDecodeFilePaths()[0]);
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestInKioskMode) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  const std::string kiosk_app = "kiosk";

  // Simulate kiosk login.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(kiosk_app, user_manager::USER_TYPE_KIOSK_APP);
  session->SwitchActiveUser(AccountId::FromUserEmail(kiosk_app));
  session->SetSessionState(SessionState::ACTIVE);

  // Verify that |SetCustomWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, image, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 false /*is_ephemeral=*/));

  // Verify that |SetOnlineWallpaperFromData| doesn't set wallpaper in kiosk
  // mode, and |account_id|'s wallpaper info is not updated.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  controller_->SetOnlineWallpaperFromData(
      InitializeUser(account_id_1), std::string() /*image_data=*/, kDummyUrl,
      WALLPAPER_LAYOUT_CENTER, false /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 false /*is_ephemeral=*/));

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 false /*is_ephemeral=*/));
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestWhenPolicyIsEnforced) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(kUser1);

  // Set a policy wallpaper for the user. Verify the user is policy controlled.
  controller_->SetPolicyWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id_1, false /*is_ephemeral=*/));

  // Verify that |SetCustomWallpaper| doesn't set wallpaper when policy is
  // enforced, and the user wallpaper info is not updated.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, image, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo policy_wallpaper_info(base::FilePath(wallpaper_files_id_1)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED, POLICY,
                                      base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetOnlineWallpaperFromData| doesn't set wallpaper when policy
  // is enforced, and the user wallpaper info is not updated.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  controller_->SetOnlineWallpaperFromData(
      InitializeUser(account_id_1), std::string() /*image_data=*/, kDummyUrl,
      WALLPAPER_LAYOUT_CENTER_CROPPED, false /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper when policy is
  // enforced, and the user wallpaper info is not updated.
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);
}

TEST_F(WallpaperControllerTest, VerifyWallpaperCache) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(kUser1);

  // |kUser1| doesn't have wallpaper cache in the beginning.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  base::FilePath path;
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |SetOnlineWallpaperFromData| updates wallpaper cache for |user1|.
  controller_->SetOnlineWallpaperFromData(
      InitializeUser(account_id_1), std::string() /*image_data=*/, kDummyUrl,
      WALLPAPER_LAYOUT_CENTER, false /*preview_mode=*/,
      WallpaperController::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // After |kUser2| is logged in, |user1|'s wallpaper cache should still be kept
  // (crbug.com/339576). Note the active user is still |user1|.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(kUser2);
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |SetDefaultWallpaper| clears wallpaper cache.
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |SetCustomWallpaper| updates wallpaper cache for |user1|.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, image, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |RemoveUserWallpaper| clears wallpaper cache.
  controller_->RemoveUserWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));
}

// Tests that the appropriate wallpaper (large vs. small) is shown depending
// on the desktop resolution.
TEST_F(WallpaperControllerTest, ShowCustomWallpaperWithCorrectResolution) {
  CreateDefaultWallpapers();
  const base::FilePath small_custom_wallpaper_path =
      GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  const base::FilePath large_custom_wallpaper_path =
      GetCustomWallpaperPath(WallpaperController::kLargeWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  const base::FilePath small_default_wallpaper_path =
      default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName);
  const base::FilePath large_default_wallpaper_path =
      default_wallpaper_dir_.GetPath().Append(kDefaultLargeWallpaperName);

  CreateAndSaveWallpapers(account_id_1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
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
  UpdateDisplay("800x600,2000x2000");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  ASSERT_EQ(2u, GetDecodeFilePaths().size());
  EXPECT_EQ(large_custom_wallpaper_path, GetDecodeFilePaths()[0]);
  EXPECT_EQ(large_default_wallpaper_path, GetDecodeFilePaths()[1]);

  // Detach the secondary display.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  // Hook up the 2000x2000 display again. The large resolution default wallpaper
  // should persist. Test for crbug/165788.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,2000x2000");
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
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
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
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);
}

// Display size change should trigger reload for both user wallpaper and preview
// wallpaper.
TEST_F(WallpaperControllerTest, ReloadWallpaper) {
  CreateAndSaveWallpapers(account_id_1);

  // Show a user wallpaper.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
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
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());

  // Start wallpaper preview.
  SimulateUserLogin(kUser1);
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  wm::GetWindowState(wallpaper_picker_window.get())->Activate();
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, CreateImage(640, 480, kWallpaperColor),
      true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  // Rotating the display should trigger a wallpaper reload.
  ClearWallpaperCount();
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, UpdateCustomWallpaperLayout) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kSmallCustomWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_STRETCH;
  WallpaperLayout new_layout = WALLPAPER_LAYOUT_CENTER;
  SimulateUserLogin(kUser1);

  // Set a custom wallpaper for the user. Verify that it's set successfully
  // and the wallpaper info is updated.
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  image, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo expected_custom_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_custom_wallpaper_info);

  // Now change to a different layout. Verify that the layout is updated for
  // both the current wallpaper and the saved wallpaper info.
  controller_->UpdateCustomWallpaperLayout(InitializeUser(account_id_1),
                                           new_layout);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), new_layout);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  expected_custom_wallpaper_info.layout = new_layout;
  EXPECT_EQ(wallpaper_info, expected_custom_wallpaper_info);

  // Now set an online wallpaper. Verify that it's set successfully and the
  // wallpaper info is updated.
  image = CreateImage(640, 480, kWallpaperColor);
  controller_->SetOnlineWallpaperFromData(
      InitializeUser(account_id_1), std::string() /*image_data=*/, kDummyUrl,
      layout, false /*preview_mode=*/,
      WallpaperController::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), ONLINE);
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  WallpaperInfo expected_online_wallpaper_info(
      kDummyUrl, layout, ONLINE, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_online_wallpaper_info);

  // Now change the layout of the online wallpaper. Verify that it's a no-op.
  controller_->UpdateCustomWallpaperLayout(InitializeUser(account_id_1),
                                           new_layout);
  RunAllTasksUntilIdle();
  // The wallpaper is not updated.
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  // The saved wallpaper info is not updated.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  EXPECT_EQ(wallpaper_info, expected_online_wallpaper_info);
}

// Tests that if a user who has a custom wallpaper is removed from the device,
// only the directory that contains the user's custom wallpapers gets removed.
// The other user's custom wallpaper is not affected.
TEST_F(WallpaperControllerTest, RemoveUserWithCustomWallpaper) {
  SimulateUserLogin(kUser1);
  base::FilePath small_wallpaper_path_1 =
      GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  // Set a custom wallpaper for |kUser1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(account_id_1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a custom wallpaper for the user.
  SimulateUserLogin(kUser2);
  base::FilePath small_wallpaper_path_2 = GetCustomWallpaperPath(
      WallpaperController::kSmallWallpaperSubDir, wallpaper_files_id_2,
      GetDummyFileName(account_id_2));
  CreateAndSaveWallpapers(account_id_2);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_2));

  // Simulate the removal of |kUser2|.
  controller_->RemoveUserWallpaper(InitializeUser(account_id_2),
                                   wallpaper_files_id_2);
  // Wait until all files under the user's custom wallpaper directory are
  // removed.
  WaitUntilCustomWallpapersDeleted(account_id_2);
  EXPECT_FALSE(base::PathExists(small_wallpaper_path_2));

  // Verify that the other user's wallpaper is not affected.
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));
}

// Tests that if a user who has a default wallpaper is removed from the device,
// the other user's custom wallpaper is not affected.
TEST_F(WallpaperControllerTest, RemoveUserWithDefaultWallpaper) {
  SimulateUserLogin(kUser1);
  base::FilePath small_wallpaper_path_1 =
      GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  // Set a custom wallpaper for |kUser1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(account_id_1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a default wallpaper for the user.
  SimulateUserLogin(kUser2);
  controller_->SetDefaultWallpaper(InitializeUser(account_id_2),
                                   wallpaper_files_id_2,
                                   true /*show_wallpaper=*/);

  // Simulate the removal of |kUser2|.
  controller_->RemoveUserWallpaper(InitializeUser(account_id_2),
                                   wallpaper_files_id_2);

  // Verify that the other user's wallpaper is not affected.
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));
}

TEST_F(WallpaperControllerTest, IsActiveUserWallpaperControlledByPolicy) {
  SetBypassDecode();
  // Simulate the login screen. Verify that it returns false since there's no
  // active user.
  ClearLogin();
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());

  SimulateUserLogin(kUser1);
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());
  // Set a policy wallpaper for the active user. Verify that the active user
  // becomes policy controlled.
  controller_->SetPolicyWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(IsActiveUserWallpaperControlledByPolicy());

  // Switch the active user. Verify the active user is not policy controlled.
  SimulateUserLogin(kUser2);
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());

  // Logs out. Verify that it returns false since there's no active user.
  ClearLogin();
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());
}

TEST_F(WallpaperControllerTest, WallpaperBlur) {
  ASSERT_TRUE(controller_->IsBlurAllowed());
  ASSERT_FALSE(controller_->IsWallpaperBlurred());

  TestWallpaperControllerObserver observer;
  controller_->AddObserver(&observer);

  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOCKED);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(1, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(2, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(3, observer.wallpaper_blur_changed_count_);

  // Blur state does not change below.
  observer.Reset();
  SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::OOBE);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::UNKNOWN);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  controller_->RemoveObserver(&observer);
}

TEST_F(WallpaperControllerTest, WallpaperBlurDuringLockScreenTransition) {
  ASSERT_TRUE(controller_->IsBlurAllowed());
  ASSERT_FALSE(controller_->IsWallpaperBlurred());

  TestWallpaperControllerObserver observer;
  controller_->AddObserver(&observer);

  // Simulate lock and unlock sequence.
  controller_->UpdateWallpaperBlur(true);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(1, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOCKED);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());

  // Change of state to ACTIVE triggers post lock animation and
  // UpdateWallpaperBlur(false)
  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(2, observer.wallpaper_blur_changed_count_);

  controller_->RemoveObserver(&observer);
}

TEST_F(WallpaperControllerTest, OnlyShowDevicePolicyWallpaperOnLoginScreen) {
  SetBypassDecode();

  // Verify the device policy wallpaper is shown on login screen.
  SetSessionState(SessionState::LOGIN_PRIMARY);
  controller_->SetDeviceWallpaperPolicyEnforced(true);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_TRUE(IsDevicePolicyWallpaper());
  // Verify the device policy wallpaper shouldn't be blurred.
  ASSERT_FALSE(controller_->IsBlurAllowed());
  ASSERT_FALSE(controller_->IsWallpaperBlurred());

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
      chromeos::switches::kFirstExecAfterBoot);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kLoginManager);
  ClearLogin();

  // Show the first wallpaper. Verify that the slower animation should be used.
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Show the second wallpaper. Verify that the slower animation should not be
  // used. (Use a different user type to ensure a different wallpaper is shown,
  // otherwise requests of loading the same wallpaper are ignored.)
  controller_->ShowUserWallpaper(
      InitializeUser(AccountId::FromUserEmail("child@test.com")));
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Log in the user and show the wallpaper. Verify that the slower animation
  // should not be used.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, ShouldNotShowInitialAnimationAfterSignOut) {
  CreateDefaultWallpapers();

  // Simulate the login screen after user sign-out. Verify that the slower
  // animation should never be used.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kLoginManager);
  CreateAndSaveWallpapers(account_id_1);
  ClearLogin();

  // Show the first wallpaper.
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Show the second wallpaper.
  controller_->ShowUserWallpaper(
      InitializeUser(AccountId::FromUserEmail("child@test.com")));
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Log in the user and show the wallpaper.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, ConfirmPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  wm::GetWindowState(wallpaper_picker_window.get())->Activate();

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  custom_wallpaper, true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

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
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, custom_wallpaper_info);

  // Set an empty online wallpaper for the user, verify it fails.
  ClearWallpaperCount();
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetOnlineWallpaperFromImage(
      account_id_1, gfx::ImageSkia(), kDummyUrl, layout, false /*save_file=*/,
      true /*preview_mode=*/,
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
  run_loop.reset(new base::RunLoop());
  SetOnlineWallpaperFromImage(
      account_id_1, online_wallpaper, kDummyUrl, layout, false /*save_file=*/,
      true /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(online_wallpaper_color, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, custom_wallpaper_info);

  // Now confirm the preview wallpaper, verify that there's no wallpaper change
  // because the wallpaper is already shown.
  ClearWallpaperCount();
  controller_->ConfirmPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(online_wallpaper_color, GetWallpaperColor());
  // Verify that the user wallpaper info is now updated to the online wallpaper
  // info.
  WallpaperInfo online_wallpaper_info(kDummyUrl, layout, ONLINE,
                                      base::Time::Now().LocalMidnight());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, online_wallpaper_info);
}

TEST_F(WallpaperControllerTest, CancelPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  wm::GetWindowState(wallpaper_picker_window.get())->Activate();

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  custom_wallpaper, true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Now cancel the preview. Verify the wallpaper changes back to the default
  // and the user wallpaper info remains unchanged.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Now set an online wallpaper for the user and enable preview. Verify that
  // the wallpaper is changed to the expected color.
  const SkColor online_wallpaper_color = SK_ColorCYAN;
  gfx::ImageSkia online_wallpaper =
      CreateImage(640, 480, online_wallpaper_color);
  EXPECT_NE(online_wallpaper_color, GetWallpaperColor());
  ClearWallpaperCount();
  SetOnlineWallpaperFromImage(
      account_id_1, online_wallpaper, kDummyUrl, layout, false /*save_file=*/,
      true /*preview_mode=*/,
      WallpaperController::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(online_wallpaper_color, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Now cancel the preview. Verify the wallpaper changes back to the default
  // and the user wallpaper info remains unchanged.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(online_wallpaper_color, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);
}

TEST_F(WallpaperControllerTest, WallpaperSyncedDuringPreview) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kUser1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Simulate opening the wallpaper picker window.
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  wm::GetWindowState(wallpaper_picker_window.get())->Activate();

  // Set a custom wallpaper for the user and enable preview. Verify that the
  // wallpaper is changed to the expected color.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia custom_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  custom_wallpaper, true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Now set another custom wallpaper for the user and disable preview (this
  // happens if a custom wallpaper set on another device is being synced).
  // Verify there's no wallpaper change since preview mode shouldn't be
  // interrupted.
  const SkColor synced_custom_wallpaper_color = SK_ColorBLUE;
  gfx::ImageSkia synced_custom_wallpaper =
      CreateImage(640, 480, synced_custom_wallpaper_color);
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_2, file_name_2, layout,
      synced_custom_wallpaper, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // However, the user wallpaper info should already be updated to the new info.
  WallpaperInfo synced_custom_wallpaper_info(
      base::FilePath(wallpaper_files_id_2).Append(file_name_2).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, synced_custom_wallpaper_info);

  // Now cancel the preview. Verify the synced custom wallpaper is shown instead
  // of the initial default wallpaper, and the user wallpaper info is still
  // correct.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(synced_custom_wallpaper_color, GetWallpaperColor());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, synced_custom_wallpaper_info);

  // Repeat the above steps for online wallpapers: set a online wallpaper for
  // the user and enable preview. Verify that the wallpaper is changed to the
  // expected color.
  gfx::ImageSkia online_wallpaper = CreateImage(640, 480, kWallpaperColor);
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());

  ClearWallpaperCount();
  SetOnlineWallpaperFromImage(
      account_id_1, online_wallpaper, kDummyUrl, layout, false /*save_file=*/,
      true /*preview_mode=*/,
      WallpaperController::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, synced_custom_wallpaper_info);

  // Now set another online wallpaper for the user and disable preview. Verify
  // there's no wallpaper change since preview mode shouldn't be interrupted.
  const SkColor synced_online_wallpaper_color = SK_ColorCYAN;
  gfx::ImageSkia synced_online_wallpaper =
      CreateImage(640, 480, synced_online_wallpaper_color);
  ClearWallpaperCount();
  SetOnlineWallpaperFromImage(
      account_id_1, synced_online_wallpaper, kDummyUrl2, layout,
      false /*save_file=*/, false /*preview_mode=*/,
      WallpaperController::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // However, the user wallpaper info should already be updated to the new info.
  WallpaperInfo synced_online_wallpaper_info(kDummyUrl2, layout, ONLINE,
                                             base::Time::Now().LocalMidnight());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, synced_online_wallpaper_info);

  // Now cancel the preview. Verify the synced online wallpaper is shown instead
  // of the previous custom wallpaper, and the user wallpaper info is still
  // correct.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(synced_online_wallpaper_color, GetWallpaperColor());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      account_id_1, &user_wallpaper_info, false /*is_ephemeral=*/));
  EXPECT_EQ(user_wallpaper_info, synced_online_wallpaper_info);
}

TEST_F(WallpaperControllerTest, AddFirstWallpaperAnimationEndCallback) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<aura::Window> test_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));

  bool is_first_callback_run = false;
  controller_->AddFirstWallpaperAnimationEndCallback(
      base::BindLambdaForTesting(
          [&is_first_callback_run]() { is_first_callback_run = true; }),
      test_window.get());
  // The callback is not run because the first wallpaper hasn't been set.
  RunAllTasksUntilIdle();
  EXPECT_FALSE(is_first_callback_run);

  // Set the first wallpaper.
  controller_->ShowDefaultWallpaperForTesting();
  bool is_second_callback_run = false;
  controller_->AddFirstWallpaperAnimationEndCallback(
      base::BindLambdaForTesting(
          [&is_second_callback_run]() { is_second_callback_run = true; }),
      test_window.get());
  // Neither callback is run because the animation of the first wallpaper
  // hasn't finished yet.
  RunAllTasksUntilIdle();
  EXPECT_FALSE(is_first_callback_run);
  EXPECT_FALSE(is_second_callback_run);

  // Force the animation to complete. The two callbacks are both run.
  RunDesktopControllerAnimation();
  EXPECT_TRUE(is_first_callback_run);
  EXPECT_TRUE(is_second_callback_run);

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
  SimulateUserLogin(kUser1);
  // First, set a custom wallpaper for |kUser1|. Verify the wallpaper is shown
  // successfully and the user wallpaper info is updated.
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  custom_wallpaper, false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::CUSTOMIZED, controller_->GetWallpaperType());
  const WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      WallpaperType::CUSTOMIZED, base::Time::Now().LocalMidnight());
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  EXPECT_EQ(expected_wallpaper_info, wallpaper_info);

  // Show a one-shot wallpaper. Verify it is shown successfully.
  ClearWallpaperCount();
  constexpr SkColor kOneShotWallpaperColor = SK_ColorWHITE;
  gfx::ImageSkia one_shot_wallpaper =
      CreateImage(640, 480, kOneShotWallpaperColor);
  controller_->ShowOneShotWallpaper(one_shot_wallpaper);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kOneShotWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::ONE_SHOT, controller_->GetWallpaperType());
  EXPECT_FALSE(controller_->IsBlurAllowed());
  EXPECT_FALSE(controller_->ShouldApplyDimming());

  // Verify the user wallpaper info is unaffected, and the one-shot wallpaper
  // can be replaced by the user wallpaper.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                false /*is_ephemeral=*/));
  EXPECT_EQ(expected_wallpaper_info, wallpaper_info);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::CUSTOMIZED, controller_->GetWallpaperType());
}

TEST_F(WallpaperControllerTest, OnFirstWallpaperShown) {
  TestWallpaperControllerObserver observer;
  controller_->AddObserver(&observer);
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(0, observer.first_wallpaper_shown_count_);
  // Show the first wallpaper, verify the observer is notified.
  controller_->ShowWallpaperImage(CreateImage(640, 480, SK_ColorBLUE),
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(SK_ColorBLUE, GetWallpaperColor());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(1, observer.first_wallpaper_shown_count_);
  // Show the second wallpaper, verify the observer is not notified.
  controller_->ShowWallpaperImage(CreateImage(640, 480, SK_ColorCYAN),
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(SK_ColorCYAN, GetWallpaperColor());
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_EQ(1, observer.first_wallpaper_shown_count_);
  controller_->RemoveObserver(&observer);
}

// A test wallpaper controller client class.
class TestWallpaperControllerClient : public mojom::WallpaperControllerClient {
 public:
  TestWallpaperControllerClient() : binding_(this) {}
  ~TestWallpaperControllerClient() override = default;

  int ready_to_set_wallpaper_count() const {
    return ready_to_set_wallpaper_count_;
  }

  mojom::WallpaperControllerClientPtr CreateInterfacePtr() {
    mojom::WallpaperControllerClientPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // mojom::WallpaperControllerClient:
  void OnReadyToSetWallpaper() override { ++ready_to_set_wallpaper_count_; }
  void OpenWallpaperPicker() override {}
  void OnFirstWallpaperAnimationFinished() override {}

 private:
  int ready_to_set_wallpaper_count_ = 0;
  mojo::Binding<mojom::WallpaperControllerClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperControllerClient);
};

// Tests for cases when local state is not available.
class WallpaperControllerDisableLocalStateTest
    : public WallpaperControllerTest {
 public:
  WallpaperControllerDisableLocalStateTest() = default;
  ~WallpaperControllerDisableLocalStateTest() override = default;

  void SetUp() override {
    disable_provide_local_state();
    WallpaperControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerDisableLocalStateTest);
};

TEST_F(WallpaperControllerDisableLocalStateTest, IgnoreShowUserWallpaper) {
  TestWallpaperControllerClient client;
  controller_->SetClientForTesting(client.CreateInterfacePtr());
  SimulateUserLogin(kUser1);

  // When local state is not available, verify |ShowUserWallpaper| request is
  // ignored.
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(0, client.ready_to_set_wallpaper_count());

  // Make local state available, verify |ShowUserWallpaper| successfully shows
  // the wallpaper, and |OnReadyToSetWallpaper| is invoked.
  std::unique_ptr<TestingPrefServiceSimple> local_state =
      std::make_unique<TestingPrefServiceSimple>();
  Shell::RegisterLocalStatePrefs(local_state->registry(), true);
  ShellTestApi().OnLocalStatePrefServiceInitialized(std::move(local_state));

  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(1, client.ready_to_set_wallpaper_count());
}

}  // namespace ash
