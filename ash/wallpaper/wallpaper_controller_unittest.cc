// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller_impl.h"

#include <cmath>
#include <cstdlib>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/time/time_override.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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
constexpr int kAlwaysOnTopWallpaperId =
    kShellWindowId_AlwaysOnTopWallpaperContainer;

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
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
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

// A test wallpaper controller client class.
class TestWallpaperControllerClient : public WallpaperControllerClient {
 public:
  TestWallpaperControllerClient() = default;
  virtual ~TestWallpaperControllerClient() = default;

  size_t open_count() const { return open_count_; }
  size_t close_preview_count() const { return close_preview_count_; }

  // WallpaperControllerClient:
  void OpenWallpaperPicker() override { open_count_++; }
  void MaybeClosePreviewWallpaper() override { close_preview_count_++; }

 private:
  size_t open_count_ = 0;
  size_t close_preview_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperControllerClient);
};

// A test implementation of the WallpaperControllerObserver interface.
class TestWallpaperControllerObserver : public WallpaperControllerObserver {
 public:
  explicit TestWallpaperControllerObserver(WallpaperController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }

  ~TestWallpaperControllerObserver() override {
    controller_->RemoveObserver(this);
  }

  // WallpaperControllerObserver
  void OnWallpaperColorsChanged() override { ++colors_changed_count_; }
  void OnWallpaperBlurChanged() override { ++blur_changed_count_; }
  void OnFirstWallpaperShown() override { ++first_shown_count_; }

  int colors_changed_count() const { return colors_changed_count_; }
  int blur_changed_count() const { return blur_changed_count_; }
  int first_shown_count() const { return first_shown_count_; }

 private:
  WallpaperController* controller_;
  int colors_changed_count_ = 0;
  int blur_changed_count_ = 0;
  int first_shown_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperControllerObserver);
};

}  // namespace

class WallpaperControllerTest : public AshTestBase {
 public:
  WallpaperControllerTest() = default;
  ~WallpaperControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

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
  // TODO(bshe): Don't require tests to run animations; it's slow.
  void RunDesktopControllerAnimation() {
    WallpaperWidgetController* controller =
        Shell::Get()
            ->GetPrimaryRootWindowController()
            ->wallpaper_widget_controller();
    ASSERT_TRUE(controller);
    ASSERT_NO_FATAL_FAILURE(
        RunAnimationForLayer(controller->wallpaper_view()->layer()));
  }

  // Convenience function to ensure ShouldCalculateColors() returns true.
  void EnableShelfColoring() {
    const gfx::ImageSkia kImage = CreateImage(10, 10, kWallpaperColor);
    controller_->ShowWallpaperImage(
        kImage, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
        /*preview_mode=*/false, /*always_on_top=*/false);
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

  // Saves images with different resolution to corresponding paths and saves
  // wallpaper info to local state, so that subsequent calls of |ShowWallpaper|
  // can retrieve the images and info.
  void CreateAndSaveWallpapers(const AccountId& account_id) {
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
    ASSERT_TRUE(controller_->SetUserWallpaperInfo(account_id, info));
  }

  // Simulates setting a custom wallpaper by directly setting the wallpaper
  // info.
  void SimulateSettingCustomWallpaper(const AccountId& account_id) {
    ASSERT_TRUE(controller_->SetUserWallpaperInfo(
        account_id,
        WallpaperInfo("dummy_file_location", WALLPAPER_LAYOUT_CENTER,
                      CUSTOMIZED, base::Time::Now().LocalMidnight())));
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
  // tests.
  void SetOnlineWallpaperFromImage(
      const AccountId& account_id,
      const gfx::ImageSkia& image,
      const std::string& url,
      WallpaperLayout layout,
      bool save_file,
      bool preview_mode,
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback callback) {
    const WallpaperControllerImpl::OnlineWallpaperParams params = {
        account_id, url, layout, preview_mode};
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

  void SetBypassDecode() { controller_->bypass_decode_for_testing_ = true; }

  void ClearWallpaperCount() { controller_->wallpaper_count_for_testing_ = 0; }

  void ClearDecodeFilePaths() {
    controller_->decode_requests_for_testing_.clear();
  }

  void ClearWallpaper() { controller_->current_wallpaper_.reset(); }

  int GetWallpaperContainerId() {
    return controller_->GetWallpaperContainerId(controller_->locked_);
  }

  WallpaperControllerImpl* controller_ = nullptr;  // Not owned.

  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir online_wallpaper_dir_;
  base::ScopedTempDir custom_wallpaper_dir_;
  base::ScopedTempDir default_wallpaper_dir_;

  user_manager::FakeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerTest);
};

TEST_F(WallpaperControllerTest, Client) {
  TestWallpaperControllerClient client;
  controller_->SetClient(&client);

  base::FilePath empty_path;
  controller_->Init(empty_path, empty_path, empty_path, empty_path);

  EXPECT_EQ(0u, client.open_count());
  EXPECT_TRUE(controller_->CanOpenWallpaperPicker());
  controller_->OpenWallpaperPickerIfAllowed();
  EXPECT_EQ(1u, client.open_count());
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
  ASSERT_NO_FATAL_FAILURE(
      RunAnimationForLayer(widget_controller->wallpaper_view()->layer()));

  EXPECT_FALSE(widget_controller->IsAnimating());
}

TEST_F(WallpaperControllerTest, ResizeCustomWallpaper) {
  UpdateDisplay("320x200");

  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);

  // Set the image as custom wallpaper, wait for the resize to finish, and check
  // that the resized image is the expected size.
  controller_->ShowWallpaperImage(
      image, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
      /*preview_mode=*/false, /*always_on_top=*/false);
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
      /*preview_mode=*/false, /*always_on_top=*/false);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(resized_image.BackedBySameObjectAs(controller_->GetWallpaper()));
}

