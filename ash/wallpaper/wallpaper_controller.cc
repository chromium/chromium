// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_decoder.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_window_state_manager.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/widget.h"

using color_utils::ColorProfile;
using color_utils::LumaRange;
using color_utils::SaturationRange;

namespace ash {

namespace {

// Names of nodes with wallpaper info in |kUserWallpaperInfo| dictionary.
constexpr char kNewWallpaperDateNodeName[] = "date";
constexpr char kNewWallpaperLayoutNodeName[] = "layout";
constexpr char kNewWallpaperLocationNodeName[] = "file";
constexpr char kNewWallpaperTypeNodeName[] = "type";

// The file name of the policy wallpaper.
constexpr char kPolicyWallpaperFile[] = "policy-controlled.jpeg";

// File path suffix of resized small wallpapers.
constexpr char kSmallWallpaperSuffix[] = "_small";

// How long to wait reloading the wallpaper after the display size has changed.
constexpr base::TimeDelta kWallpaperReloadDelay =
    base::TimeDelta::FromMilliseconds(100);

// How long to wait for resizing of the the wallpaper.
constexpr base::TimeDelta kCompositorLockTimeout =
    base::TimeDelta::FromMilliseconds(750);

// Default quality for encoding wallpaper.
constexpr int kDefaultEncodingQuality = 90;

// The color of the wallpaper if no other wallpaper images are available.
constexpr SkColor kDefaultWallpaperColor = SK_ColorGRAY;

// The paths of wallpaper directories.
base::FilePath& GlobalUserDataDir() {
  static base::NoDestructor<base::FilePath> dir_user_data;
  return *dir_user_data;
}

base::FilePath& GlobalChromeOSWallpapersDir() {
  static base::NoDestructor<base::FilePath> dir_chrome_os_wallpapers;
  return *dir_chrome_os_wallpapers;
}

base::FilePath& GlobalChromeOSCustomWallpapersDir() {
  static base::NoDestructor<base::FilePath> dir_chrome_os_custom_wallpapers;
  return *dir_chrome_os_custom_wallpapers;
}

// The file path of the device policy wallpaper (if any).
base::FilePath& GlobalDevicePolicyWallpaperFile() {
  static base::NoDestructor<base::FilePath> device_policy_wallpaper_file;
  return *device_policy_wallpaper_file;
}

void SetGlobalUserDataDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalUserDataDir();
  global_path = path;
}

void SetGlobalChromeOSWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSWallpapersDir();
  global_path = path;
}

void SetGlobalChromeOSCustomWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSCustomWallpapersDir();
  global_path = path;
}

void SetGlobalDevicePolicyWallpaperFile(const base::FilePath& path) {
  base::FilePath& global_path = GlobalDevicePolicyWallpaperFile();
  global_path = path;
}

// Returns the appropriate wallpaper resolution for all root windows.
WallpaperController::WallpaperResolution GetAppropriateResolution() {
  gfx::Size size = WallpaperController::GetMaxDisplaySizeInNative();
  return (size.width() > kSmallWallpaperMaxWidth ||
          size.height() > kSmallWallpaperMaxHeight)
             ? WallpaperController::WALLPAPER_RESOLUTION_LARGE
             : WallpaperController::WALLPAPER_RESOLUTION_SMALL;
}

// Returns the path of the online wallpaper corresponding to |url| and
// |resolution|.
base::FilePath GetOnlineWallpaperPath(
    const std::string& url,
    WallpaperController::WallpaperResolution resolution) {
  std::string file_name = GURL(url).ExtractFileName();
  if (resolution == WallpaperController::WALLPAPER_RESOLUTION_SMALL) {
    file_name = base::FilePath(file_name)
                    .InsertBeforeExtension(kSmallWallpaperSuffix)
                    .value();
  }
  DCHECK(!GlobalChromeOSWallpapersDir().empty());
  return GlobalChromeOSWallpapersDir().Append(file_name);
}

// Returns wallpaper subdirectory name for current resolution.
std::string GetCustomWallpaperSubdirForCurrentResolution() {
  WallpaperController::WallpaperResolution resolution =
      GetAppropriateResolution();
  return resolution == WallpaperController::WALLPAPER_RESOLUTION_SMALL
             ? WallpaperController::kSmallWallpaperSubDir
             : WallpaperController::kLargeWallpaperSubDir;
}

// Resizes |image| to a resolution which is nearest to |preferred_width| and
// |preferred_height| while respecting the |layout| choice. Encodes the image to
// JPEG and saves to |output|. Returns true on success.
bool ResizeAndEncodeImage(const gfx::ImageSkia& image,
                          WallpaperLayout layout,
                          int preferred_width,
                          int preferred_height,
                          scoped_refptr<base::RefCountedBytes>* output) {
  int width = image.width();
  int height = image.height();
  int resized_width;
  int resized_height;
  *output = base::MakeRefCounted<base::RefCountedBytes>();

  if (layout == WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // Do not resize wallpaper if it is smaller than preferred size.
    if (width < preferred_width || height < preferred_height)
      return false;

    double horizontal_ratio = static_cast<double>(preferred_width) / width;
    double vertical_ratio = static_cast<double>(preferred_height) / height;
    if (vertical_ratio > horizontal_ratio) {
      resized_width =
          gfx::ToRoundedInt(static_cast<double>(width) * vertical_ratio);
      resized_height = preferred_height;
    } else {
      resized_width = preferred_width;
      resized_height =
          gfx::ToRoundedInt(static_cast<double>(height) * horizontal_ratio);
    }
  } else if (layout == WALLPAPER_LAYOUT_STRETCH) {
    resized_width = preferred_width;
    resized_height = preferred_height;
  } else {
    resized_width = width;
    resized_height = height;
  }

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));

  SkBitmap bitmap = *(resized_image.bitmap());
  gfx::JPEGCodec::Encode(bitmap, kDefaultEncodingQuality, &(*output)->data());
  return true;
}

// Resizes |image| to a resolution which is nearest to |preferred_width| and
// |preferred_height| while respecting the |layout| choice and saves the
// resized wallpaper to |path|. Returns true on success.
bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                            const base::FilePath& path,
                            WallpaperLayout layout,
                            int preferred_width,
                            int preferred_height) {
  if (layout == WALLPAPER_LAYOUT_CENTER) {
    if (base::PathExists(path))
      base::DeleteFile(path, false);
    return false;
  }
  scoped_refptr<base::RefCountedBytes> data;
  if (!ResizeAndEncodeImage(image, layout, preferred_width, preferred_height,
                            &data)) {
    return false;
  }

  // Saves |data| to |path| in local file system.
  size_t written_bytes =
      base::WriteFile(path, data->front_as<const char>(), data->size());
  return written_bytes == data->size();
}

// Creates a 1x1 solid color image.
gfx::ImageSkia CreateSolidColorWallpaper(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Returns true if a color should be extracted from the wallpaper based on the
// command kAshShelfColor line arg.
bool IsShelfColoringEnabled() {
  const bool kDefaultValue = true;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshShelfColor)) {
    return kDefaultValue;
  }

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshShelfColor);
  if (switch_value != switches::kAshShelfColorEnabled &&
      switch_value != switches::kAshShelfColorDisabled) {
    LOG(WARNING) << "Invalid '--" << switches::kAshShelfColor << "' value of '"
                 << switch_value << "'. Defaulting to "
                 << (kDefaultValue ? "enabled." : "disabled.");
    return kDefaultValue;
  }

  return switch_value == switches::kAshShelfColorEnabled;
}

// Gets the color profiles for extracting wallpaper prominent colors.
std::vector<ColorProfile> GetProminentColorProfiles() {
  return {ColorProfile(LumaRange::DARK, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::NORMAL, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::LIGHT, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::DARK, SaturationRange::MUTED),
          ColorProfile(LumaRange::NORMAL, SaturationRange::MUTED),
          ColorProfile(LumaRange::LIGHT, SaturationRange::MUTED)};
}

// Gets the corresponding color profile type based on the given
// |color_profile|.
ColorProfileType GetColorProfileType(ColorProfile color_profile) {
  bool vibrant = color_profile.saturation == SaturationRange::VIBRANT;
  switch (color_profile.luma) {
    case LumaRange::DARK:
      return vibrant ? ColorProfileType::DARK_VIBRANT
                     : ColorProfileType::DARK_MUTED;
    case LumaRange::NORMAL:
      return vibrant ? ColorProfileType::NORMAL_VIBRANT
                     : ColorProfileType::NORMAL_MUTED;
    case LumaRange::LIGHT:
      return vibrant ? ColorProfileType::LIGHT_VIBRANT
                     : ColorProfileType::LIGHT_MUTED;
  }
  NOTREACHED();
  return ColorProfileType::DARK_MUTED;
}

// If |read_is_successful| is true, start decoding the image, which will run
// |callback| upon completion; if it's false, run |callback| directly with an
// empty image.
void OnWallpaperDataRead(LoadedCallback callback,
                         std::unique_ptr<std::string> data,
                         bool read_is_successful) {
  if (!read_is_successful) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::Passed(gfx::ImageSkia())));
    return;
  }
  // This image was once encoded to JPEG by |ResizeAndEncodeImage|.
  DecodeWallpaper(*data, data_decoder::mojom::ImageCodec::ROBUST_JPEG,
                  std::move(callback));
}