TEST_F(WallpaperControllerTest, GetMaxDisplaySize) {
  // Device scale factor shouldn't affect the native size.
  UpdateDisplay("1000x300*2");
  EXPECT_EQ("1000x300",
            WallpaperControllerImpl::GetMaxDisplaySizeInNative().ToString());

  // Rotated display should return the rotated size.
  UpdateDisplay("1000x300*2/r");
  EXPECT_EQ("300x1000",
            WallpaperControllerImpl::GetMaxDisplaySizeInNative().ToString());

  // UI Scaling shouldn't affect the native size.
  UpdateDisplay("1000x300*2@1.5");
  EXPECT_EQ("1000x300",
            WallpaperControllerImpl::GetMaxDisplaySizeInNative().ToString());

  // First display has maximum size.
  UpdateDisplay("400x300,100x100");
  EXPECT_EQ("400x300",
            WallpaperControllerImpl::GetMaxDisplaySizeInNative().ToString());

  // Second display has maximum size.
  UpdateDisplay("400x300,500x600");
  EXPECT_EQ("500x600",
            WallpaperControllerImpl::GetMaxDisplaySizeInNative().ToString());

  // Maximum width and height belongs to different displays.
  UpdateDisplay("400x300,100x500");
  EXPECT_EQ("400x500",
            WallpaperControllerImpl::GetMaxDisplaySizeInNative().ToString());
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
        /*preview_mode=*/false, /*always_on_top=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*always_on_top=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 high resolution"));
    controller_->ShowWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*always_on_top=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*always_on_top=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600/u@1.5");  // 1.5 ui scale
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 high resolution"));
    controller_->ShowWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*always_on_top=*/false);
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 low resolution"));
    controller_->ShowWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER),
        /*preview_mode=*/false, /*always_on_top=*/false);
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

TEST_F(WallpaperControllerTest, EnableShelfColoringNotifiesObservers) {
  TestWallpaperControllerObserver observer(controller_);
  EXPECT_EQ(0, observer.colors_changed_count());

  // Enable shelf coloring will set a customized wallpaper image and change
  // session state to ACTIVE, which will trigger wallpaper colors calculation.
  EnableShelfColoring();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.colors_changed_count());
}