// Deletes a list of wallpaper files in |file_list|.
void DeleteWallpaperInList(std::vector<base::FilePath> file_list) {
  for (const base::FilePath& path : file_list) {
    if (!base::DeleteFile(path, true))
      LOG(ERROR) << "Failed to remove user wallpaper at " << path.value();
  }
}

// Creates all new custom wallpaper directories for |wallpaper_files_id| if they
// don't exist.
void EnsureCustomWallpaperDirectories(const std::string& wallpaper_files_id) {
  base::FilePath dir = WallpaperController::GetCustomWallpaperDir(
                           WallpaperController::kSmallWallpaperSubDir)
                           .Append(wallpaper_files_id);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);

  dir = WallpaperController::GetCustomWallpaperDir(
            WallpaperController::kLargeWallpaperSubDir)
            .Append(wallpaper_files_id);

  if (!base::PathExists(dir))
    base::CreateDirectory(dir);

  dir = WallpaperController::GetCustomWallpaperDir(
            WallpaperController::kOriginalWallpaperSubDir)
            .Append(wallpaper_files_id);
  if (!base::PathExists(dir))
    base::CreateDirectory(dir);
}

// Saves original custom wallpaper to |path| (absolute path) on filesystem
// and starts resizing operation of the custom wallpaper if necessary.
void SaveCustomWallpaper(const std::string& wallpaper_files_id,
                         const base::FilePath& original_path,
                         WallpaperLayout layout,
                         std::unique_ptr<gfx::ImageSkia> image) {
  base::DeleteFile(WallpaperController::GetCustomWallpaperDir(
                       WallpaperController::kOriginalWallpaperSubDir)
                       .Append(wallpaper_files_id),
                   true /* recursive */);
  base::DeleteFile(WallpaperController::GetCustomWallpaperDir(
                       WallpaperController::kSmallWallpaperSubDir)
                       .Append(wallpaper_files_id),
                   true /* recursive */);
  base::DeleteFile(WallpaperController::GetCustomWallpaperDir(
                       WallpaperController::kLargeWallpaperSubDir)
                       .Append(wallpaper_files_id),
                   true /* recursive */);
  EnsureCustomWallpaperDirectories(wallpaper_files_id);
  const std::string file_name = original_path.BaseName().value();
  const base::FilePath small_wallpaper_path =
      WallpaperController::GetCustomWallpaperPath(
          WallpaperController::kSmallWallpaperSubDir, wallpaper_files_id,
          file_name);
  const base::FilePath large_wallpaper_path =
      WallpaperController::GetCustomWallpaperPath(
          WallpaperController::kLargeWallpaperSubDir, wallpaper_files_id,
          file_name);

  // Re-encode orginal file to jpeg format and saves the result in case that
  // resized wallpaper is not generated (i.e. chrome shutdown before resized
  // wallpaper is saved).
  ResizeAndSaveWallpaper(*image, original_path, WALLPAPER_LAYOUT_STRETCH,
                         image->width(), image->height());
  ResizeAndSaveWallpaper(*image, small_wallpaper_path, layout,
                         kSmallWallpaperMaxWidth, kSmallWallpaperMaxHeight);
  ResizeAndSaveWallpaper(*image, large_wallpaper_path, layout,
                         kLargeWallpaperMaxWidth, kLargeWallpaperMaxHeight);
}

// Checks if kiosk app is running. Note: it returns false either when there's
// no active user (e.g. at login screen), or the active user is not kiosk.
bool IsInKioskMode() {
  base::Optional<user_manager::UserType> active_user_type =
      Shell::Get()->session_controller()->GetUserType();
  // |active_user_type| is empty when there's no active user.
  return active_user_type &&
         *active_user_type == user_manager::USER_TYPE_KIOSK_APP;
}

// Checks if |account_id| is the current active user.
bool IsActiveUser(const AccountId& account_id) {
  // The current active user has index 0.
  const mojom::UserSession* const active_user_session =
      Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
  return active_user_session &&
         active_user_session->user_info->account_id == account_id;
}

// Returns the file path of the wallpaper corresponding to |url| if it exists in
// local file system, otherwise returns an empty file path.
base::FilePath GetExistingOnlineWallpaperPath(const std::string& url) {
  WallpaperController::WallpaperResolution resolution =
      GetAppropriateResolution();
  base::FilePath wallpaper_path = GetOnlineWallpaperPath(url, resolution);
  if (base::PathExists(wallpaper_path))
    return wallpaper_path;

  // Falls back to the large wallpaper if the small one doesn't exist.
  if (resolution == WallpaperController::WALLPAPER_RESOLUTION_SMALL) {
    wallpaper_path = GetOnlineWallpaperPath(
        url, WallpaperController::WALLPAPER_RESOLUTION_LARGE);
    if (base::PathExists(wallpaper_path))
      return wallpaper_path;
  }
  return base::FilePath();
}

// Saves the online wallpaper with both large and small sizes to local file
// system.
void SaveOnlineWallpaper(const std::string& url,
                         WallpaperLayout layout,
                         std::unique_ptr<gfx::ImageSkia> image) {
  DCHECK(!GlobalChromeOSWallpapersDir().empty());
  if (!base::DirectoryExists(GlobalChromeOSWallpapersDir()) &&
      !base::CreateDirectory(GlobalChromeOSWallpapersDir())) {
    return;
  }
  ResizeAndSaveWallpaper(
      *image,
      GetOnlineWallpaperPath(url,
                             WallpaperController::WALLPAPER_RESOLUTION_LARGE),
      layout, image->width(), image->height());
  ResizeAndSaveWallpaper(
      *image,
      GetOnlineWallpaperPath(url,
                             WallpaperController::WALLPAPER_RESOLUTION_SMALL),
      WALLPAPER_LAYOUT_CENTER_CROPPED, kSmallWallpaperMaxWidth,
      kSmallWallpaperMaxHeight);
}

// Implementation of |WallpaperController::GetOfflineWallpaper|.
std::vector<std::string> GetOfflineWallpaperListImpl() {
  DCHECK(!GlobalChromeOSWallpapersDir().empty());
  std::vector<std::string> url_list;
  if (base::DirectoryExists(GlobalChromeOSWallpapersDir())) {
    base::FileEnumerator files(GlobalChromeOSWallpapersDir(),
                               /*recursive=*/false,
                               base::FileEnumerator::FILES);
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      // Do not add file name of small resolution wallpaper to the list.
      if (!base::EndsWith(current.BaseName().RemoveExtension().value(),
                          kSmallWallpaperSuffix,
                          base::CompareCase::SENSITIVE)) {
        url_list.push_back(current.BaseName().value());
      }
    }
  }
  return url_list;
}

}  // namespace

const char WallpaperController::kSmallWallpaperSubDir[] = "small";
const char WallpaperController::kLargeWallpaperSubDir[] = "large";
const char WallpaperController::kOriginalWallpaperSubDir[] = "original";

WallpaperController::WallpaperController()
    : locked_(false),
      wallpaper_mode_(WALLPAPER_NONE),
      color_profiles_(GetProminentColorProfiles()),
      wallpaper_reload_delay_(kWallpaperReloadDelay),
      sequenced_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      scoped_session_observer_(this),
      weak_factory_(this) {
  DCHECK(!color_profiles_.empty());
  prominent_colors_ =
      std::vector<SkColor>(color_profiles_.size(), kInvalidWallpaperColor);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
}

WallpaperController::~WallpaperController() {
  if (current_wallpaper_)
    current_wallpaper_->RemoveObserver(this);
  if (color_calculator_)
    color_calculator_->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  weak_factory_.InvalidateWeakPtrs();
}

// static
void WallpaperController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUserWallpaperInfo);
  registry->RegisterDictionaryPref(prefs::kWallpaperColors);
}

// static
gfx::Size WallpaperController::GetMaxDisplaySizeInNative() {
  // Return an empty size for test environments where the screen is null.
  if (!display::Screen::GetScreen())
    return gfx::Size();

  gfx::Size max;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    // Use the native size, not ManagedDisplayInfo::size_in_pixel or
    // Display::size.
    display::ManagedDisplayInfo info =
        Shell::Get()->display_manager()->GetDisplayInfo(display.id());
    DCHECK_EQ(display.id(), info.id());
    gfx::Size size = info.bounds_in_native().size();
    if (display.rotation() == display::Display::ROTATE_90 ||
        display.rotation() == display::Display::ROTATE_270) {
      size = gfx::Size(size.height(), size.width());
    }
    max.SetToMax(size);
  }

  return max;
}

// static
base::FilePath WallpaperController::GetCustomWallpaperPath(
    const std::string& sub_dir,
    const std::string& wallpaper_files_id,
    const std::string& file_name) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(wallpaper_files_id).Append(file_name);
}

// static
base::FilePath WallpaperController::GetCustomWallpaperDir(
    const std::string& sub_dir) {
  DCHECK(!GlobalChromeOSCustomWallpapersDir().empty());
  return GlobalChromeOSCustomWallpapersDir().Append(sub_dir);
}