TEST_F(WallpaperControllerTest, SetCustomWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(kUser1);

  // Set a custom wallpaper for |kUser1|. Verify the wallpaper is set
  // successfully and wallpaper info is updated.
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, image,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), CUSTOMIZED);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
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
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, image,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Verify the updated wallpaper is shown after |kUser1| becomes active again.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
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
  ClearWallpaperCount();
  controller_->SetOnlineWallpaperIfExists(
      account_id_1, kDummyUrl, layout, false /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool file_exists) {
        EXPECT_FALSE(file_exists);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());

  // Set an online wallpaper with image data. Verify that the wallpaper is set
  // successfully.
  ClearWallpaperCount();
  controller_->SetOnlineWallpaperFromData(
      account_id_1, std::string() /*image_data=*/, kDummyUrl, layout,
      false /*preview_mode=*/,
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), ONLINE);
  // Verify that the user wallpaper info is updated.
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(kDummyUrl, layout, ONLINE,
                                        base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Change the on-screen wallpaper to a different one. (Otherwise the
  // subsequent calls will be no-op since we intentionally prevent reloading the
  // same wallpaper.)
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(
      account_id_1, wallpaper_files_id_1, file_name_1, layout,
      CreateImage(640, 480, kWallpaperColor), false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), CUSTOMIZED);

  // Attempt to set an online wallpaper without providing the image data. Verify
  // it succeeds this time because |SetOnlineWallpaperFromData| has saved the
  // file.
  ClearWallpaperCount();
  run_loop.reset(new base::RunLoop());
  controller_->SetOnlineWallpaperIfExists(
      account_id_1, kDummyUrl, layout, false /*preview_mode=*/,
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
  ClearWallpaperCount();
  controller_->SetOnlineWallpaperFromData(
      account_id_1, std::string() /*image_data=*/, kDummyUrl2, layout,
      false /*preview_mode=*/,
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
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
  EXPECT_FALSE(
      controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  EXPECT_FALSE(controller_->IsPolicyControlled(account_id_1));
  // A default wallpaper is shown for the user.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);

  // Set a policy wallpaper. Verify that the user becomes policy controlled and
  // the wallpaper info is updated.
  ClearWallpaperCount();
  controller_->SetPolicyWallpaper(account_id_1, wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo policy_wallpaper_info(base::FilePath(wallpaper_files_id_1)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED, POLICY,
                                      base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);
  EXPECT_TRUE(controller_->IsPolicyControlled(account_id_1));
  // Verify the wallpaper is not updated since the user hasn't logged in.
  EXPECT_EQ(0, GetWallpaperCount());

  // Log in the user. Verify the policy wallpaper is now being shown.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), POLICY);

  // Clear the wallpaper and log out the user. Verify the policy wallpaper is
  // shown in the login screen.
  ClearWallpaper();
  ClearLogin();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(controller_->GetWallpaperType(), POLICY);
  EXPECT_TRUE(controller_->IsPolicyControlled(account_id_1));
  // Remove the policy wallpaper. Verify the wallpaper info is reset to default
  // and the user is no longer policy controlled.
  ClearWallpaperCount();
  controller_->RemovePolicyWallpaper(account_id_1, wallpaper_files_id_1);
  WaitUntilCustomWallpapersDeleted(account_id_1);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  EXPECT_FALSE(controller_->IsPolicyControlled(account_id_1));
  // Verify the wallpaper is not updated since the user hasn't logged in (to
  // avoid abrupt wallpaper change in login screen).
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), POLICY);

  // Log in the user. Verify the default wallpaper is now being shown.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
}

TEST_F(WallpaperControllerTest, RemovePolicyWallpaperNoOp) {
  auto verify_custom_wallpaper_info = [&]() {
    EXPECT_EQ(CUSTOMIZED, controller_->GetWallpaperType());
    EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
    WallpaperInfo wallpaper_info;
    EXPECT_TRUE(
        controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
    WallpaperInfo expected_wallpaper_info(
        base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(),
        WALLPAPER_LAYOUT_CENTER, CUSTOMIZED, base::Time::Now().LocalMidnight());
    EXPECT_EQ(expected_wallpaper_info, wallpaper_info);
  };

  // Set a custom wallpaper. Verify the user is not policy controlled and the
  // wallpaper info is correct.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(
      account_id_1, wallpaper_files_id_1, file_name_1, WALLPAPER_LAYOUT_CENTER,
      CreateImage(640, 480, kWallpaperColor), false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_FALSE(controller_->IsPolicyControlled(account_id_1));
  verify_custom_wallpaper_info();

  // Verify RemovePolicyWallpaper() is a no-op when the user doesn't have a
  // policy wallpaper.
  controller_->RemovePolicyWallpaper(account_id_1, wallpaper_files_id_1);
  RunAllTasksUntilIdle();
  verify_custom_wallpaper_info();
}

TEST_F(WallpaperControllerTest, SetThirdPartyWallpaper) {
  SetBypassDecode();
  SimulateUserLogin(kUser1);

  // Verify the user starts with no wallpaper info.
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(
      controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));

  // Set a third-party wallpaper for |kUser1|.
  const WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  gfx::ImageSkia third_party_wallpaper = CreateImage(640, 480, kWallpaperColor);
  ClearWallpaperCount();
  EXPECT_TRUE(controller_->SetThirdPartyWallpaper(
      account_id_1, wallpaper_files_id_1, file_name_1, layout,
      third_party_wallpaper));
  // Verify the wallpaper is shown.
  EXPECT_EQ(1, GetWallpaperCount());
  // Verify the user wallpaper info is updated.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Switch active user to |kUser2|, but set another third-party wallpaper for
  // |kUser1|; the operation should not be allowed, because |kUser1| is not the
  // active user.
  SimulateUserLogin(kUser2);
  ClearWallpaperCount();
  EXPECT_FALSE(controller_->SetThirdPartyWallpaper(
      account_id_1, wallpaper_files_id_2, file_name_2, layout,
      third_party_wallpaper));
  // Verify the wallpaper is not shown.
  EXPECT_EQ(0, GetWallpaperCount());
  // Verify the wallpaper info for |kUser1| is updated, because setting
  // wallpaper is still allowed for non-active users.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo expected_wallpaper_info_2(
      base::FilePath(wallpaper_files_id_2).Append(file_name_2).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info_2);

  // Set a policy wallpaper for |kUser2|. Verify that |kUser2| becomes policy
  // controlled.
  controller_->SetPolicyWallpaper(account_id_2, wallpaper_files_id_2,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsPolicyControlled(account_id_2));
  EXPECT_TRUE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Setting a third-party wallpaper for |kUser2| should not be allowed, because
  // third-party wallpapers cannot be set for policy controlled users.
  ClearWallpaperCount();
  EXPECT_FALSE(controller_->SetThirdPartyWallpaper(
      account_id_2, wallpaper_files_id_1, file_name_1, layout,
      third_party_wallpaper));
  // Verify the wallpaper is not shown.
  EXPECT_EQ(0, GetWallpaperCount());
  // Verify |kUser2| is still policy controlled and has the policy wallpaper
  // info.
  EXPECT_TRUE(controller_->IsPolicyControlled(account_id_2));
  EXPECT_TRUE(controller_->IsActiveUserWallpaperControlledByPolicy());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_2, &wallpaper_info));
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
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the large default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(account_id_1, wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id_1);
  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the small default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(account_id_1, wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id_1);
  // Verify that when screen is rotated, |SetDefaultWallpaper| removes the
  // previously set custom wallpaper info, and the small default wallpaper is
  // set successfully with the correct file path.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(account_id_1, wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName),
            GetDecodeFilePaths()[0]);

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForChildAccount) {
  CreateDefaultWallpapers();

  const std::string child_email = "child@test.com";
  const AccountId child_account_id = AccountId::FromUserEmail(child_email);
  const std::string child_wallpaper_files_id = GetDummyFileId(child_account_id);
  fake_user_manager_->AddChildUser(child_account_id);

  // Verify the large child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(child_account_id, child_wallpaper_files_id,
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
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(child_account_id, child_wallpaper_files_id,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
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
  fake_user_manager_->AddGuestUser(guest_id);
  controller_->SetDefaultWallpaper(guest_id, wallpaper_files_id_1,
                                   /*show_wallpaper=*/true);

  WallpaperInfo wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  // Verify that the current displayed wallpaper is the default one inside the
  // guest session.
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  // Second, set a user policy for which is being set for another
  // user and verifying that the policy has been applied successfully.
  WallpaperInfo policy_wallpaper_info;
  controller_->SetPolicyWallpaper(account_id_1, wallpaper_files_id_2,
                                  /*data=*/std::string());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &policy_wallpaper_info));
  WallpaperInfo expected_policy_wallpaper_info(
      base::FilePath(wallpaper_files_id_2)
          .Append("policy-controlled.jpeg")
          .value(),
      WALLPAPER_LAYOUT_CENTER_CROPPED, POLICY,
      base::Time::Now().LocalMidnight());
  EXPECT_EQ(policy_wallpaper_info, expected_policy_wallpaper_info);
  EXPECT_TRUE(controller_->IsPolicyControlled(account_id_1));

  // Finally, verifying that the guest session hasn't been affected by the new
  // policy and |ShowWallpaperImage| hasn't been invoked another time.

  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestLargeWallpaperName),
            GetDecodeFilePaths()[0]);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForGuestSession) {
  CreateDefaultWallpapers();

  // First, simulate setting a custom wallpaper for a regular user.
  SimulateUserLogin(kUser1);
  SimulateSettingCustomWallpaper(account_id_1);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  const AccountId guest_id =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  fake_user_manager_->AddGuestUser(guest_id);

  // Verify that during a guest session, |SetDefaultWallpaper| removes the user
  // custom wallpaper info, but a guest specific wallpaper should be set,
  // instead of the regular default wallpaper.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(guest_id, wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(guest_id, &wallpaper_info));
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  ASSERT_EQ(1u, GetDecodeFilePaths().size());
  EXPECT_EQ(default_wallpaper_dir_.GetPath().Append(kGuestLargeWallpaperName),
            GetDecodeFilePaths()[0]);

  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  controller_->SetDefaultWallpaper(guest_id, wallpaper_files_id_1,
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
  SimulateUserLogin("kiosk", user_manager::USER_TYPE_KIOSK_APP);

  // Verify that |SetCustomWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id_1|'s wallpaper info is not updated.
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, WALLPAPER_LAYOUT_CENTER, image,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  WallpaperInfo wallpaper_info;
  EXPECT_FALSE(
      controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));

  // Verify that |SetOnlineWallpaperFromData| doesn't set wallpaper in kiosk
  // mode, and |account_id_1|'s wallpaper info is not updated.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  controller_->SetOnlineWallpaperFromData(
      account_id_1, std::string() /*image_data=*/, kDummyUrl,
      WALLPAPER_LAYOUT_CENTER, false /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(
      controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id_1|'s wallpaper info is not updated.
  ClearWallpaperCount();
  controller_->SetDefaultWallpaper(account_id_1, wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(
      controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestWhenPolicyIsEnforced) {
  SetBypassDecode();
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(kUser1);

  // Set a policy wallpaper for the user. Verify the user is policy controlled.
  controller_->SetPolicyWallpaper(account_id_1, wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsPolicyControlled(account_id_1));

  // Verify that |SetCustomWallpaper| doesn't set wallpaper when policy is
  // enforced, and the user wallpaper info is not updated.
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, WALLPAPER_LAYOUT_CENTER, image,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo policy_wallpaper_info(base::FilePath(wallpaper_files_id_1)
                                          .Append("policy-controlled.jpeg")
                                          .value(),
                                      WALLPAPER_LAYOUT_CENTER_CROPPED, POLICY,
                                      base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetOnlineWallpaperFromData| doesn't set wallpaper when policy
  // is enforced, and the user wallpaper info is not updated.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ClearWallpaperCount();
  controller_->SetOnlineWallpaperFromData(
      account_id_1, std::string() /*image_data=*/, kDummyUrl,
      WALLPAPER_LAYOUT_CENTER_CROPPED, false /*preview_mode=*/,
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper when policy is
  // enforced, and the user wallpaper info is not updated.
  ClearWallpaperCount();
  controller_->SetDefaultWallpaper(account_id_1, wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
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
      account_id_1, std::string() /*image_data=*/, kDummyUrl,
      WALLPAPER_LAYOUT_CENTER, false /*preview_mode=*/,
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback());
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
  controller_->SetDefaultWallpaper(account_id_1, wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |SetCustomWallpaper| updates wallpaper cache for |user1|.
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, WALLPAPER_LAYOUT_CENTER, image,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |RemoveUserWallpaper| clears wallpaper cache.
  controller_->RemoveUserWallpaper(account_id_1, wallpaper_files_id_1);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));
}

// Tests that the appropriate wallpaper (large vs. small) is shown depending
// on the desktop resolution.
TEST_F(WallpaperControllerTest, ShowCustomWallpaperWithCorrectResolution) {
  CreateDefaultWallpapers();
  const base::FilePath small_custom_wallpaper_path =
      GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  const base::FilePath large_custom_wallpaper_path =
      GetCustomWallpaperPath(WallpaperControllerImpl::kLargeWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  const base::FilePath small_default_wallpaper_path =
      default_wallpaper_dir_.GetPath().Append(kDefaultSmallWallpaperName);
  const base::FilePath large_default_wallpaper_path =
      default_wallpaper_dir_.GetPath().Append(kDefaultLargeWallpaperName);

  CreateAndSaveWallpapers(account_id_1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
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

// Display size change should trigger wallpaper reload.
TEST_F(WallpaperControllerTest, ReloadWallpaper) {
  CreateAndSaveWallpapers(account_id_1);

  // Show a user wallpaper.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
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
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());

  // Start wallpaper preview.
  SimulateUserLogin(kUser1);
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(
      account_id_1, wallpaper_files_id_1, file_name_1, WALLPAPER_LAYOUT_CENTER,
      CreateImage(640, 480, kWallpaperColor), true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  // Rotating the display should trigger a wallpaper reload.
  ClearWallpaperCount();
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());

  // Show an always-on-top wallpaper.
  const base::FilePath image_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          chromeos::switches::kGuestWallpaperLarge);
  CreateDefaultWallpapers();
  SetBypassDecode();
  ClearWallpaperCount();
  controller_->ShowAlwaysOnTopWallpaper(image_path);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  // Rotating the display should trigger a wallpaper reload.
  ClearWallpaperCount();
  UpdateDisplay("800x600/r");
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
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, image,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo expected_custom_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_custom_wallpaper_info);

  // Now change to a different layout. Verify that the layout is updated for
  // both the current wallpaper and the saved wallpaper info.
  ClearWallpaperCount();
  controller_->UpdateCustomWallpaperLayout(account_id_1, new_layout);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), new_layout);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  expected_custom_wallpaper_info.layout = new_layout;
  EXPECT_EQ(wallpaper_info, expected_custom_wallpaper_info);

  // Now set an online wallpaper. Verify that it's set successfully and the
  // wallpaper info is updated.
  image = CreateImage(640, 480, kWallpaperColor);
  ClearWallpaperCount();
  controller_->SetOnlineWallpaperFromData(
      account_id_1, std::string() /*image_data=*/, kDummyUrl, layout,
      false /*preview_mode=*/,
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), ONLINE);
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  WallpaperInfo expected_online_wallpaper_info(
      kDummyUrl, layout, ONLINE, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_online_wallpaper_info);

  // Now change the layout of the online wallpaper. Verify that it's a no-op.
  ClearWallpaperCount();
  controller_->UpdateCustomWallpaperLayout(account_id_1, new_layout);
  RunAllTasksUntilIdle();
  // The wallpaper is not updated.
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  // The saved wallpaper info is not updated.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  EXPECT_EQ(wallpaper_info, expected_online_wallpaper_info);
}

// Tests that if a user who has a custom wallpaper is removed from the device,
// only the directory that contains the user's custom wallpapers gets removed.
// The other user's custom wallpaper is not affected.
TEST_F(WallpaperControllerTest, RemoveUserWithCustomWallpaper) {
  SimulateUserLogin(kUser1);
  base::FilePath small_wallpaper_path_1 =
      GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  // Set a custom wallpaper for |kUser1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(account_id_1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a custom wallpaper for the user.
  SimulateUserLogin(kUser2);
  base::FilePath small_wallpaper_path_2 = GetCustomWallpaperPath(
      WallpaperControllerImpl::kSmallWallpaperSubDir, wallpaper_files_id_2,
      GetDummyFileName(account_id_2));
  CreateAndSaveWallpapers(account_id_2);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_2));

  // Simulate the removal of |kUser2|.
  controller_->RemoveUserWallpaper(account_id_2, wallpaper_files_id_2);
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
      GetCustomWallpaperPath(WallpaperControllerImpl::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  // Set a custom wallpaper for |kUser1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(account_id_1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a default wallpaper for the user.
  SimulateUserLogin(kUser2);
  controller_->SetDefaultWallpaper(account_id_2, wallpaper_files_id_2,
                                   true /*show_wallpaper=*/);

  // Simulate the removal of |kUser2|.
  controller_->RemoveUserWallpaper(account_id_2, wallpaper_files_id_2);

  // Verify that the other user's wallpaper is not affected.
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));
}

TEST_F(WallpaperControllerTest, IsActiveUserWallpaperControlledByPolicy) {
  SetBypassDecode();
  // Simulate the login screen. Verify that it returns false since there's no
  // active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  SimulateUserLogin(kUser1);
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());
  // Set a policy wallpaper for the active user. Verify that the active user
  // becomes policy controlled.
  controller_->SetPolicyWallpaper(account_id_1, wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Switch the active user. Verify the active user is not policy controlled.
  SimulateUserLogin(kUser2);
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());

  // Logs out. Verify that it returns false since there's no active user.
  ClearLogin();
  EXPECT_FALSE(controller_->IsActiveUserWallpaperControlledByPolicy());
}

TEST_F(WallpaperControllerTest, WallpaperBlur) {
  TestWallpaperControllerObserver observer(controller_);

  ASSERT_TRUE(controller_->IsBlurAllowedForLockState());
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
      /*preview_mode=*/false, /*always_on_top=*/false);

  TestWallpaperControllerObserver observer(controller_);

  ASSERT_TRUE(controller_->IsBlurAllowedForLockState());
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
      /*preview_mode=*/false, /*always_on_top=*/false);
  TestWallpaperControllerObserver observer(controller_);

  Shell::Get()->overview_controller()->StartOverview();

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
  ASSERT_EQ(30, wallpaper_view->property().blur_sigma);
  ASSERT_EQ(1, wallpaper_view->property().opacity);
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
  ASSERT_FALSE(controller_->IsBlurAllowedForLockState());
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
      chromeos::switches::kFirstExecAfterBoot);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kLoginManager);
  ClearLogin();

  // Show the first wallpaper. Verify that the slower animation should be used.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Show the second wallpaper. Verify that the slower animation should not be
  // used. (Use a different user type to ensure a different wallpaper is shown,
  // otherwise requests of loading the same wallpaper are ignored.)
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(AccountId::FromUserEmail("child@test.com"));
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Log in the user and show the wallpaper. Verify that the slower animation
  // should not be used.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
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
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Show the second wallpaper.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(AccountId::FromUserEmail("child@test.com"));
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());

  // Log in the user and show the wallpaper.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(controller_->ShouldShowInitialAnimation());
  EXPECT_EQ(1, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, ClosePreviewWallpaperOnOverviewStart) {
  TestWallpaperControllerClient client;
  controller_->SetClient(&client);

  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

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
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, custom_wallpaper,
                                  true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Now enter overview mode. Verify the wallpaper changes back to the default,
  // the user wallpaper info remains unchanged, and enters overview mode
  // properly.
  ClearWallpaperCount();
  Shell::Get()->overview_controller()->StartOverview();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(1u, client.close_preview_count());
}