// static
void WallpaperController::SetWallpaperFromPath(
    const AccountId& account_id,
    const user_manager::UserType& user_type,
    const WallpaperInfo& info,
    const base::FilePath& wallpaper_path,
    bool show_wallpaper,
    const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
    base::WeakPtr<WallpaperController> weak_ptr) {
  base::FilePath valid_path = wallpaper_path;
  if (!base::PathExists(valid_path)) {
    // Falls back to the original file if the file with correct resolution does
    // not exist. This may happen when the original custom wallpaper is small or
    // browser shutdown before resized wallpaper saved.
    valid_path =
        GetCustomWallpaperDir(kOriginalWallpaperSubDir).Append(info.location);
  }

  if (!base::PathExists(valid_path)) {
    LOG(ERROR) << "The path " << valid_path.value()
               << " doesn't exist. Falls back to default wallpaper.";
    reply_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&WallpaperController::SetDefaultWallpaperImpl, weak_ptr,
                       account_id, user_type, show_wallpaper));
  } else {
    reply_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&WallpaperController::StartDecodeFromPath,
                                  weak_ptr, account_id, user_type, valid_path,
                                  info, show_wallpaper));
  }
}

void WallpaperController::BindRequest(
    mojom::WallpaperControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void WallpaperController::AddObserver(WallpaperControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperController::RemoveObserver(
    WallpaperControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

SkColor WallpaperController::GetProminentColor(
    ColorProfile color_profile) const {
  ColorProfileType type = GetColorProfileType(color_profile);
  return prominent_colors_[static_cast<int>(type)];
}

gfx::ImageSkia WallpaperController::GetWallpaper() const {
  return current_wallpaper_ ? current_wallpaper_->image() : gfx::ImageSkia();
}

WallpaperLayout WallpaperController::GetWallpaperLayout() const {
  return current_wallpaper_ ? current_wallpaper_->wallpaper_info().layout
                            : NUM_WALLPAPER_LAYOUT;
}

WallpaperType WallpaperController::GetWallpaperType() const {
  return current_wallpaper_ ? current_wallpaper_->wallpaper_info().type
                            : WALLPAPER_TYPE_COUNT;
}

bool WallpaperController::ShouldShowInitialAnimation() {
  // The slower initial animation is only applicable if:
  // 1) It's the first run after system boot, not after user sign-out.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFirstExecAfterBoot)) {
    return false;
  }
  // 2) It's at the login screen.
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kLoginManager)) {
    return false;
  }
  // 3) It's the first wallpaper being shown, not for the switching between
  //    multiple user pods.
  if (!is_first_wallpaper_)
    return false;

  return true;
}

void WallpaperController::OnWallpaperAnimationFinished() {
  // TODO(wzang|784495): This is used by a code path in web-UI login. Remove it
  // if views-based login is not interested in this event.
  if (wallpaper_controller_client_ && is_first_wallpaper_) {
    wallpaper_controller_client_->OnFirstWallpaperAnimationFinished();
  }
}

bool WallpaperController::CanOpenWallpaperPicker() {
  return ShouldShowWallpaperSettingImpl() &&
         !IsActiveUserWallpaperControlledByPolicyImpl();
}

bool WallpaperController::HasShownAnyWallpaper() const {
  return !!current_wallpaper_;
}

void WallpaperController::ShowWallpaperImage(const gfx::ImageSkia& image,
                                             WallpaperInfo info,
                                             bool preview_mode) {
  // Ignore show wallpaper requests during preview mode. This could happen if a
  // custom wallpaper previously set on another device is being synced.
  if (confirm_preview_wallpaper_callback_ && !preview_mode)
    return;

  if (preview_mode) {
    for (auto& observer : observers_)
      observer.OnWallpaperPreviewStarted();
  }

  // 1x1 wallpaper should be stretched to fill the entire screen.
  // (WALLPAPER_LAYOUT_TILE also serves this purpose.)
  if (image.width() == 1 && image.height() == 1)
    info.layout = WALLPAPER_LAYOUT_STRETCH;

  VLOG(1) << "SetWallpaper: image_id=" << WallpaperResizer::GetImageId(image)
          << " layout=" << info.layout;

  if (WallpaperIsAlreadyLoaded(image, /*compare_layouts=*/true, info.layout)) {
    VLOG(1) << "Wallpaper is already loaded";
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", info.type,
                            WALLPAPER_TYPE_COUNT);

  // Cancel any in-flight color calculation because we have a new wallpaper.
  if (color_calculator_) {
    color_calculator_->RemoveObserver(this);
    color_calculator_.reset();
  }

  is_first_wallpaper_ = !current_wallpaper_;
  current_wallpaper_ = std::make_unique<WallpaperResizer>(
      image, GetMaxDisplaySizeInNative(), info, sequenced_task_runner_);
  current_wallpaper_->AddObserver(this);
  current_wallpaper_->StartResize();

  if (is_first_wallpaper_) {
    for (auto& observer : observers_)
      observer.OnFirstWallpaperShown();
  }
  mojo_observers_.ForAllPtrs([this](mojom::WallpaperObserver* observer) {
    observer->OnWallpaperChanged(current_wallpaper_->original_image_id());
  });

  wallpaper_mode_ = WALLPAPER_IMAGE;
  InstallDesktopControllerForAllWindows();
  ++wallpaper_count_for_testing_;
}

bool WallpaperController::IsPolicyControlled(const AccountId& account_id,
                                             bool is_ephemeral) const {
  WallpaperInfo info;
  return GetUserWallpaperInfo(account_id, &info, is_ephemeral) &&
         info.type == POLICY;
}

void WallpaperController::UpdateWallpaperBlur(bool blur) {
  bool needs_blur = blur && IsBlurAllowed();
  if (needs_blur == is_wallpaper_blurred_)
    return;

  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    root_window_controller->wallpaper_widget_controller()->SetWallpaperBlur(
        needs_blur ? login_constants::kBlurSigma
                   : login_constants::kClearBlurSigma);
  }
  is_wallpaper_blurred_ = needs_blur;
  for (auto& observer : observers_)
    observer.OnWallpaperBlurChanged();
  mojo_observers_.ForAllPtrs([this](mojom::WallpaperObserver* observer) {
    observer->OnWallpaperBlurChanged(is_wallpaper_blurred_);
  });
}

bool WallpaperController::ShouldApplyDimming() const {
  return Shell::Get()->session_controller()->IsUserSessionBlocked() &&
         !IsOneShotWallpaper() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshDisableLoginDimAndBlur);
}

bool WallpaperController::IsBlurAllowed() const {
  return !IsDevicePolicyWallpaper() && !IsOneShotWallpaper() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshDisableLoginDimAndBlur);
}

bool WallpaperController::IsWallpaperBlurred() const {
  return is_wallpaper_blurred_;
}

bool WallpaperController::SetUserWallpaperInfo(const AccountId& account_id,
                                               const WallpaperInfo& info,
                                               bool is_ephemeral) {
  if (is_ephemeral) {
    ephemeral_users_wallpaper_info_[account_id] = info;
    return true;
  }

  if (!local_state_)
    return false;

  WallpaperInfo old_info;
  if (GetUserWallpaperInfo(account_id, &old_info, is_ephemeral)) {
    // Remove the color cache of the previous wallpaper if it exists.
    DictionaryPrefUpdate wallpaper_colors_update(local_state_,
                                                 prefs::kWallpaperColors);
    wallpaper_colors_update->RemoveWithoutPathExpansion(old_info.location,
                                                        nullptr);
  }

  DictionaryPrefUpdate wallpaper_update(local_state_,
                                        prefs::kUserWallpaperInfo);
  auto wallpaper_info_dict = std::make_unique<base::DictionaryValue>();
  wallpaper_info_dict->SetString(
      kNewWallpaperDateNodeName,
      base::Int64ToString(info.date.ToInternalValue()));
  wallpaper_info_dict->SetString(kNewWallpaperLocationNodeName, info.location);
  wallpaper_info_dict->SetInteger(kNewWallpaperLayoutNodeName, info.layout);
  wallpaper_info_dict->SetInteger(kNewWallpaperTypeNodeName, info.type);
  wallpaper_update->SetWithoutPathExpansion(account_id.GetUserEmail(),
                                            std::move(wallpaper_info_dict));
  return true;
}

bool WallpaperController::GetUserWallpaperInfo(const AccountId& account_id,
                                               WallpaperInfo* info,
                                               bool is_ephemeral) const {
  if (is_ephemeral) {
    // Ephemeral users do not save anything to local state. Return true if the
    // info can be found in the map, otherwise return false.
    auto it = ephemeral_users_wallpaper_info_.find(account_id);
    if (it == ephemeral_users_wallpaper_info_.end())
      return false;

    *info = it->second;
    return true;
  }

  if (!local_state_)
    return false;
  const base::DictionaryValue* info_dict;
  if (!local_state_->GetDictionary(prefs::kUserWallpaperInfo)
           ->GetDictionaryWithoutPathExpansion(account_id.GetUserEmail(),
                                               &info_dict)) {
    return false;
  }

  // Use temporary variables to keep |info| untouched in the error case.
  std::string location;
  if (!info_dict->GetString(kNewWallpaperLocationNodeName, &location))
    return false;
  int layout;
  if (!info_dict->GetInteger(kNewWallpaperLayoutNodeName, &layout))
    return false;
  int type;
  if (!info_dict->GetInteger(kNewWallpaperTypeNodeName, &type))
    return false;
  std::string date_string;
  if (!info_dict->GetString(kNewWallpaperDateNodeName, &date_string))
    return false;
  int64_t date_val;
  if (!base::StringToInt64(date_string, &date_val))
    return false;

  info->location = location;
  info->layout = static_cast<WallpaperLayout>(layout);
  info->type = static_cast<WallpaperType>(type);
  info->date = base::Time::FromInternalValue(date_val);
  return true;
}