TEST_F(WallpaperControllerTest, ClosePreviewWallpaperOnWindowCycleStart) {
  TestWallpaperControllerClient client;
  controller_->SetClient(&client);

  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

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
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, custom_wallpaper,
                                  true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Now start window cycle. Verify the wallpaper changes back to the default,
  // the user wallpaper info remains unchanged, and enters window cycle.
  ClearWallpaperCount();
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::FORWARD);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_NE(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);
  EXPECT_TRUE(Shell::Get()->window_cycle_controller()->IsCycling());
  EXPECT_EQ(1u, client.close_preview_count());
}

TEST_F(WallpaperControllerTest, ConfirmPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

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
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, custom_wallpaper,
                                  true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, online_wallpaper_info);
}

TEST_F(WallpaperControllerTest, CancelPreviewWallpaper) {
  // Verify the user starts with a default wallpaper and the user wallpaper info
  // is initialized with default values.
  SimulateUserLogin(kUser1);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

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
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, custom_wallpaper,
                                  true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(online_wallpaper_color, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  WallpaperInfo user_wallpaper_info;
  WallpaperInfo default_wallpaper_info(std::string(),
                                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEFAULT,
                                       base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

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
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, custom_wallpaper,
                                  true /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, default_wallpaper_info);

  // Now set another custom wallpaper for the user and disable preview (this
  // happens if a custom wallpaper set on another device is being synced).
  // Verify there's no wallpaper change since preview mode shouldn't be
  // interrupted.
  const SkColor synced_custom_wallpaper_color = SK_ColorBLUE;
  gfx::ImageSkia synced_custom_wallpaper =
      CreateImage(640, 480, synced_custom_wallpaper_color);
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_2,
                                  file_name_2, layout, synced_custom_wallpaper,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // However, the user wallpaper info should already be updated to the new info.
  WallpaperInfo synced_custom_wallpaper_info(
      base::FilePath(wallpaper_files_id_2).Append(file_name_2).value(), layout,
      CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, synced_custom_wallpaper_info);

  // Now cancel the preview. Verify the synced custom wallpaper is shown instead
  // of the initial default wallpaper, and the user wallpaper info is still
  // correct.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(synced_custom_wallpaper_color, GetWallpaperColor());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // Verify that the user wallpaper info remains unchanged during the preview.
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
      WallpaperControllerImpl::SetOnlineWallpaperFromDataCallback());
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  // However, the user wallpaper info should already be updated to the new info.
  WallpaperInfo synced_online_wallpaper_info(kDummyUrl2, layout, ONLINE,
                                             base::Time::Now().LocalMidnight());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
  EXPECT_EQ(user_wallpaper_info, synced_online_wallpaper_info);

  // Now cancel the preview. Verify the synced online wallpaper is shown instead
  // of the previous custom wallpaper, and the user wallpaper info is still
  // correct.
  ClearWallpaperCount();
  controller_->CancelPreviewWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(synced_online_wallpaper_color, GetWallpaperColor());
  EXPECT_TRUE(
      controller_->GetUserWallpaperInfo(account_id_1, &user_wallpaper_info));
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
  {
    // The animation is quite short (0.01 seconds) which is problematic in
    // debug builds if RunAllTasksUntilIdle is a bit slow to execute. That leads
    // to test flakes. We work around that by temporarily freezing time, which
    // prevents the animation from unexpectedly completing too soon.
    // Ideally this test should use MockTime instead, which will become easier
    // after https://crrev.com/c/1352260 lands.
    base::subtle::ScopedTimeClockOverrides time_override(
        nullptr,
        []() {
          static base::TimeTicks time_ticks =
              base::subtle::TimeTicksNowIgnoringOverride();
          return time_ticks;
        },
        nullptr);

    RunAllTasksUntilIdle();
  }
  // Neither callback is run because the animation of the first wallpaper
  // hasn't finished yet.
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
  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, layout, custom_wallpaper,
                                  false /*preview_mode=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::CUSTOMIZED, controller_->GetWallpaperType());
  const WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      WallpaperType::CUSTOMIZED, base::Time::Now().LocalMidnight());
  WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
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
  EXPECT_FALSE(controller_->IsBlurAllowedForLockState());
  EXPECT_FALSE(controller_->ShouldApplyDimming());

  // Verify that we can reload wallpaer without losing it.
  // This is important for screen rotation.
  controller_->ReloadWallpaperForTesting(/*clear_cache=*/false);
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());  // Reload increments count.
  EXPECT_EQ(kOneShotWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::ONE_SHOT, controller_->GetWallpaperType());
  EXPECT_FALSE(controller_->IsBlurAllowedForLockState());
  EXPECT_FALSE(controller_->ShouldApplyDimming());

  // Verify the user wallpaper info is unaffected, and the one-shot wallpaper
  // can be replaced by the user wallpaper.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info));
  EXPECT_EQ(expected_wallpaper_info, wallpaper_info);
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
  EXPECT_EQ(WallpaperType::CUSTOMIZED, controller_->GetWallpaperType());
}

TEST_F(WallpaperControllerTest, OnFirstWallpaperShown) {
  TestWallpaperControllerObserver observer(controller_);
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(0, observer.first_shown_count());
  // Show the first wallpaper, verify the observer is notified.
  controller_->ShowWallpaperImage(CreateImage(640, 480, SK_ColorBLUE),
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  /*preview_mode=*/false,
                                  /*always_on_top=*/false);
  RunAllTasksUntilIdle();
  EXPECT_EQ(SK_ColorBLUE, GetWallpaperColor());
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(1, observer.first_shown_count());
  // Show the second wallpaper, verify the observer is not notified.
  controller_->ShowWallpaperImage(CreateImage(640, 480, SK_ColorCYAN),
                                  CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH),
                                  /*preview_mode=*/false,
                                  /*always_on_top=*/false);
  RunAllTasksUntilIdle();
  EXPECT_EQ(SK_ColorCYAN, GetWallpaperColor());
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_EQ(1, observer.first_shown_count());
}

// Although ephemeral users' custom wallpapers are not saved to disk, they
// should be kept within the user session. Test for https://crbug.com/825237.
TEST_F(WallpaperControllerTest, ShowWallpaperForEphemeralUser) {
  // Add an ephemeral user session and simulate login, like SimulateUserLogin.
  UserSession session;
  session.session_id = 0;
  session.user_info.account_id = account_id_1;
  session.user_info.is_ephemeral = true;
  Shell::Get()->session_controller()->UpdateUserSession(std::move(session));
  TestSessionControllerClient* const client = GetSessionControllerClient();
  client->ProvidePrefServiceForUser(account_id_1);
  client->SwitchActiveUser(AccountId::FromUserEmail(kUser1));
  client->SetSessionState(SessionState::ACTIVE);

  // The user doesn't have wallpaper cache in the beginning.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  base::FilePath path;
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));

  ClearWallpaperCount();
  controller_->SetCustomWallpaper(account_id_1, wallpaper_files_id_1,
                                  file_name_1, WALLPAPER_LAYOUT_CENTER,
                                  CreateImage(640, 480, kWallpaperColor),
                                  /*preview_mode=*/false);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(CUSTOMIZED, controller_->GetWallpaperType());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());

  // The custom wallpaper is cached.
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_EQ(
      kWallpaperColor,
      cached_wallpaper.GetRepresentation(1.0f).GetBitmap().getColor(0, 0));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // Calling |ShowUserWallpaper| will continue showing the custom wallpaper
  // instead of reverting to the default.
  ClearWallpaperCount();
  controller_->ShowUserWallpaper(account_id_1);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(CUSTOMIZED, controller_->GetWallpaperType());
  EXPECT_EQ(kWallpaperColor, GetWallpaperColor());
}