bool WallpaperController::GetWallpaperFromCache(const AccountId& account_id,
                                                gfx::ImageSkia* image) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end() && !it->second.second.isNull()) {
    *image = it->second.second;
    return true;
  }
  return false;
}

bool WallpaperController::GetPathFromCache(const AccountId& account_id,
                                           base::FilePath* path) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end()) {
    *path = it->second.first;
    return true;
  }
  return false;
}

void WallpaperController::AddFirstWallpaperAnimationEndCallback(
    base::OnceClosure callback,
    aura::Window* window) {
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(window)->wallpaper_widget_controller();
  if (!current_wallpaper_ ||
      (is_first_wallpaper_ && wallpaper_widget_controller->IsAnimating())) {
    // No wallpaper has been set, or the first wallpaper is still animating.
    wallpaper_widget_controller->AddAnimationEndCallback(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void WallpaperController::StartDecodeFromPath(
    const AccountId& account_id,
    const user_manager::UserType& user_type,
    const base::FilePath& wallpaper_path,
    const WallpaperInfo& info,
    bool show_wallpaper) {
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperController::OnWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), account_id, user_type,
                     wallpaper_path, info, show_wallpaper),
      sequenced_task_runner_, wallpaper_path);
}

void WallpaperController::Init(
    mojom::WallpaperControllerClientPtr client,
    const base::FilePath& user_data_path,
    const base::FilePath& chromeos_wallpapers_path,
    const base::FilePath& chromeos_custom_wallpapers_path,
    const base::FilePath& device_policy_wallpaper_path,
    bool is_device_wallpaper_policy_enforced) {
  DCHECK(!wallpaper_controller_client_.get());
  wallpaper_controller_client_ = std::move(client);
  SetGlobalUserDataDir(user_data_path);
  SetGlobalChromeOSWallpapersDir(chromeos_wallpapers_path);
  SetGlobalChromeOSCustomWallpapersDir(chromeos_custom_wallpapers_path);
  SetGlobalDevicePolicyWallpaperFile(device_policy_wallpaper_path);
  is_device_wallpaper_policy_enforced_ = is_device_wallpaper_policy_enforced;
}

void WallpaperController::SetCustomWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    WallpaperLayout layout,
    const gfx::ImageSkia& image,
    bool preview_mode) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  if (!CanSetUserWallpaper(user_info->account_id, user_info->is_ephemeral))
    return;

  const bool is_active_user = IsActiveUser(user_info->account_id);
  if (preview_mode) {
    DCHECK(is_active_user);
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperController::SaveAndSetWallpaper, weak_factory_.GetWeakPtr(),
        std::move(user_info), wallpaper_files_id, file_name, CUSTOMIZED, layout,
        /*show_wallpaper=*/false, image);
    reload_preview_wallpaper_callback_ =
        base::BindRepeating(&WallpaperController::ShowWallpaperImage,
                            weak_factory_.GetWeakPtr(), image,
                            WallpaperInfo{std::string(), layout, CUSTOMIZED,
                                          base::Time::Now().LocalMidnight()},
                            /*preview_mode=*/true);
    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SaveAndSetWallpaper(std::move(user_info), wallpaper_files_id, file_name,
                        CUSTOMIZED, layout, /*show_wallpaper=*/is_active_user,
                        image);
  }
}

void WallpaperController::SetOnlineWallpaperIfExists(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& url,
    WallpaperLayout layout,
    bool preview_mode,
    SetOnlineWallpaperIfExistsCallback callback) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  DCHECK(CanSetUserWallpaper(user_info->account_id, user_info->is_ephemeral));

  const OnlineWallpaperParams params = {user_info->account_id,
                                        user_info->is_ephemeral, url, layout,
                                        preview_mode};
  base::PostTaskAndReplyWithResult(
      sequenced_task_runner_.get(), FROM_HERE,
      base::BindOnce(&GetExistingOnlineWallpaperPath, url),
      base::BindOnce(&WallpaperController::SetOnlineWallpaperFromPath,
                     weak_factory_.GetWeakPtr(), std::move(callback), params));
}

void WallpaperController::SetOnlineWallpaperFromData(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& image_data,
    const std::string& url,
    WallpaperLayout layout,
    bool preview_mode,
    SetOnlineWallpaperFromDataCallback callback) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      !CanSetUserWallpaper(user_info->account_id, user_info->is_ephemeral)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  const OnlineWallpaperParams params = {user_info->account_id,
                                        user_info->is_ephemeral, url, layout,
                                        preview_mode};
  LoadedCallback decoded_callback =
      base::BindOnce(&WallpaperController::OnOnlineWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), params, /*save_file=*/true,
                     std::move(callback));
  if (bypass_decode_for_testing_) {
    std::move(decoded_callback)
        .Run(CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }
  // Use default codec because 1) online wallpapers may have various formats,
  // 2) the image data comes from the Chrome OS wallpaper picker and is
  // trusted (third-party wallpaper apps use |SetThirdPartyWallpaper|), 3) the
  // code path is never used on login screen (enforced by the check above).
  DecodeWallpaper(image_data, data_decoder::mojom::ImageCodec::DEFAULT,
                  std::move(decoded_callback));
}

void WallpaperController::SetDefaultWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    bool show_wallpaper) {
  if (!CanSetUserWallpaper(user_info->account_id, user_info->is_ephemeral))
    return;

  const AccountId account_id = user_info->account_id;
  const bool is_ephemeral = user_info->is_ephemeral;
  const user_manager::UserType type = user_info->type;

  RemoveUserWallpaper(std::move(user_info), wallpaper_files_id);
  if (!InitializeUserWallpaperInfo(account_id, is_ephemeral)) {
    LOG(ERROR) << "Initializing user wallpaper info fails. This should never "
                  "happen except in tests.";
  }
  if (show_wallpaper)
    SetDefaultWallpaperImpl(account_id, type, /*show_wallpaper=*/true);
}

void WallpaperController::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  customized_default_small_path_ = customized_default_small_path;
  customized_default_large_path_ = customized_default_large_path;

  // If the current wallpaper has type DEFAULT, the new customized default
  // wallpaper should be shown immediately to update the screen. It shouldn't
  // replace wallpapers of other types.
  bool show_wallpaper = (GetWallpaperType() == DEFAULT);

  // Customized default wallpapers are subject to the same restrictions as other
  // default wallpapers, e.g. they should not be set during guest sessions.
  SetDefaultWallpaperImpl(EmptyAccountId(), user_manager::USER_TYPE_REGULAR,
                          show_wallpaper);
}

void WallpaperController::SetPolicyWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& data) {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode())
    return;

  // Updates the screen only when the user has logged in.
  const bool show_wallpaper =
      Shell::Get()->session_controller()->IsActiveUserSessionStarted();
  LoadedCallback callback = base::BindOnce(
      &WallpaperController::SaveAndSetWallpaper, weak_factory_.GetWeakPtr(),
      base::Passed(&user_info), wallpaper_files_id, kPolicyWallpaperFile,
      POLICY, WALLPAPER_LAYOUT_CENTER_CROPPED, show_wallpaper);

  if (bypass_decode_for_testing_) {
    std::move(callback).Run(CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }
  // The default codec cannot be used here because the image data is provided by
  // user and thus not trusted. In addition, only JPEG |data| is accepted.
  DecodeWallpaper(data, data_decoder::mojom::ImageCodec::ROBUST_JPEG,
                  std::move(callback));
}

void WallpaperController::SetDeviceWallpaperPolicyEnforced(bool enforced) {
  bool previous_enforced = is_device_wallpaper_policy_enforced_;
  is_device_wallpaper_policy_enforced_ = enforced;

  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
  } else if ((previous_enforced != enforced) && !enforced) {
    // If the device wallpaper policy is cleared, the wallpaper should revert to
    // the wallpaper of the current user with the large pod in the users list in
    // the login screen. If there is no such user, use the first user in the
    // users list.
    // TODO(xdai): Get the account id from the session controller and then call
    // ShowUserWallpaper() to display it.
  }
}