TEST_F(WallpaperControllerTest, AlwaysOnTopWallpaper) {
  CreateDefaultWallpapers();
  SetBypassDecode();

  // Show a default wallpaper.
  EXPECT_EQ(0, GetWallpaperCount());
  controller_->ShowSigninWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // Show an always-on-top wallpaper.
  const base::FilePath image_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          chromeos::switches::kGuestWallpaperLarge);
  controller_->ShowAlwaysOnTopWallpaper(image_path);
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), ONE_SHOT);
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(1, ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // Subsequent wallpaper requests are ignored when the current wallpaper is
  // always-on-top.
  controller_->ShowSigninWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(2, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), ONE_SHOT);
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(1, ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // The wallpaper reverts to the default after the always-on-top wallpaper is
  // removed.
  controller_->RemoveAlwaysOnTopWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(3, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kAlwaysOnTopWallpaperId));

  // Calling |RemoveAlwaysOnTopWallpaper| is a no-op when the current wallpaper
  // is not always-on-top.
  controller_->RemoveAlwaysOnTopWallpaper();
  RunAllTasksUntilIdle();
  EXPECT_EQ(3, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), DEFAULT);
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kAlwaysOnTopWallpaperId));
}

namespace {

class WallpaperControllerPrefTest : public AshTestBase {
 public:
  WallpaperControllerPrefTest() {
    auto property = std::make_unique<base::DictionaryValue>();
    property->SetInteger("rotation",
                         static_cast<int>(display::Display::ROTATE_90));
    property->SetInteger("width", 800);
    property->SetInteger("height", 600);

    DictionaryPrefUpdate update(local_state(), prefs::kDisplayProperties);
    update.Get()->Set("2200000000", std::move(property));
  }

  ~WallpaperControllerPrefTest() override = default;
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

}  // namespace ash