void WallpaperController::SetThirdPartyWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    WallpaperLayout layout,
    const gfx::ImageSkia& image,
    SetThirdPartyWallpaperCallback callback) {
  const uint32_t image_id = WallpaperResizer::GetImageId(image);
  bool allowed_to_set_wallpaper =
      CanSetUserWallpaper(user_info->account_id, user_info->is_ephemeral);
  bool allowed_to_show_wallpaper = IsActiveUser(user_info->account_id);
  std::move(callback).Run(allowed_to_set_wallpaper && allowed_to_show_wallpaper,
                          image_id);

  if (allowed_to_set_wallpaper) {
    SaveAndSetWallpaper(std::move(user_info), wallpaper_files_id, file_name,
                        CUSTOMIZED, layout, allowed_to_show_wallpaper, image);
  }
}

void WallpaperController::ConfirmPreviewWallpaper() {
  if (!confirm_preview_wallpaper_callback_) {
    DCHECK(!reload_preview_wallpaper_callback_);
    return;
  }
  std::move(confirm_preview_wallpaper_callback_).Run();
  reload_preview_wallpaper_callback_.Reset();
  for (auto& observer : observers_)
    observer.OnWallpaperPreviewEnded();
}

void WallpaperController::CancelPreviewWallpaper() {
  confirm_preview_wallpaper_callback_.Reset();
  reload_preview_wallpaper_callback_.Reset();
  ReloadWallpaper(/*clear_cache=*/false);
  for (auto& observer : observers_)
    observer.OnWallpaperPreviewEnded();
}

void WallpaperController::UpdateCustomWallpaperLayout(
    mojom::WallpaperUserInfoPtr user_info,
    WallpaperLayout layout) {
  // This method has a very specific use case: the user should be active and
  // have a custom wallpaper.
  // The currently active user has index 0.
  const mojom::UserSession* const active_user_session =
      Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
  if (!active_user_session ||
      active_user_session->user_info->account_id != user_info->account_id) {
    return;
  }
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(user_info->account_id, &info,
                            user_info->is_ephemeral) ||
      info.type != CUSTOMIZED) {
    return;
  }
  if (info.layout == layout)
    return;

  info.layout = layout;
  if (!SetUserWallpaperInfo(user_info->account_id, info,
                            user_info->is_ephemeral)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }
  ShowUserWallpaper(std::move(user_info));
}

void WallpaperController::ShowUserWallpaper(
    mojom::WallpaperUserInfoPtr user_info) {
  current_user_ = std::move(user_info);
  const user_manager::UserType user_type = current_user_->type;

  if (user_type == user_manager::USER_TYPE_KIOSK_APP ||
      user_type == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    return;
  }

  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
    return;
  }

  const AccountId account_id = current_user_->account_id;
  const bool is_ephemeral = current_user_->is_ephemeral;
  // Guest user or regular user in ephemeral mode.
  if ((is_ephemeral && current_user_->has_gaia_account) ||
      current_user_->type == user_manager::USER_TYPE_GUEST) {
    if (!InitializeUserWallpaperInfo(account_id, is_ephemeral))
      return;
    SetDefaultWallpaperImpl(account_id, current_user_->type,
                            /*show_wallpaper=*/true);
    VLOG(1) << "User is ephemeral. Fallback to default wallpaper.";
    return;
  }

  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info, is_ephemeral)) {
    if (!InitializeUserWallpaperInfo(account_id, is_ephemeral))
      return;
    GetUserWallpaperInfo(account_id, &info, is_ephemeral);
  }

  gfx::ImageSkia user_wallpaper;
  if (GetWallpaperFromCache(account_id, &user_wallpaper)) {
    ShowWallpaperImage(user_wallpaper, info, /*preview_mode=*/false);
    return;
  }

  if (info.location.empty()) {
    // Uses default wallpaper when file is empty.
    SetDefaultWallpaperImpl(account_id, current_user_->type,
                            /*show_wallpaper=*/true);
    return;
  }

  if (info.type != CUSTOMIZED && info.type != POLICY && info.type != DEVICE) {
    // Load wallpaper according to WallpaperInfo.
    SetWallpaperFromInfo(account_id, current_user_->type, info,
                         /*show_wallpaper=*/true);
    return;
  }

  base::FilePath wallpaper_path;
  if (info.type == DEVICE) {
    wallpaper_path = GlobalDevicePolicyWallpaperFile();
  } else {
    std::string sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
    // Wallpaper is not resized when layout is
    // WALLPAPER_LAYOUT_CENTER.
    // Original wallpaper should be used in this case.
    if (info.layout == WALLPAPER_LAYOUT_CENTER)
      sub_dir = kOriginalWallpaperSubDir;
    wallpaper_path = GetCustomWallpaperDir(sub_dir).Append(info.location);
  }

  CustomWallpaperMap::iterator it = wallpaper_cache_map_.find(account_id);
  // Do not try to load the wallpaper if the path is the same, since loading
  // could still be in progress. We ignore the existence of the image.
  if (it != wallpaper_cache_map_.end() && it->second.first == wallpaper_path)
    return;

  // Set the new path and reset the existing image - the image will be
  // added once it becomes available.
  wallpaper_cache_map_[account_id] =
      CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SetWallpaperFromPath, account_id, current_user_->type,
                     info, wallpaper_path, /*show_wallpaper=*/true,
                     base::ThreadTaskRunnerHandle::Get(),
                     weak_factory_.GetWeakPtr()));
}

void WallpaperController::ShowSigninWallpaper() {
  current_user_.reset();
  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
  } else {
    SetDefaultWallpaperImpl(EmptyAccountId(), user_manager::USER_TYPE_REGULAR,
                            /*show_wallpaper=*/true);
  }
}

void WallpaperController::ShowOneShotWallpaper(const gfx::ImageSkia& image) {
  const WallpaperInfo info = {
      std::string(), WallpaperLayout::WALLPAPER_LAYOUT_STRETCH,
      WallpaperType::ONE_SHOT, base::Time::Now().LocalMidnight()};
  ShowWallpaperImage(image, info, /*preview_mode=*/false);
}

void WallpaperController::RemoveUserWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id) {
  RemoveUserWallpaperInfo(user_info->account_id, user_info->is_ephemeral);
  RemoveUserWallpaperImpl(user_info->account_id, wallpaper_files_id);
}

void WallpaperController::RemovePolicyWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id) {
  if (!IsPolicyControlled(user_info->account_id, user_info->is_ephemeral))
    return;

  // Updates the screen only when the user has logged in.
  const bool show_wallpaper =
      Shell::Get()->session_controller()->IsActiveUserSessionStarted();
  // Removes the wallpaper info so that the user is no longer policy controlled,
  // otherwise setting default wallpaper is not allowed.
  RemoveUserWallpaperInfo(user_info->account_id, user_info->is_ephemeral);
  SetDefaultWallpaper(std::move(user_info), wallpaper_files_id, show_wallpaper);
}

void WallpaperController::GetOfflineWallpaperList(
    GetOfflineWallpaperListCallback callback) {
  base::PostTaskAndReplyWithResult(sequenced_task_runner_.get(), FROM_HERE,
                                   base::BindOnce(&GetOfflineWallpaperListImpl),
                                   std::move(callback));
}

void WallpaperController::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  animation_duration_ = animation_duration;
}

void WallpaperController::OpenWallpaperPickerIfAllowed() {
  if (wallpaper_controller_client_ && CanOpenWallpaperPicker()) {
    wallpaper_controller_client_->OpenWallpaperPicker();
  }
}

void WallpaperController::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  if (!window_state_manager_)
    window_state_manager_ = std::make_unique<WallpaperWindowStateManager>();

  window_state_manager_->MinimizeInactiveWindows(user_id_hash);
}

void WallpaperController::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  if (!window_state_manager_) {
    NOTREACHED() << "This should only be called after calling "
                 << "MinimizeInactiveWindows.";
    return;
  }
  window_state_manager_->RestoreMinimizedWindows(user_id_hash);
}

void WallpaperController::AddObserver(
    mojom::WallpaperObserverAssociatedPtrInfo observer) {
  mojom::WallpaperObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  observer_ptr->OnWallpaperColorsChanged(prominent_colors_);
  mojo_observers_.AddPtr(std::move(observer_ptr));
}

void WallpaperController::GetWallpaperImage(
    GetWallpaperImageCallback callback) {
  std::move(callback).Run(GetWallpaper());
}

void WallpaperController::GetWallpaperColors(
    GetWallpaperColorsCallback callback) {
  std::move(callback).Run(prominent_colors_);
}

void WallpaperController::IsWallpaperBlurred(
    IsWallpaperBlurredCallback callback) {
  std::move(callback).Run(is_wallpaper_blurred_);
}

void WallpaperController::IsActiveUserWallpaperControlledByPolicy(
    IsActiveUserWallpaperControlledByPolicyCallback callback) {
  std::move(callback).Run(IsActiveUserWallpaperControlledByPolicyImpl());
}

void WallpaperController::GetActiveUserWallpaperInfo(
    GetActiveUserWallpaperInfoCallback callback) {
  WallpaperInfo info;
  if (!GetActiveUserWallpaperInfoImpl(&info)) {
    std::move(callback).Run(std::string(), ash::NUM_WALLPAPER_LAYOUT);
    return;
  }
  std::move(callback).Run(info.location, info.layout);
}

void WallpaperController::ShouldShowWallpaperSetting(
    ShouldShowWallpaperSettingCallback callback) {
  std::move(callback).Run(ShouldShowWallpaperSettingImpl());
}

void WallpaperController::OnDisplayConfigurationChanged() {
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ == max_display_size)
    return;

  current_max_display_size_ = max_display_size;
  if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_) {
    timer_.Stop();
    GetInternalDisplayCompositorLock();
    timer_.Start(
        FROM_HERE, wallpaper_reload_delay_,
        base::BindRepeating(&WallpaperController::ReloadWallpaper,
                            weak_factory_.GetWeakPtr(), /*clear_cache=*/false));
  }
}

void WallpaperController::OnRootWindowAdded(aura::Window* root_window) {
  // The wallpaper hasn't been set yet.
  if (wallpaper_mode_ == WALLPAPER_NONE)
    return;

  // Handle resolution change for "built-in" images.
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_)
      ReloadWallpaper(/*clear_cache=*/true);
  }

  InstallDesktopController(root_window);
}

void WallpaperController::OnLocalStatePrefServiceInitialized(
    PrefService* pref_service) {
  local_state_ = pref_service;
  if (wallpaper_controller_client_) {
    wallpaper_controller_client_->OnReadyToSetWallpaper();
  } else {
    // Ensure unit tests have a wallpaper as placeholder.
    CreateEmptyWallpaperForTesting();
  }
}

void WallpaperController::OnWallpaperResized() {
  CalculateWallpaperColors();
  compositor_lock_.reset();
}

void WallpaperController::OnColorCalculationComplete() {
  const std::vector<SkColor> colors = color_calculator_->prominent_colors();
  color_calculator_.reset();
  // Use |WallpaperInfo::location| as the key for storing |prominent_colors_| in
  // the |kWallpaperColors| pref.
  // TODO(crbug.com/787134): The |prominent_colors_| of wallpapers with empty
  // location should be cached as well.
  if (!current_wallpaper_->wallpaper_info().location.empty()) {
    CacheProminentColors(colors, current_wallpaper_->wallpaper_info().location);
  }
  SetProminentColors(colors);
}

void WallpaperController::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Replace the device policy wallpaper with a user wallpaper if necessary.
  if (IsDevicePolicyWallpaper() && !ShouldSetDevicePolicyWallpaper())
    ReloadWallpaper(/*clear_cache=*/false);

  CalculateWallpaperColors();

  // The wallpaper may be dimmed/blurred based on session state. The color of
  // the dimming overlay depends on the prominent color cached from a previous
  // calculation, or a default color if cache is not available. It should never
  // depend on any in-flight color calculation.
  if (wallpaper_mode_ == WALLPAPER_IMAGE &&
      (state == session_manager::SessionState::ACTIVE ||
       state == session_manager::SessionState::LOCKED ||
       state == session_manager::SessionState::LOGIN_SECONDARY)) {
    // TODO(crbug.com/753518): Reuse the existing WallpaperWidgetController for
    // dimming/blur purpose.
    InstallDesktopControllerForAllWindows();
  }

  if (state == session_manager::SessionState::ACTIVE)
    MoveToUnlockedContainer();
  else
    MoveToLockedContainer();
}

void WallpaperController::CompositorLockTimedOut() {
  compositor_lock_.reset();
}

void WallpaperController::InitializePathsForTesting(
    const base::FilePath& user_data_path,
    const base::FilePath& chromeos_wallpapers_path,
    const base::FilePath& chromeos_custom_wallpapers_path) {
  SetGlobalUserDataDir(user_data_path);
  SetGlobalChromeOSWallpapersDir(chromeos_wallpapers_path);
  SetGlobalChromeOSCustomWallpapersDir(chromeos_custom_wallpapers_path);
}

void WallpaperController::ShowDefaultWallpaperForTesting() {
  SetDefaultWallpaperImpl(EmptyAccountId(), user_manager::USER_TYPE_REGULAR,
                          /*show_wallpaper=*/true);
}

void WallpaperController::CreateEmptyWallpaperForTesting() {
  ResetProminentColors();
  current_wallpaper_.reset();
  wallpaper_mode_ = WALLPAPER_IMAGE;
  InstallDesktopControllerForAllWindows();
}

void WallpaperController::SetClientForTesting(
    mojom::WallpaperControllerClientPtr client) {
  wallpaper_controller_client_ = std::move(client);
}

void WallpaperController::FlushForTesting() {
  if (wallpaper_controller_client_)
    wallpaper_controller_client_.FlushForTesting();
  mojo_observers_.FlushForTesting();
}

void WallpaperController::InstallDesktopController(aura::Window* root_window) {
  DCHECK_EQ(WALLPAPER_IMAGE, wallpaper_mode_);

  bool session_blocked =
      Shell::Get()->session_controller()->IsUserSessionBlocked();
  bool in_overview = Shell::Get()->window_selector_controller()->IsSelecting();
  bool is_wallpaper_blurred =
      (session_blocked || in_overview) && IsBlurAllowed();

  if (is_wallpaper_blurred_ != is_wallpaper_blurred) {
    is_wallpaper_blurred_ = is_wallpaper_blurred;
    for (auto& observer : observers_)
      observer.OnWallpaperBlurChanged();
    mojo_observers_.ForAllPtrs([this](mojom::WallpaperObserver* observer) {
      observer->OnWallpaperBlurChanged(is_wallpaper_blurred_);
    });
  }

  const int container_id = GetWallpaperContainerId(locked_);
  float blur = login_constants::kClearBlurSigma;
  if (is_wallpaper_blurred) {
    blur = session_blocked ? login_constants::kBlurSigma
                           : WindowSelectorController::kWallpaperBlurSigma;
  }
  RootWindowController::ForWindow(root_window)
      ->wallpaper_widget_controller()
      ->SetWallpaperWidget(CreateWallpaperWidget(root_window, container_id),
                           blur);
}

void WallpaperController::InstallDesktopControllerForAllWindows() {
  for (aura::Window* root : Shell::GetAllRootWindows())
    InstallDesktopController(root);
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool WallpaperController::ReparentWallpaper(int container) {
  bool moved = false;
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (root_window_controller->wallpaper_widget_controller()->Reparent(
            root_window_controller->GetRootWindow(), container)) {
      moved = true;
    }
  }
  return moved;
}

int WallpaperController::GetWallpaperContainerId(bool locked) {
  return locked ? kShellWindowId_LockScreenWallpaperContainer
                : kShellWindowId_WallpaperContainer;
}

void WallpaperController::RemoveUserWallpaperInfo(const AccountId& account_id,
                                                  bool is_ephemeral) {
  if (wallpaper_cache_map_.find(account_id) != wallpaper_cache_map_.end())
    wallpaper_cache_map_.erase(account_id);

  if (!local_state_)
    return;
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info, is_ephemeral);
  DictionaryPrefUpdate prefs_wallpapers_info_update(local_state_,
                                                    prefs::kUserWallpaperInfo);
  prefs_wallpapers_info_update->RemoveWithoutPathExpansion(
      account_id.GetUserEmail(), nullptr);
  // Remove the color cache of the previous wallpaper if it exists.
  DictionaryPrefUpdate wallpaper_colors_update(local_state_,
                                               prefs::kWallpaperColors);
  wallpaper_colors_update->RemoveWithoutPathExpansion(info.location, nullptr);
}

void WallpaperController::RemoveUserWallpaperImpl(
    const AccountId& account_id,
    const std::string& wallpaper_files_id) {
  if (wallpaper_files_id.empty())
    return;

  std::vector<base::FilePath> files_to_remove;

  // Remove small user wallpapers.
  base::FilePath wallpaper_path = GetCustomWallpaperDir(kSmallWallpaperSubDir);
  files_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  // Remove large user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  files_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  // Remove original user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  files_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DeleteWallpaperInList, std::move(files_to_remove)));
}

void WallpaperController::SetDefaultWallpaperImpl(
    const AccountId& account_id,
    const user_manager::UserType& user_type,
    bool show_wallpaper) {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode())
    return;

  wallpaper_cache_map_.erase(account_id);

  const bool use_small =
      (GetAppropriateResolution() == WALLPAPER_RESOLUTION_SMALL);
  WallpaperLayout layout =
      use_small ? WALLPAPER_LAYOUT_CENTER : WALLPAPER_LAYOUT_CENTER_CROPPED;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath file_path;
  base::Optional<user_manager::UserType> active_user_type =
      Shell::Get()->session_controller()->GetUserType();

  // The wallpaper is determined in the following order:
  // Guest wallpaper, child wallpaper, customized default wallpaper, and regular
  // default wallpaper.
  // TODO(wzang|xdai): The current code intentionally distinguishes between
  // |active_user_type| and |user_type|. We should try to unify them.
  if (active_user_type && *active_user_type == user_manager::USER_TYPE_GUEST) {
    const std::string switch_string =
        use_small ? chromeos::switches::kGuestWallpaperSmall
                  : chromeos::switches::kGuestWallpaperLarge;
    file_path = command_line->GetSwitchValuePath(switch_string);
  } else if (user_type == user_manager::USER_TYPE_CHILD) {
    const std::string switch_string =
        use_small ? chromeos::switches::kChildWallpaperSmall
                  : chromeos::switches::kChildWallpaperLarge;
    file_path = command_line->GetSwitchValuePath(switch_string);
  } else if (!customized_default_small_path_.empty()) {
    DCHECK(!customized_default_large_path_.empty());
    file_path = use_small ? customized_default_small_path_
                          : customized_default_large_path_;
  } else {
    const std::string switch_string =
        use_small ? chromeos::switches::kDefaultWallpaperSmall
                  : chromeos::switches::kDefaultWallpaperLarge;
    file_path = command_line->GetSwitchValuePath(switch_string);
  }

  // We need to decode the image if there's no cache, or if the file path
  // doesn't match the cached value (i.e. the cache is outdated). Otherwise,
  // directly run the callback with the cached image.
  if (!cached_default_wallpaper_.image.isNull() &&
      cached_default_wallpaper_.file_path == file_path) {
    OnDefaultWallpaperDecoded(file_path, layout, show_wallpaper,
                              cached_default_wallpaper_.image);
  } else {
    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperController::OnDefaultWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), file_path, layout,
                       show_wallpaper),
        sequenced_task_runner_, file_path);
  }
}

bool WallpaperController::CanSetUserWallpaper(const AccountId& account_id,
                                              bool is_ephemeral) const {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode())
    return false;
  // Don't allow user wallpapers while policy is in effect.
  if (IsPolicyControlled(account_id, is_ephemeral))
    return false;
  return true;
}

bool WallpaperController::WallpaperIsAlreadyLoaded(
    const gfx::ImageSkia& image,
    bool compare_layouts,
    WallpaperLayout layout) const {
  if (!current_wallpaper_)
    return false;

  // Compare layouts only if necessary.
  if (compare_layouts && layout != current_wallpaper_->wallpaper_info().layout)
    return false;

  return WallpaperResizer::GetImageId(image) ==
         current_wallpaper_->original_image_id();
}

void WallpaperController::ReadAndDecodeWallpaper(
    LoadedCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& file_path) {
  decode_requests_for_testing_.push_back(file_path);
  if (bypass_decode_for_testing_) {
    std::move(callback).Run(CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }
  std::string* data = new std::string;
  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&base::ReadFileToString, file_path, data),
      base::BindOnce(&OnWallpaperDataRead, std::move(callback),
                     base::Passed(base::WrapUnique(data))));
}

bool WallpaperController::InitializeUserWallpaperInfo(
    const AccountId& account_id,
    bool is_ephemeral) {
  const WallpaperInfo info = {std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                              DEFAULT, base::Time::Now().LocalMidnight()};
  return SetUserWallpaperInfo(account_id, info, is_ephemeral);
}

void WallpaperController::SetOnlineWallpaperFromPath(
    SetOnlineWallpaperIfExistsCallback callback,
    const OnlineWallpaperParams& params,
    const base::FilePath& file_path) {
  bool file_exists = !file_path.empty();
  std::move(callback).Run(file_exists);
  if (file_exists) {
    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperController::OnOnlineWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), params, /*save_file=*/false,
                       SetOnlineWallpaperFromDataCallback()),
        sequenced_task_runner_, file_path);
  }
}

void WallpaperController::OnOnlineWallpaperDecoded(
    const OnlineWallpaperParams& params,
    bool save_file,
    SetOnlineWallpaperFromDataCallback callback,
    const gfx::ImageSkia& image) {
  bool success = !image.isNull();
  if (callback)
    std::move(callback).Run(success);
  if (!success) {
    LOG(ERROR) << "Failed to decode online wallpaper.";
    return;
  }

  if (save_file) {
    image.EnsureRepsForSupportedScales();
    std::unique_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveOnlineWallpaper, params.url, params.layout,
                       base::Passed(std::move(deep_copy))));
  }

  const bool is_active_user = IsActiveUser(params.account_id);
  if (params.preview_mode) {
    DCHECK(is_active_user);
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperController::SetOnlineWallpaperImpl,
        weak_factory_.GetWeakPtr(), params, image, /*show_wallpaper=*/false);
    reload_preview_wallpaper_callback_ =
        base::BindRepeating(&WallpaperController::ShowWallpaperImage,
                            weak_factory_.GetWeakPtr(), image,
                            WallpaperInfo{params.url, params.layout, ONLINE,
                                          base::Time::Now().LocalMidnight()},
                            /*preview_mode=*/true);
    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SetOnlineWallpaperImpl(params, image, /*show_wallpaper=*/is_active_user);
  }
}

void WallpaperController::SetOnlineWallpaperImpl(
    const OnlineWallpaperParams& params,
    const gfx::ImageSkia& image,
    bool show_wallpaper) {
  WallpaperInfo wallpaper_info = {params.url, params.layout, ONLINE,
                                  base::Time::Now().LocalMidnight()};
  if (!SetUserWallpaperInfo(params.account_id, wallpaper_info,
                            params.is_ephemeral)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }
  if (show_wallpaper)
    ShowWallpaperImage(image, wallpaper_info, /*preview_mode=*/false);

  wallpaper_cache_map_[params.account_id] =
      CustomWallpaperElement(base::FilePath(), image);
}

void WallpaperController::SetWallpaperFromInfo(
    const AccountId& account_id,
    const user_manager::UserType& user_type,
    const WallpaperInfo& info,
    bool show_wallpaper) {
  if (info.type != ONLINE && info.type != DEFAULT) {
    // This method is meant to be used for ONLINE and DEFAULT types. In
    // unexpected cases, revert to default wallpaper to fail safely. See
    // crosbug.com/38429.
    LOG(ERROR) << "Wallpaper reverts to default unexpected.";
    SetDefaultWallpaperImpl(account_id, user_type, show_wallpaper);
    return;
  }

  // Do a sanity check that the file path is not empty.
  if (info.location.empty()) {
    // File name might be empty on debug configurations when stub users
    // were created directly in local state (for testing). Ignore such
    // errors i.e. allow such type of debug configurations on the desktop.
    LOG(WARNING) << "User wallpaper info is empty: " << account_id.Serialize();
    SetDefaultWallpaperImpl(account_id, user_type, show_wallpaper);
    return;
  }

  base::FilePath wallpaper_path;
  if (info.type == ONLINE) {
    wallpaper_path =
        GetOnlineWallpaperPath(info.location, GetAppropriateResolution());

    // If the wallpaper exists and it already contains the correct image we
    // can return immediately.
    CustomWallpaperMap::iterator it = wallpaper_cache_map_.find(account_id);
    if (it != wallpaper_cache_map_.end() &&
        it->second.first == wallpaper_path && !it->second.second.isNull())
      return;

    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperController::OnWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), account_id, user_type,
                       wallpaper_path, info, show_wallpaper),
        sequenced_task_runner_, wallpaper_path);
  } else {
    // Default wallpapers are migrated from M21 user profiles. A code
    // refactor overlooked that case and caused these wallpapers not being
    // loaded at all. On some slow devices, it caused login webui not
    // visible after upgrade to M26 from M21. See crosbug.com/38429 for
    // details.
    DCHECK(!GlobalUserDataDir().empty());
    wallpaper_path = GlobalUserDataDir().Append(info.location);

    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperController::OnWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), account_id, user_type,
                       wallpaper_path, info, show_wallpaper),
        sequenced_task_runner_, wallpaper_path);
  }
}

void WallpaperController::OnDefaultWallpaperDecoded(
    const base::FilePath& path,
    WallpaperLayout layout,
    bool show_wallpaper,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Create a solid color wallpaper if the default wallpaper decoding fails.
    cached_default_wallpaper_.image =
        CreateSolidColorWallpaper(kDefaultWallpaperColor);
    cached_default_wallpaper_.file_path.clear();
  } else {
    cached_default_wallpaper_.image = image;
    cached_default_wallpaper_.file_path = path;
  }

  if (show_wallpaper) {
    WallpaperInfo info(cached_default_wallpaper_.file_path.value(), layout,
                       DEFAULT, base::Time::Now().LocalMidnight());
    ShowWallpaperImage(cached_default_wallpaper_.image, info,
                       /*preview_mode=*/false);
  }
}

void WallpaperController::SaveAndSetWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    WallpaperType type,
    WallpaperLayout layout,
    bool show_wallpaper,
    const gfx::ImageSkia& image) {
  // If the image of the new wallpaper is empty, the current wallpaper is still
  // kept instead of reverting to the default.
  if (image.isNull()) {
    LOG(ERROR) << "The wallpaper image is empty due to a decoding failure, or "
                  "the client provided an empty image.";
    return;
  }

  const std::string relative_path =
      base::FilePath(wallpaper_files_id).Append(file_name).value();
  // User's custom wallpaper path is determined by relative path and the
  // appropriate wallpaper resolution.
  WallpaperInfo info = {relative_path, layout, type,
                        base::Time::Now().LocalMidnight()};
  if (!SetUserWallpaperInfo(user_info->account_id, info,
                            user_info->is_ephemeral)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }

  base::FilePath wallpaper_path =
      GetCustomWallpaperPath(WallpaperController::kOriginalWallpaperSubDir,
                             wallpaper_files_id, file_name);

  const bool should_save_to_disk =
      !user_info->is_ephemeral ||
      (type == POLICY &&
       user_info->type == user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  if (should_save_to_disk) {
    image.EnsureRepsForSupportedScales();
    std::unique_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
    // Block shutdown on this task. Otherwise, we may lose the custom wallpaper
    // that the user selected.
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    blocking_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveCustomWallpaper, wallpaper_files_id, wallpaper_path,
                       layout, base::Passed(std::move(deep_copy))));
  }

  if (show_wallpaper)
    ShowWallpaperImage(image, info, /*preview_mode=*/false);

  wallpaper_cache_map_[user_info->account_id] =
      CustomWallpaperElement(wallpaper_path, image);
}

void WallpaperController::OnWallpaperDecoded(
    const AccountId& account_id,
    const user_manager::UserType& user_type,
    const base::FilePath& path,
    const WallpaperInfo& info,
    bool show_wallpaper,
    const gfx::ImageSkia& image) {
  // Empty image indicates decode failure. Use default wallpaper in this case.
  if (image.isNull()) {
    LOG(ERROR) << "Failed to decode user wallpaper at " << path.value()
               << " Falls back to default wallpaper. ";
    SetDefaultWallpaperImpl(account_id, user_type, show_wallpaper);
    return;
  }

  wallpaper_cache_map_[account_id] = CustomWallpaperElement(path, image);
  if (show_wallpaper)
    ShowWallpaperImage(image, info, /*preview_mode=*/false);
}

void WallpaperController::ReloadWallpaper(bool clear_cache) {
  current_wallpaper_.reset();
  if (clear_cache)
    wallpaper_cache_map_.clear();

  if (reload_preview_wallpaper_callback_)
    reload_preview_wallpaper_callback_.Run();
  else if (current_user_)
    ShowUserWallpaper(std::move(current_user_));
  else
    ShowSigninWallpaper();
}

void WallpaperController::SetProminentColors(
    const std::vector<SkColor>& colors) {
  if (prominent_colors_ == colors)
    return;

  prominent_colors_ = colors;
  for (auto& observer : observers_)
    observer.OnWallpaperColorsChanged();
  mojo_observers_.ForAllPtrs([this](mojom::WallpaperObserver* observer) {
    observer->OnWallpaperColorsChanged(prominent_colors_);
  });
}

void WallpaperController::ResetProminentColors() {
  static const std::vector<SkColor> kInvalidColors(color_profiles_.size(),
                                                   kInvalidWallpaperColor);
  SetProminentColors(kInvalidColors);
}

void WallpaperController::CalculateWallpaperColors() {
  if (!current_wallpaper_)
    return;

  // Cancel any in-flight color calculation.
  if (color_calculator_) {
    color_calculator_->RemoveObserver(this);
    color_calculator_.reset();
  }

  // Fetch the color cache if it exists.
  if (!current_wallpaper_->wallpaper_info().location.empty()) {
    base::Optional<std::vector<SkColor>> cached_colors =
        GetCachedColors(current_wallpaper_->wallpaper_info().location);
    if (cached_colors.has_value()) {
      SetProminentColors(cached_colors.value());
      return;
    }
  }

  // Color calculation is only allowed during an active session for performance
  // reasons. Observers outside an active session are notified of the cache, or
  // an invalid color if a previous calculation during active session failed.
  if (!ShouldCalculateColors()) {
    ResetProminentColors();
    return;
  }

  color_calculator_ = std::make_unique<WallpaperColorCalculator>(
      GetWallpaper(), color_profiles_, sequenced_task_runner_);
  color_calculator_->AddObserver(this);
  if (!color_calculator_->StartCalculation()) {
    ResetProminentColors();
  }
}

bool WallpaperController::ShouldCalculateColors() const {
  gfx::ImageSkia image = GetWallpaper();
  return IsShelfColoringEnabled() &&
         Shell::Get()->session_controller()->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !image.isNull();
}

void WallpaperController::CacheProminentColors(
    const std::vector<SkColor>& colors,
    const std::string& current_location) {
  if (!local_state_)
    return;
  DictionaryPrefUpdate wallpaper_colors_update(local_state_,
                                               prefs::kWallpaperColors);
  auto wallpaper_colors = std::make_unique<base::ListValue>();
  for (SkColor color : colors)
    wallpaper_colors->AppendDouble(static_cast<double>(color));
  wallpaper_colors_update->SetWithoutPathExpansion(current_location,
                                                   std::move(wallpaper_colors));
}

base::Optional<std::vector<SkColor>> WallpaperController::GetCachedColors(
    const std::string& current_location) const {
  base::Optional<std::vector<SkColor>> cached_colors_out;
  const base::ListValue* prominent_colors = nullptr;
  if (!local_state_ ||
      !local_state_->GetDictionary(prefs::kWallpaperColors)
           ->GetListWithoutPathExpansion(current_location, &prominent_colors)) {
    return cached_colors_out;
  }
  cached_colors_out = std::vector<SkColor>();
  cached_colors_out.value().reserve(prominent_colors->GetList().size());
  for (const auto& value : *prominent_colors) {
    cached_colors_out.value().push_back(
        static_cast<SkColor>(value.GetDouble()));
  }
  return cached_colors_out;
}

bool WallpaperController::MoveToLockedContainer() {
  if (locked_)
    return false;

  locked_ = true;
  return ReparentWallpaper(GetWallpaperContainerId(true));
}

bool WallpaperController::MoveToUnlockedContainer() {
  if (!locked_)
    return false;

  locked_ = false;
  return ReparentWallpaper(GetWallpaperContainerId(false));
}

bool WallpaperController::IsDevicePolicyWallpaper() const {
  return current_wallpaper_ &&
         current_wallpaper_->wallpaper_info().type == WallpaperType::DEVICE;
}

bool WallpaperController::IsOneShotWallpaper() const {
  return current_wallpaper_ &&
         current_wallpaper_->wallpaper_info().type == WallpaperType::ONE_SHOT;
}

bool WallpaperController::ShouldSetDevicePolicyWallpaper() const {
  // Only allow the device wallpaper if the policy is in effect for enterprise
  // managed devices.
  if (!is_device_wallpaper_policy_enforced_)
    return false;

  // Only set the device wallpaper if we're at the login screen.
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    return false;
  }

  return true;
}

void WallpaperController::SetDevicePolicyWallpaper() {
  DCHECK(ShouldSetDevicePolicyWallpaper());
  ReadAndDecodeWallpaper(
      base::BindRepeating(&WallpaperController::OnDevicePolicyWallpaperDecoded,
                          weak_factory_.GetWeakPtr()),
      sequenced_task_runner_.get(), GlobalDevicePolicyWallpaperFile());
}

void WallpaperController::OnDevicePolicyWallpaperDecoded(
    const gfx::ImageSkia& image) {
  // It might be possible that the device policy controlled wallpaper finishes
  // decoding after the user logs in. In this case do nothing.
  if (!ShouldSetDevicePolicyWallpaper())
    return;

  if (image.isNull()) {
    // If device policy wallpaper failed decoding, fall back to the default
    // wallpaper.
    SetDefaultWallpaperImpl(EmptyAccountId(), user_manager::USER_TYPE_REGULAR,
                            /*show_wallpaper=*/true);
  } else {
    WallpaperInfo info(GlobalDevicePolicyWallpaperFile().value(),
                       WALLPAPER_LAYOUT_CENTER_CROPPED, DEVICE,
                       base::Time::Now().LocalMidnight());
    ShowWallpaperImage(image, info, /*preview_mode=*/false);
  }
}

bool WallpaperController::IsActiveUserWallpaperControlledByPolicyImpl() const {
  // The currently active user has index 0.
  const mojom::UserSession* const active_user_session =
      Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
  if (!active_user_session)
    return false;
  return IsPolicyControlled(active_user_session->user_info->account_id,
                            active_user_session->user_info->is_ephemeral);
}

bool WallpaperController::GetActiveUserWallpaperInfoImpl(
    WallpaperInfo* info_out) const {
  // The currently active user has index 0.
  const mojom::UserSession* const active_user_session =
      Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
  if (!active_user_session)
    return false;

  if (!GetUserWallpaperInfo(active_user_session->user_info->account_id,
                            info_out,
                            active_user_session->user_info->is_ephemeral)) {
    return false;
  }
  return true;
}

bool WallpaperController::ShouldShowWallpaperSettingImpl() const {
  // The currently active user has index 0.
  const mojom::UserSession* const active_user_session =
      Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
  if (!active_user_session)
    return false;

  user_manager::UserType active_user_type =
      active_user_session->user_info->type;
  return active_user_type == user_manager::USER_TYPE_REGULAR ||
         active_user_type == user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
         active_user_type == user_manager::USER_TYPE_SUPERVISED ||
         active_user_type == user_manager::USER_TYPE_CHILD;
}

void WallpaperController::GetInternalDisplayCompositorLock() {
  if (!display::Display::HasInternalDisplay())
    return;

  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(display::Display::InternalDisplayId());
  if (!root_window)
    return;

  compositor_lock_ = root_window->layer()->GetCompositor()->GetCompositorLock(
      this, kCompositorLockTimeout);
}

}  // namespace ash
