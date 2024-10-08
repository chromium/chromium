// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller_impl.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_drivefs_delegate.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/system/time/time_of_day.h"
#include "ash/wallpaper/online_wallpaper_manager.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_blur_manager.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_daily_refresh_scheduler.h"
#include "ash/wallpaper/wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_info_migrator.h"
#include "ash/wallpaper/wallpaper_metrics_manager.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_ephemeral_user.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resolution.h"
#include "ash/wallpaper/wallpaper_window_state_manager.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"
#include "url/gurl.h"

using color_utils::ColorProfile;

using FilePathCallback = base::OnceCallback<void(const base::FilePath&)>;

namespace ash {

namespace {

// Global to hold a WallpaperPrefManager for testing in `Create`.
std::unique_ptr<WallpaperPrefManager> g_test_pref_manager;

// Global to hold a WallpaperImageDownloader for testing in `Create`.
std::unique_ptr<WallpaperImageDownloader> g_test_image_downloader;

// The file name of the policy wallpaper.
constexpr char kPolicyWallpaperFile[] = "policy-controlled.jpeg";

// How long to wait reloading the wallpaper after the display size has changed.
constexpr base::TimeDelta kWallpaperReloadDelay = base::Milliseconds(100);

// How long to wait for resizing of the the wallpaper.
constexpr base::TimeDelta kCompositorLockTimeout = base::Milliseconds(750);

// The color of the wallpaper if no other wallpaper images are available.
constexpr SkColor kDefaultWallpaperColor = SK_ColorGRAY;

// The color of the Oobe wallpaper if no other wallpaper images are available.
constexpr SkColor kOobeWallpaperColor = SK_ColorWHITE;

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

base::FilePath& GlobalChromeOSGooglePhotosWallpapersDir() {
  static base::NoDestructor<base::FilePath>
      dir_chrome_os_google_photos_wallpapers;
  return *dir_chrome_os_google_photos_wallpapers;
}

base::FilePath& GlobalChromeOSSeaPenWallpaperDir() {
  static base::NoDestructor<base::FilePath> dir_chrome_os_sea_pen_wallpaper;
  return *dir_chrome_os_sea_pen_wallpaper;
}

void SetGlobalUserDataDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalUserDataDir();
  global_path = path;
}

void SetGlobalChromeOSWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSWallpapersDir();
  global_path = path;
}

void SetGlobalChromeOSGooglePhotosWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSGooglePhotosWallpapersDir();
  global_path = path;
}

void SetGlobalChromeOSSeaPenWallpaperDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSSeaPenWallpaperDir();
  global_path = path;
}

void SetGlobalChromeOSCustomWallpapersDir(const base::FilePath& path) {
  base::FilePath& global_path = GlobalChromeOSCustomWallpapersDir();
  global_path = path;
}

base::FilePath GetUserGooglePhotosWallpaperDir(const AccountId& account_id) {
  DCHECK(account_id.HasAccountIdKey());
  return GlobalChromeOSGooglePhotosWallpapersDir().Append(
      account_id.GetAccountIdKey());
}

base::FilePath GetUserSeaPenWallpaperDir(const AccountId& account_id) {
  DCHECK(account_id.HasAccountIdKey());
  const base::FilePath& global_sea_pen_dir = GlobalChromeOSSeaPenWallpaperDir();
  DCHECK(!global_sea_pen_dir.empty());
  return global_sea_pen_dir.Append(account_id.GetAccountIdKey());
}

// Returns wallpaper subdirectory name for current resolution.
std::string GetCustomWallpaperSubdirForCurrentResolution() {
  WallpaperResolution resolution = GetAppropriateResolution();
  return resolution == WallpaperResolution::kSmall ? kSmallWallpaperSubDir
                                                   : kLargeWallpaperSubDir;
}

// Creates a 1x1 solid color image.
gfx::ImageSkia CreateSolidColorWallpaper(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Deletes a list of wallpaper files in |file_list|.
void DeleteWallpaperInList(std::vector<base::FilePath> file_list) {
  for (const base::FilePath& path : file_list) {
    if (!base::DeletePathRecursively(path))
      LOG(ERROR) << "Failed to remove user wallpaper at " << path.value();
  }
}

// Checks if kiosk app is running. Note: it returns false either when there's
// no active user (e.g. at login screen), or the active user is not kiosk.
bool IsInKioskMode() {
  std::optional<user_manager::UserType> active_user_type =
      Shell::Get()->session_controller()->GetUserType();
  // |active_user_type| is empty when there's no active user.
  return active_user_type &&
         *active_user_type == user_manager::UserType::kKioskApp;
}

// Returns the currently active user session (at index 0).
const UserSession* GetActiveUserSession() {
  return Shell::Get()->session_controller()->GetUserSession(/*user index=*/0);
}

AccountId GetActiveAccountId() {
  const UserSession* const session = GetActiveUserSession();
  DCHECK(session);
  return session->user_info.account_id;
}

// Checks if |account_id| is the current active user.
bool IsActiveUser(const AccountId& account_id) {
  const UserSession* const session = GetActiveUserSession();
  return session && session->user_info.account_id == account_id;
}

// Returns the type of the user with the specified |id| or kRegular.
user_manager::UserType GetUserType(const AccountId& id) {
  const UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSessionByAccountId(id);
  // If we can't match the account with a session, we can't safely continue.
  if (!user_session) {
    // TODO(crbug.com/1329256): Change tests that hit this codepath to sign in
    // users first if they have an active session so that this can be changed to
    // a CHECK.
    LOG(ERROR) << "Cannot resolve user. Assuming regular. This should only "
                  "happen in tests";
    return user_manager::UserType::kRegular;
  }

  return user_session->user_info.type;
}

// Gets |account_id|'s custom wallpaper at |wallpaper_path|. Falls back to the
// original custom wallpaper. Verifies that the returned path exists. If a valid
// path cannot be found, returns an empty FilePath. Must run on wallpaper
// sequenced worker thread.
base::FilePath PathWithFallback(const AccountId& account_id,
                                const WallpaperInfo& info,
                                const base::FilePath& wallpaper_path) {
  if (base::PathExists(wallpaper_path))
    return wallpaper_path;

  // Falls back to the original file if the file with correct resolution does
  // not exist. This may happen when the original custom wallpaper is small or
  // browser shutdown before resized wallpaper saved.
  base::FilePath valid_path =
      WallpaperControllerImpl::GetCustomWallpaperDir(kOriginalWallpaperSubDir)
          .Append(info.location);

  return base::PathExists(valid_path) ? valid_path : base::FilePath();
}

// Deletes the user-specific directory inside the Google Photos cache
// directory. Only call this by posting it to `sequenced_task_runner_` with no
// delay to ensure that file IO is called in a well defined order. This avoids
// accidentally deleting the cache immediately after creating it, etc.
void DeleteGooglePhotosCache(const AccountId& account_id) {
  // Don't bother deleting for anyone without an AccountId, since they don't
  // have a way to set Google Photos Wallpapers. Guest accounts may not be able
  // to call `AccountId::GetAccountIdKey()`, so we can't compute a path for
  // them.
  if (account_id.HasAccountIdKey()) {
    base::DeletePathRecursively(GetUserGooglePhotosWallpaperDir(account_id));
  }
}

scoped_refptr<base::RefCountedMemory> EncodeAndResizeImage(
    gfx::ImageSkia image) {
  auto resized = WallpaperResizer::GetResizedImage(image,
                                                   /*max_size_in_dips=*/1024);
  // Conversion quality between 0 - 100. Manually tested to use 90 for good
  // performance with reasonable quality.
  std::optional<std::vector<uint8_t>> jpg_buffer =
      gfx::JPEG1xEncodedDataFromImage(gfx::Image(resized), /*quality=*/90);
  if (jpg_buffer) {
    return base::RefCountedBytes::TakeVector(&jpg_buffer.value());
  }
  return base::MakeRefCounted<base::RefCountedBytes>(0);
}

// Selects the online wallpaper variant to show and specifies it in the
// returned `WallpaperInfo`. Returns nullptr on failure.
std::unique_ptr<WallpaperInfo> CreateOnlineWallpaperInfo(
    const OnlineWallpaperParams& params,
    const ScheduledFeature& scheduled_feature,
    const char* source) {
  const OnlineWallpaperVariant* selected_variant = FirstValidVariant(
      params.variants, scheduled_feature.current_checkpoint());
  if (!selected_variant) {
    LOG(ERROR) << "Failed to select online wallpaper variant from " << source;
    return nullptr;
  }
  return std::make_unique<WallpaperInfo>(params, *selected_variant);
}

}  // namespace

// static
std::unique_ptr<WallpaperControllerImpl> WallpaperControllerImpl::Create(
    PrefService* local_state) {
  std::unique_ptr<WallpaperPrefManager> pref_manager =
      g_test_pref_manager ? std::move(g_test_pref_manager)
                          : WallpaperPrefManager::Create(local_state);

  std::unique_ptr<WallpaperImageDownloader> wallpaper_image_downloader =
      g_test_image_downloader
          ? std::move(g_test_image_downloader)
          : std::make_unique<WallpaperImageDownloaderImpl>();

  return std::make_unique<WallpaperControllerImpl>(
      std::move(pref_manager), std::move(wallpaper_image_downloader));
}

// static
void WallpaperControllerImpl::SetWallpaperPrefManagerForTesting(
    std::unique_ptr<WallpaperPrefManager> pref_manager) {
  g_test_pref_manager.swap(pref_manager);
}

// static
void WallpaperControllerImpl::SetWallpaperImageDownloaderForTesting(
    std::unique_ptr<WallpaperImageDownloader> image_downloader) {
  g_test_image_downloader.swap(image_downloader);
}

WallpaperControllerImpl::WallpaperControllerImpl(
    std::unique_ptr<WallpaperPrefManager> pref_manager,
    std::unique_ptr<WallpaperImageDownloader> image_downloader)
    : pref_manager_(std::move(pref_manager)),
      blur_manager_(std::make_unique<WallpaperBlurManager>()),
      wallpaper_reload_delay_(kWallpaperReloadDelay),
      wallpaper_image_downloader_(std::move(image_downloader)),
      wallpaper_file_manager_(std::make_unique<WallpaperFileManager>()),
      online_wallpaper_manager_(
          OnlineWallpaperManager(wallpaper_image_downloader_.get(),
                                 wallpaper_file_manager_.get())),
      google_photos_wallpaper_manager_(
          GooglePhotosWallpaperManager(wallpaper_image_downloader_.get(),
                                       wallpaper_file_manager_.get())),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->login_screen_controller()->data_dispatcher()->AddObserver(this);
  theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  wallpaper_metrics_manager_ = std::make_unique<WallpaperMetricsManager>();
}

WallpaperControllerImpl::~WallpaperControllerImpl() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  // Per ash/shell.cc, wallpaper_controller_impl outlives
  // login_screen_controller. Therefore don't remove the observer from
  // data_dispatcher on destruction.
}

// static
base::FilePath WallpaperControllerImpl::GetCustomWallpaperPath(
    const std::string& sub_dir,
    const std::string& wallpaper_files_id,
    const std::string& file_name) {
  base::FilePath custom_wallpaper_path = GetCustomWallpaperDir(sub_dir);
  return custom_wallpaper_path.Append(wallpaper_files_id).Append(file_name);
}

// static
base::FilePath WallpaperControllerImpl::GetCustomWallpaperDir(
    const std::string& sub_dir) {
  DCHECK(!GlobalChromeOSCustomWallpapersDir().empty());
  return GlobalChromeOSCustomWallpapersDir().Append(sub_dir);
}

SkColor WallpaperControllerImpl::GetKMeanColor() const {
  return calculated_colors_ ? calculated_colors_->k_mean_color
                            : kInvalidWallpaperColor;
}

std::optional<SkColor> WallpaperControllerImpl::GetCachedWallpaperColorForUser(
    const AccountId& account_id,
    bool should_use_k_means) const {
  WallpaperInfo info;
  if (!pref_manager_->GetLocalWallpaperInfo(account_id, &info)) {
    return {};
  }
  return should_use_k_means ? pref_manager_->GetCachedKMeanColor(info.location)
                            : pref_manager_->GetCelebiColor(info.location);
}

gfx::ImageSkia WallpaperControllerImpl::GetWallpaper() const {
  return current_wallpaper_ ? current_wallpaper_->image() : gfx::ImageSkia();
}

WallpaperLayout WallpaperControllerImpl::GetWallpaperLayout() const {
  return current_wallpaper_ ? current_wallpaper_->wallpaper_info().layout
                            : NUM_WALLPAPER_LAYOUT;
}

WallpaperType WallpaperControllerImpl::GetWallpaperType() const {
  return current_wallpaper_ ? current_wallpaper_->wallpaper_info().type
                            : WallpaperType::kCount;
}

bool WallpaperControllerImpl::ShouldShowInitialAnimation() {
  // The slower initial animation is only applicable if:
  // 1) It's the first run after system boot, not after user sign-out.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFirstExecAfterBoot)) {
    return false;
  }
  // 2) It's at the login screen.
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLoginManager)) {
    return false;
  }
  // 3) It's the first wallpaper being shown, not for the switching between
  //    multiple user pods.
  if (!is_first_wallpaper_)
    return false;

  return true;
}

bool WallpaperControllerImpl::HasShownAnyWallpaper() const {
  return !!current_wallpaper_;
}

void WallpaperControllerImpl::MaybeClosePreviewWallpaper() {
  if (!confirm_preview_wallpaper_callback_) {
    DCHECK(!reload_preview_wallpaper_callback_);
    return;
  }
  CancelPreviewWallpaper();
}

void WallpaperControllerImpl::ShowWallpaperImage(const gfx::ImageSkia& image,
                                                 WallpaperInfo info,
                                                 bool preview_mode,
                                                 bool is_override) {
  // Prevent showing other wallpapers if there is an override wallpaper.
  if (is_override_wallpaper_ && !is_override) {
    return;
  }

  // Ignore show wallpaper requests during preview mode. This could happen if a
  // custom wallpaper previously set on another device is being synced.
  if (confirm_preview_wallpaper_callback_ && !preview_mode)
    return;

  if (preview_mode) {
    DVLOG(1) << __func__ << " preview_mode=true";
    for (auto& observer : observers_)
      observer.OnWallpaperPreviewStarted();
  }

  // 1x1 wallpaper should be stretched to fill the entire screen.
  // (WALLPAPER_LAYOUT_TILE also serves this purpose.)
  if (image.width() == 1 && image.height() == 1)
    info.layout = WALLPAPER_LAYOUT_STRETCH;

  if (info.type == WallpaperType::kOneShot)
    info.one_shot_wallpaper = image.DeepCopy();

  VLOG(1) << "SetWallpaper: image_id=" << WallpaperResizer::GetImageId(image)
          << " layout=" << info.layout;

  if (WallpaperIsAlreadyLoaded(image, /*compare_layouts=*/true, info.layout)) {
    VLOG(1) << "Wallpaper is already loaded";
    return;
  }

  // Cancel any in-flight color calculation because we have a new wallpaper.
  if (color_calculator_) {
    color_calculator_.reset();
  }

  is_first_wallpaper_ = !current_wallpaper_;
  current_wallpaper_ = std::make_unique<WallpaperResizer>(
      image, GetMaxDisplaySizeInNative(), info);
  // `this` owns `current_wallpaper_` and therefore can use `base::Unretained`.
  current_wallpaper_->StartResize(base::BindOnce(
      &WallpaperControllerImpl::OnWallpaperResized, base::Unretained(this)));

  if (is_first_wallpaper_) {
    for (auto& observer : observers_)
      observer.OnFirstWallpaperShown();
  }

  for (auto& observer : observers_)
    observer.OnWallpaperChanging();

  wallpaper_mode_ = WALLPAPER_IMAGE;
  UpdateWallpaperForAllRootWindows(
      Shell::Get()->session_controller()->IsUserSessionBlocked());
  ++wallpaper_count_for_testing_;

  for (auto& observer : observers_)
    observer.OnWallpaperChanged();
}

void WallpaperControllerImpl::UpdateWallpaperBlurForLockState(bool blur) {
  bool changed =
      blur_manager_->UpdateWallpaperBlurForLockState(blur, GetWallpaperType());
  if (changed) {
    for (auto& observer : observers_) {
      observer.OnWallpaperBlurChanged();
    }
  }
}

void WallpaperControllerImpl::RestoreWallpaperBlurForLockState(float blur) {
  const WallpaperType wallpaper_type = GetWallpaperType();
  if (!blur_manager_->IsBlurAllowedForLockState(wallpaper_type)) {
    return;
  }

  blur_manager_->RestoreWallpaperBlurForLockState(blur, wallpaper_type);
  for (auto& observer : observers_) {
    observer.OnWallpaperBlurChanged();
  }
}

bool WallpaperControllerImpl::ShouldApplyShield() const {
  bool needs_shield = false;
  if (Shell::Get()->overview_controller()->InOverviewSession()) {
    needs_shield = false;
  } else if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    needs_shield = true;
  } else if (display::Screen::GetScreen()->InTabletMode() &&
             !confirm_preview_wallpaper_callback_) {
    needs_shield = true;
  }

  return needs_shield && (!IsOneShotWallpaper() || allow_shield_for_testing_);
}

bool WallpaperControllerImpl::SetUserWallpaperInfo(const AccountId& account_id,
                                                   const WallpaperInfo& info) {
  CleanUpBeforeSettingUserWallpaperInfo(account_id, info);
  return pref_manager_->SetUserWallpaperInfo(account_id, info);
}

bool WallpaperControllerImpl::SetUserWallpaperInfo(const AccountId& account_id,
                                                   bool is_ephemeral,
                                                   const WallpaperInfo& info) {
  CleanUpBeforeSettingUserWallpaperInfo(account_id, info);
  return pref_manager_->SetUserWallpaperInfo(account_id, is_ephemeral, info);
}

bool WallpaperControllerImpl::GetUserWallpaperInfo(const AccountId& account_id,
                                                   WallpaperInfo* info) const {
  return pref_manager_->GetUserWallpaperInfo(account_id, info);
}

bool WallpaperControllerImpl::GetWallpaperFromCache(const AccountId& account_id,
                                                    gfx::ImageSkia* image) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end() && !it->second.second.isNull()) {
    *image = it->second.second;
    return true;
  }
  return false;
}

bool WallpaperControllerImpl::GetPathFromCache(const AccountId& account_id,
                                               base::FilePath* path) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end()) {
    *path = it->second.first;
    return true;
  }
  return false;
}

void WallpaperControllerImpl::AddFirstWallpaperAnimationEndCallback(
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

void WallpaperControllerImpl::StartDecodeFromPath(
    const AccountId& account_id,
    const user_manager::UserType user_type,
    const WallpaperInfo& info,
    bool show_wallpaper,
    const base::FilePath& wallpaper_path) {
  if (wallpaper_path.empty()) {
    // Fallback to default if the path is empty.
    wallpaper_cache_map_.erase(account_id);
    SetDefaultWallpaperImpl(user_type, show_wallpaper, base::DoNothing());
    return;
  }

  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), account_id, wallpaper_path,
                     info, show_wallpaper),
      wallpaper_path);
}

void WallpaperControllerImpl::SetClient(WallpaperControllerClient* client) {
  wallpaper_controller_client_ = client;
  pref_manager_->SetClient(client);
  variant_info_fetcher_.SetClient(client);
  google_photos_wallpaper_manager_.SetClient(client);
}

void WallpaperControllerImpl::SetDriveFsDelegate(
    std::unique_ptr<WallpaperDriveFsDelegate> drivefs_delegate) {
  DCHECK(!drivefs_delegate_);
  drivefs_delegate_ = std::move(drivefs_delegate);
}

void WallpaperControllerImpl::Init(
    const base::FilePath& user_data_path,
    const base::FilePath& chromeos_wallpapers_path,
    const base::FilePath& chromeos_custom_wallpapers_path,
    const base::FilePath& device_policy_wallpaper_path) {
  SetGlobalUserDataDir(user_data_path);
  SetGlobalChromeOSWallpapersDir(chromeos_wallpapers_path);
  SetGlobalChromeOSGooglePhotosWallpapersDir(
      chromeos_wallpapers_path.Append("google_photos/"));
  SetGlobalChromeOSSeaPenWallpaperDir(chromeos_wallpapers_path.Append(
      wallpaper_constants::kSeaPenWallpaperDirName));
  SetGlobalChromeOSCustomWallpapersDir(chromeos_custom_wallpapers_path);
  SetDevicePolicyWallpaperPath(device_policy_wallpaper_path);
}

bool WallpaperControllerImpl::CanSetUserWallpaper(
    const AccountId& account_id) const {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode()) {
    return false;
  }
  // Don't allow user wallpapers while policy is in effect.
  if (IsWallpaperControlledByPolicy(account_id)) {
    return false;
  }
  return true;
}

void WallpaperControllerImpl::SetCustomWallpaper(
    const AccountId& account_id,
    const base::FilePath& file_path,
    WallpaperLayout layout,
    bool preview_mode,
    SetWallpaperCallback callback) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  if (!CanSetUserWallpaper(account_id)) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kCustomized, SetWallpaperResult::kPermissionDenied);
    // Return early to skip the work of decoding.
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::SetDecodedCustomWallpaper,
                     set_wallpaper_weak_factory_.GetWeakPtr(), account_id,
                     file_path.BaseName().value(), layout, preview_mode,
                     std::move(callback), file_path.value()),
      file_path);
}

void WallpaperControllerImpl::SetDecodedCustomWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    WallpaperLayout layout,
    bool preview_mode,
    SetWallpaperCallback callback,
    const std::string& file_path,
    const gfx::ImageSkia& image) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  if (image.isNull() || !CanSetUserWallpaper(account_id)) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kCustomized, SetWallpaperResult::kDecodingError);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnUserSetWallpaper(account_id);
  }
  wallpaper_metrics_manager_->LogWallpaperResult(WallpaperType::kCustomized,
                                                 SetWallpaperResult::kSuccess);

  // Run callback before finishing setting the image. This is the same timing of
  // success callback, then |WallpaperControllerObserver::OnWallpaperChanged|,
  // when setting online wallpaper and simplifies the logic in observers.
  std::move(callback).Run(/*success=*/true);
  const bool is_active_user = IsActiveUser(account_id);
  if (preview_mode) {
    DCHECK(is_active_user);
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperControllerImpl::SaveAndSetWallpaper, base::Unretained(this),
        account_id, IsEphemeralUser(account_id), file_name, file_path,
        WallpaperType::kCustomized, layout,
        /*show_wallpaper=*/false, image);
    reload_preview_wallpaper_callback_ = base::BindRepeating(
        &WallpaperControllerImpl::ShowWallpaperImage, base::Unretained(this),
        image,
        WallpaperInfo{/*in_location=*/std::string(), layout,
                      WallpaperType::kCustomized, base::Time::Now(), file_path},
        /*preview_mode=*/true, /*is_override=*/false);
    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SaveAndSetWallpaperWithCompletion(
        account_id, IsEphemeralUser(account_id), file_name, file_path,
        WallpaperType::kCustomized, layout,
        /*show_wallpaper=*/is_active_user, image,
        base::BindOnce(
            &WallpaperControllerImpl::SaveWallpaperToDriveFsAndSyncInfo,
            weak_factory_.GetWeakPtr(), account_id));
  }
}

void WallpaperControllerImpl::SetOnlineWallpaper(
    const OnlineWallpaperParams& params,
    SetWallpaperCallback callback) {
  DCHECK(callback);
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  DVLOG(1) << __func__ << " params=" << params;
  if (!CanSetUserWallpaper(params.account_id)) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        params.daily_refresh_enabled ? WallpaperType::kDaily
                                     : WallpaperType::kOnline,
        SetWallpaperResult::kPermissionDenied);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::unique_ptr<WallpaperInfo> new_info = CreateOnlineWallpaperInfo(
      params, GetScheduleForOnlineWallpaper(params.collection_id), __func__);
  if (!new_info) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (current_wallpaper_ &&
      current_wallpaper_->wallpaper_info().MatchesAsset(*new_info)) {
    DVLOG(1) << "Detected no change in online wallpaper";
    std::move(callback).Run(/*success=*/true);
    // Fires resized signal to
    // `PersonalizationAppWallpaperProviderImpl::OnWallpaperResized` to tell the
    // UI to clear the loading state.
    for (auto& observer : observers_) {
      observer.OnWallpaperResized();
    }
    return;
  }

  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();

  for (auto& observer : observers_)
    observer.OnOnlineWallpaperSet(params);

  online_wallpaper_manager_.GetOnlineWallpaper(
      GlobalChromeOSWallpapersDir(), params.account_id, *new_info,
      base::BindOnce(&WallpaperControllerImpl::OnOnlineWallpaperDecoded,
                     set_wallpaper_weak_factory_.GetWeakPtr(),
                     params.account_id, params.preview_mode, *new_info,
                     std::move(callback)));
}

void WallpaperControllerImpl::SetGooglePhotosWallpaper(
    const GooglePhotosWallpaperParams& params,
    WallpaperController::SetWallpaperCallback callback) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      !CanSetUserWallpaper(params.account_id)) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        params.daily_refresh_enabled && !params.id.empty()
            ? WallpaperType::kDailyGooglePhotos
            : WallpaperType::kOnceGooglePhotos,
        SetWallpaperResult::kPermissionDenied);
    std::move(callback).Run(/*success=*/false);
    return;
  }
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();

  if (params.daily_refresh_enabled) {
    // If `params.id` is empty, then we are disabling Daily Refresh, so we set
    // the currently shown wallpaper as a `WallpaperType::kOnceGooglePhotos`
    // Wallpaper.
    if (params.id.empty()) {
      WallpaperInfo info;
      if (!GetUserWallpaperInfo(params.account_id, &info) ||
          info.type != WallpaperType::kDailyGooglePhotos) {
        LOG(ERROR) << "Failed to get wallpaper info when disabling google "
                      "photos daily refresh.";
        wallpaper_metrics_manager_->LogWallpaperResult(
            WallpaperType::kOnceGooglePhotos,
            SetWallpaperResult::kInvalidState);
        std::move(callback).Run(false);
        return;
      }

      std::move(callback).Run(true);

      info.collection_id = std::string();
      info.type = WallpaperType::kOnceGooglePhotos;
      SetUserWallpaperInfo(params.account_id, info);
      return;
    } else {
      wallpaper_controller_client_->FetchDailyGooglePhotosPhoto(
          params.account_id, params.id,
          base::BindOnce(
              &WallpaperControllerImpl::OnDailyGooglePhotosPhotoFetched,
              set_wallpaper_weak_factory_.GetWeakPtr(), params,
              std::move(callback)));
    }
  } else {
    wallpaper_controller_client_->FetchGooglePhotosPhoto(
        params.account_id, params.id,
        base::BindOnce(&WallpaperControllerImpl::OnGooglePhotosPhotoFetched,
                       set_wallpaper_weak_factory_.GetWeakPtr(), params,
                       std::move(callback)));
  }
}

void WallpaperControllerImpl::SetGooglePhotosDailyRefreshAlbumId(
    const AccountId& account_id,
    const std::string& album_id) {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info)) {
    LOG(ERROR) << __func__ << " Failed to get user wallpaper info.";
    return;
  }

  // If daily refresh is being enabled.
  if (!album_id.empty()) {
    info.type = WallpaperType::kDailyGooglePhotos;
    info.collection_id = album_id;
  }

  // If Daily Refresh is disabled without selecting another wallpaper, we should
  // keep the current wallpaper and change to type
  // `WallpaperType::kOnceGooglePhotos`, so daily refreshes stop.
  if (album_id.empty() && info.type == WallpaperType::kDailyGooglePhotos) {
    info.type = WallpaperType::kOnceGooglePhotos;
  }
  SetUserWallpaperInfo(account_id, info);
}

std::string WallpaperControllerImpl::GetGooglePhotosDailyRefreshAlbumId(
    const AccountId& account_id) const {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info))
    return std::string();
  if (info.type != WallpaperType::kDailyGooglePhotos)
    return std::string();
  return info.collection_id;
}

bool WallpaperControllerImpl::SetDailyGooglePhotosWallpaperIdCache(
    const AccountId& account_id,
    const DailyGooglePhotosIdCache& ids) {
  return pref_manager_->SetDailyGooglePhotosWallpaperIdCache(account_id, ids);
}

bool WallpaperControllerImpl::GetDailyGooglePhotosWallpaperIdCache(
    const AccountId& account_id,
    DailyGooglePhotosIdCache& ids_out) const {
  return pref_manager_->GetDailyGooglePhotosWallpaperIdCache(account_id,
                                                             ids_out);
}

void WallpaperControllerImpl::SetTimeOfDayWallpaper(
    const AccountId& account_id,
    SetWallpaperCallback callback) {
  OnlineWallpaperVariantInfoFetcher::FetchParamsCallback on_fetch =
      base::BindOnce(&WallpaperControllerImpl::OnWallpaperVariantsFetched,
                     set_wallpaper_weak_factory_.GetWeakPtr(),
                     WallpaperType::kOnline, std::move(callback));
  variant_info_fetcher_.FetchTimeOfDayWallpaper(
      account_id, wallpaper_constants::kDefaultTimeOfDayWallpaperUnitId,
      std::move(on_fetch));
}

bool WallpaperControllerImpl::IsTimeOfDayWallpaper() const {
  return current_wallpaper_ &&
         ::ash::IsTimeOfDayWallpaper(
             current_wallpaper_->wallpaper_info().collection_id);
}

void WallpaperControllerImpl::SetDefaultWallpaper(
    const AccountId& account_id,
    bool show_wallpaper,
    SetWallpaperCallback callback) {
  if (!CanSetUserWallpaper(account_id)) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDefault, SetWallpaperResult::kPermissionDenied);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  RemoveUserWallpaper(account_id, /*on_removed=*/base::DoNothing());
  if (!SetDefaultWallpaperInfo(account_id, base::Time::Now())) {
    LOG(ERROR) << "Initializing user wallpaper info fails. This should never "
                  "happen except in tests.";
  }
  if (show_wallpaper) {
    wallpaper_cache_map_.erase(account_id);
    SetDefaultWallpaperImpl(GetUserType(account_id), /*show_wallpaper=*/true,
                            std::move(callback));
  } else {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDefault, SetWallpaperResult::kSuccess);
    std::move(callback).Run(/*success=*/true);
  }
}

base::FilePath WallpaperControllerImpl::GetDefaultWallpaperPath(
    user_manager::UserType user_type) {
  const bool use_small =
      (GetAppropriateResolution() == WallpaperResolution::kSmall);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // The wallpaper is determined in the following order:
  // Guest wallpaper, child wallpaper, customized default wallpaper, and regular
  // default wallpaper.
  if (user_type == user_manager::UserType::kGuest) {
    const std::string_view switch_string = use_small
                                               ? switches::kGuestWallpaperSmall
                                               : switches::kGuestWallpaperLarge;
    return command_line->GetSwitchValuePath(switch_string);
  } else if (user_type == user_manager::UserType::kChild) {
    const std::string_view switch_string = use_small
                                               ? switches::kChildWallpaperSmall
                                               : switches::kChildWallpaperLarge;
    return command_line->GetSwitchValuePath(switch_string);
  } else if (!customized_default_small_path_.empty()) {
    DCHECK(!customized_default_large_path_.empty());
    return use_small ? customized_default_small_path_
                     : customized_default_large_path_;
  } else {
    const std::string_view switch_string =
        use_small ? switches::kDefaultWallpaperSmall
                  : switches::kDefaultWallpaperLarge;
    return command_line->GetSwitchValuePath(switch_string);
  }
}

void WallpaperControllerImpl::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  customized_default_small_path_ = customized_default_small_path;
  customized_default_large_path_ = customized_default_large_path;

  // If the current wallpaper has type `WallpaperType::kDefault`, the new
  // customized default wallpaper should be shown immediately to update the
  // screen. It shouldn't replace wallpapers of other types.
  bool show_wallpaper = (GetWallpaperType() == WallpaperType::kDefault);

  // Customized default wallpapers are subject to the same restrictions as other
  // default wallpapers, e.g. they should not be set during guest sessions.
  // This should ONLY be called from OOBE where there should not be an active
  // session.
  auto* active_user_session = GetActiveUserSession();
  // Login does not have an active session and the expected behavior is that of
  // a regular user.
  user_manager::UserType user_type = user_manager::UserType::kRegular;
  if (active_user_session) {
    // We expect that this finishes before the user has logged in.
    LOG(WARNING) << "Set customized default wallpaper after login";
    user_type = active_user_session->user_info.type;
  }

  SetDefaultWallpaperImpl(user_type, show_wallpaper, base::DoNothing());
}

void WallpaperControllerImpl::SetPolicyWallpaper(
    const AccountId& account_id,
    user_manager::UserType user_type,
    const std::string& data) {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode())
    return;

  DCHECK(user_type == user_manager::UserType::kRegular ||
         user_type == user_manager::UserType::kPublicAccount);

  // Updates the screen only when the user with this account_id has logged in.
  const bool show_wallpaper = IsActiveUser(account_id);

  if (bypass_decode_for_testing_) {
    OnPolicyWallpaperDecoded(account_id, user_type, show_wallpaper,
                             CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }

  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  image_util::DecodeImageData(
      base::BindOnce(&WallpaperControllerImpl::OnPolicyWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), account_id, user_type,
                     show_wallpaper),
      data_decoder::mojom::ImageCodec::kDefault, data);
}

void WallpaperControllerImpl::OnPolicyWallpaperDecoded(
    const AccountId& account_id,
    user_manager::UserType user_type,
    bool show_wallpaper,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kPolicy, SetWallpaperResult::kDecodingError);
    return;
  }
  wallpaper_metrics_manager_->LogWallpaperResult(WallpaperType::kPolicy,
                                                 SetWallpaperResult::kSuccess);
  SaveAndSetWallpaper(account_id, IsEphemeralUser(account_id),
                      kPolicyWallpaperFile, /*file_path=*/"",
                      WallpaperType::kPolicy, WALLPAPER_LAYOUT_CENTER_CROPPED,
                      show_wallpaper, image);
}

void WallpaperControllerImpl::SetDevicePolicyWallpaperPath(
    const base::FilePath& device_policy_wallpaper_path) {
  const bool was_device_policy_wallpaper_enforced =
      !device_policy_wallpaper_path_.empty();
  device_policy_wallpaper_path_ = device_policy_wallpaper_path;
  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
  } else if (was_device_policy_wallpaper_enforced &&
             device_policy_wallpaper_path.empty()) {
    // If the device wallpaper policy is cleared, the wallpaper should revert to
    // the wallpaper of the current user with the large pod in the users list in
    // the login screen. If there is no such user, use the first user in the
    // users list.
    // TODO(xdai): Get the account id from the session controller and then call
    // ShowUserWallpaper() to display it.
  }
}

bool WallpaperControllerImpl::SetThirdPartyWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    WallpaperLayout layout,
    const gfx::ImageSkia& image) {
  bool allowed_to_set_wallpaper = CanSetUserWallpaper(account_id);
  bool allowed_to_show_wallpaper = IsActiveUser(account_id);

  if (image.isNull()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kThirdParty, SetWallpaperResult::kFileNotFound);
    return false;
  }

  if (allowed_to_set_wallpaper) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kThirdParty, SetWallpaperResult::kSuccess);
    SaveAndSetWallpaperWithCompletion(
        account_id, IsEphemeralUser(account_id), file_name,
        /*file_path=*/"", WallpaperType::kCustomized, layout,
        allowed_to_show_wallpaper, image,
        base::BindOnce(
            &WallpaperControllerImpl::SaveWallpaperToDriveFsAndSyncInfo,
            weak_factory_.GetWeakPtr(), account_id));
  } else {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kThirdParty, SetWallpaperResult::kPermissionDenied);
  }
  return allowed_to_set_wallpaper && allowed_to_show_wallpaper;
}

void WallpaperControllerImpl::SetSeaPenWallpaper(
    const AccountId& account_id,
    const uint32_t image_id,
    const bool preview_mode,
    SetWallpaperCallback callback) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  if (!CanSetUserWallpaper(account_id)) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kSeaPen, SetWallpaperResult::kPermissionDenied);
    // Return early to skip the work of decoding.
    std::move(callback).Run(/*success=*/false);
    return;
  }

  sea_pen_wallpaper_manager_.TouchFile(account_id, image_id);

  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  sea_pen_wallpaper_manager_.GetImage(
      account_id, image_id,
      base::BindOnce(&WallpaperControllerImpl::OnSeaPenWallpaperDecoded,
                     set_wallpaper_weak_factory_.GetWeakPtr(), account_id,
                     image_id, preview_mode, std::move(callback)));
}

void WallpaperControllerImpl::ConfirmPreviewWallpaper() {
  if (!confirm_preview_wallpaper_callback_) {
    DCHECK(!reload_preview_wallpaper_callback_);
    return;
  }
  std::move(confirm_preview_wallpaper_callback_).Run();
  reload_preview_wallpaper_callback_.Reset();

  // Ensure shield is applied after confirming the preview wallpaper.
  if (ShouldApplyShield())
    RepaintWallpaper();

  for (auto& observer : observers_)
    observer.OnWallpaperPreviewEnded();
}

void WallpaperControllerImpl::CancelPreviewWallpaper() {
  if (!confirm_preview_wallpaper_callback_) {
    return;
  }
  confirm_preview_wallpaper_callback_.Reset();
  reload_preview_wallpaper_callback_.Reset();
  ReloadWallpaper(/*clear_cache=*/false);
  for (auto& observer : observers_)
    observer.OnWallpaperPreviewEnded();
}

void WallpaperControllerImpl::UpdateCurrentWallpaperLayout(
    const AccountId& account_id,
    WallpaperLayout layout) {
  // This method has a very specific use case: the user should be active and
  // have a custom wallpaper.
  if (!IsActiveUser(account_id))
    return;

  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info) ||
      ((info.type != WallpaperType::kCustomized) &&
       (info.type != WallpaperType::kOnceGooglePhotos))) {
    return;
  }
  if (info.layout == layout)
    return;

  info.layout = layout;
  if (!SetUserWallpaperInfo(account_id, info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }
  ShowUserWallpaper(account_id);
}

void WallpaperControllerImpl::ShowUserWallpaper(const AccountId& account_id) {
  ShowUserWallpaper(account_id, GetUserType(account_id));
}

void WallpaperControllerImpl::ShowUserWallpaper(
    const AccountId& account_id,
    const user_manager::UserType user_type) {
  current_account_id_ = account_id;
  if (user_type == user_manager::UserType::kKioskApp ||
      user_type == user_manager::UserType::kWebKioskApp ||
      user_type == user_manager::UserType::kKioskIWA) {
    return;
  }

  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
    return;
  }

  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info)) {
    if (!SetDefaultWallpaperInfo(account_id, base::Time::Min()))
      return;
    GetUserWallpaperInfo(account_id, &info);
  }

  // For ephemeral users, the cache is the only place to access their wallpaper
  // because it is not saved to disk. If the image doesn't exist in cache, it
  // means the user's wallpaper type is default (i.e. the user never sets their
  // own wallpaper), and it's a bug if it's not.
  //
  // For regular users, the image will be read from disk if the cache is not
  // hit (e.g. when the first time the wallpaper is shown on login screen).
  gfx::ImageSkia user_wallpaper;
  if (GetWallpaperFromCache(account_id, &user_wallpaper)) {
    ShowWallpaperImage(user_wallpaper, info, /*preview_mode=*/false,
                       /*is_override=*/false);
    return;
  }

  if (info.type == WallpaperType::kDefault) {
    session_manager::SessionState session_state =
        Shell::Get()->session_controller()->GetSessionState();
    if (session_state == session_manager::SessionState::OOBE) {
      ShowOobeWallpaper();
      return;
    }
    wallpaper_cache_map_.erase(account_id);
    SetDefaultWallpaperImpl(user_type, /*show_wallpaper=*/true,
                            base::DoNothing());
    return;
  }

  if (IsOnlineWallpaper(info.type) ||
      info.type == WallpaperType::kOnceGooglePhotos ||
      info.type == WallpaperType::kDailyGooglePhotos ||
      info.type == WallpaperType::kSeaPen) {
    // Load wallpaper according to WallpaperInfo.
    SetWallpaperFromInfo(account_id, info);
    return;
  }

  CHECK(info.type == WallpaperType::kCustomized ||
        info.type == WallpaperType::kPolicy)
      << " Got unhandled wallpaper type=" << base::to_underlying(info.type);

  std::string sub_dir = GetCustomWallpaperSubdirForCurrentResolution();
  base::FilePath wallpaper_path =
      GetCustomWallpaperDir(sub_dir).Append(info.location);

  // Do not try to load the wallpaper if the path is the same, since loading
  // could still be in progress. We ignore the existence of the image.
  base::FilePath cached_wallpaper_path;
  if (GetPathFromCache(account_id, &cached_wallpaper_path) &&
      cached_wallpaper_path == wallpaper_path) {
    return;
  }

  // Set the new path and reset the existing image - the image will be
  // added once it becomes available.
  wallpaper_cache_map_[account_id] =
      CustomWallpaperElement(wallpaper_path, gfx::ImageSkia());

  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PathWithFallback, account_id, info, wallpaper_path),
      base::BindOnce(&WallpaperControllerImpl::StartDecodeFromPath,
                     weak_factory_.GetWeakPtr(), account_id, user_type, info,
                     /*show_wallpaper=*/true));
}

void WallpaperControllerImpl::ShowSigninWallpaper() {
  current_account_id_ = EmptyAccountId();
  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
    return;
  }

  if (IsOobeState()) {
    ShowOobeWallpaper();
    return;
  }

  // If we don't have a user, use the regular default.
  SetDefaultWallpaperImpl(user_manager::UserType::kRegular,
                          /*show_wallpaper=*/true, base::DoNothing());
}

void WallpaperControllerImpl::ShowOneShotWallpaper(
    const gfx::ImageSkia& image) {
  const WallpaperInfo info = {/*in_location=*/std::string(),
                              WallpaperLayout::WALLPAPER_LAYOUT_STRETCH,
                              WallpaperType::kOneShot, base::Time::Now()};
  ShowWallpaperImage(image, info, /*preview_mode=*/false,
                     /*is_override=*/false);
}

void WallpaperControllerImpl::ShowOverrideWallpaper(
    const base::FilePath& image_path,
    bool always_on_top) {
  is_always_on_top_wallpaper_ = always_on_top;
  is_override_wallpaper_ = true;
  const WallpaperInfo info = {/*in_location=*/std::string(),
                              WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
                              WallpaperType::kOneShot, base::Time::Now()};
  ReparentWallpaper();
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnOverrideWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), info),
      image_path);
}

void WallpaperControllerImpl::RemoveOverrideWallpaper() {
  if (!is_override_wallpaper_) {
    DCHECK(!reload_override_wallpaper_callback_);
    return;
  }
  is_always_on_top_wallpaper_ = false;
  is_override_wallpaper_ = false;
  reload_override_wallpaper_callback_.Reset();
  ReparentWallpaper();
  // Forget current wallpaper data.
  current_wallpaper_.reset();
  ReloadWallpaper(/*clear_cache=*/false);
}

void WallpaperControllerImpl::RemoveUserWallpaper(
    const AccountId& account_id,
    base::OnceClosure on_removed) {
  wallpaper_cache_map_.erase(account_id);
  pref_manager_->RemoveUserWallpaperInfo(account_id);
  RemoveUserWallpaperImpl(account_id, std::move(on_removed));
}

void WallpaperControllerImpl::RemovePolicyWallpaper(
    const AccountId& account_id) {
  if (!IsWallpaperControlledByPolicy(account_id))
    return;

  // Updates the screen only when the user has logged in.
  const bool show_wallpaper =
      Shell::Get()->session_controller()->IsActiveUserSessionStarted();
  // Removes the wallpaper info so that the user is no longer policy controlled,
  // otherwise setting default wallpaper is not allowed.
  pref_manager_->RemoveUserWallpaperInfo(account_id);
  SetDefaultWallpaper(account_id, show_wallpaper, base::DoNothing());
}

void WallpaperControllerImpl::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  animation_duration_ = animation_duration;
}

void WallpaperControllerImpl::OpenWallpaperPickerIfAllowed() {
  const auto* session = GetActiveUserSession();
  if (wallpaper_controller_client_ && session &&
      CanSetUserWallpaper(session->user_info.account_id)) {
    wallpaper_controller_client_->OpenWallpaperPicker();
  }
}

void WallpaperControllerImpl::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  if (!window_state_manager_)
    window_state_manager_ = std::make_unique<WallpaperWindowStateManager>();

  window_state_manager_->MinimizeInactiveWindows(user_id_hash);
}

void WallpaperControllerImpl::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  if (!window_state_manager_) {
    DVLOG(1) << "No minimized window state saved";
    return;
  }
  window_state_manager_->RestoreMinimizedWindows(user_id_hash);
}

void WallpaperControllerImpl::AddObserver(
    WallpaperControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperControllerImpl::RemoveObserver(
    WallpaperControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::ImageSkia WallpaperControllerImpl::GetWallpaperImage() {
  return GetWallpaper();
}

void WallpaperControllerImpl::LoadPreviewImage(
    LoadPreviewImageCallback callback) {
  if (!current_wallpaper_) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto image = current_wallpaper_->image();
  image.MakeThreadSafe();
  if (!IsOnlineWallpaper(current_wallpaper_->wallpaper_info().type)) {
    sequenced_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&EncodeAndResizeImage, image),
        std::move(callback));
    return;
  }

  auto variants = current_wallpaper_->wallpaper_info().variants;
  auto it =
      base::ranges::find(variants, backdrop::Image::IMAGE_TYPE_PREVIEW_MODE,
                         &OnlineWallpaperVariant::type);
  // No image with |backdrop::Image::IMAGE_TYPE_PREVIEW_MODE|, fallback to
  // |resized|.
  if (it == variants.end()) {
    sequenced_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&EncodeAndResizeImage, image),
        std::move(callback));
    return;
  }

  const auto& preview_variant = *it;
  wallpaper_file_manager_->LoadOnlineWallpaperPreview(
      GlobalChromeOSWallpapersDir(), preview_variant.raw_url,
      std::move(callback));
}

bool WallpaperControllerImpl::IsWallpaperBlurredForLockState() const {
  return blur_manager_->is_wallpaper_blurred_for_lock_state();
}

bool WallpaperControllerImpl::IsActiveUserWallpaperControlledByPolicy() {
  const UserSession* const active_user_session = GetActiveUserSession();
  if (!active_user_session)
    return false;
  return IsWallpaperControlledByPolicy(
      active_user_session->user_info.account_id);
}

bool WallpaperControllerImpl::IsWallpaperControlledByPolicy(
    const AccountId& account_id) const {
  WallpaperInfo info;
  return GetUserWallpaperInfo(account_id, &info) &&
         info.type == WallpaperType::kPolicy;
}

std::optional<WallpaperInfo>
WallpaperControllerImpl::GetActiveUserWallpaperInfo() const {
  WallpaperInfo info;
  const UserSession* const active_user_session = GetActiveUserSession();
  if (!active_user_session) {
    return std::nullopt;
  }
  return GetWallpaperInfoForAccountId(
      active_user_session->user_info.account_id);
}

std::optional<WallpaperInfo>
WallpaperControllerImpl::GetWallpaperInfoForAccountId(
    const AccountId& account_id) const {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info)) {
    return std::nullopt;
  }
  return info;
}

void WallpaperControllerImpl::OnDidApplyDisplayChanges() {
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ == max_display_size)
    return;

  current_max_display_size_ = max_display_size;
  if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_) {
    timer_.Stop();
    GetInternalDisplayCompositorLock();
    timer_.Start(
        FROM_HERE, wallpaper_reload_delay_,
        base::BindOnce(&WallpaperControllerImpl::ReloadWallpaper,
                       weak_factory_.GetWeakPtr(), /*clear_cache=*/false));
  }
}

void WallpaperControllerImpl::OnRootWindowAdded(aura::Window* root_window) {
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

  UpdateWallpaperForRootWindow(root_window, /*lock_state_changed=*/false,
                               /*new_root=*/true);
}

void WallpaperControllerImpl::OnShellInitialized() {
  auto* shell = Shell::Get();
  shell->overview_controller()->AddObserver(this);
  shell->dark_light_mode_controller()->AddCheckpointObserver(this);
  daily_refresh_scheduler_ = std::make_unique<WallpaperDailyRefreshScheduler>();
  time_of_day_scheduler_ = std::make_unique<WallpaperTimeOfDayScheduler>();
  if (!time_of_day_scheduler_observation_.IsObserving()) {
    time_of_day_scheduler_observation_.Observe(time_of_day_scheduler_.get());
  }
  if (!daily_refresh_observation_.IsObserving()) {
    daily_refresh_observation_.Observe(daily_refresh_scheduler_.get());
  }
}

void WallpaperControllerImpl::OnShellDestroying() {
  auto* shell = Shell::Get();
  shell->overview_controller()->RemoveObserver(this);
  shell->dark_light_mode_controller()->RemoveCheckpointObserver(this);
  daily_refresh_observation_.Reset();
  time_of_day_scheduler_observation_.Reset();
}

void WallpaperControllerImpl::SaveMigratedWallpaperInfo(
    const std::optional<WallpaperInfo>& migrated_info) {
  AccountId account_id = GetActiveAccountId();

  if (migrated_info) {
    // Migration succeeded, save the migrated info.
    pref_manager_->SetLocalWallpaperInfo(account_id, *migrated_info);
  } else {
    LOG(ERROR) << "Wallpaper info migration failed for account " << account_id;
  }
  HandleWallpaperInfoAfterMigration(account_id);
}

void WallpaperControllerImpl::HandleWallpaperInfoAfterMigration(
    const AccountId& account_id) {
  WallpaperInfo local_info;
  bool has_local_info =
      pref_manager_->GetLocalWallpaperInfo(account_id, &local_info);
  bool should_set_time_of_day_wallpaper =
      IsOobeState() && has_local_info &&
      local_info.type == WallpaperType::kDefault &&
      features::IsTimeOfDayWallpaperEnabled();
  if (should_set_time_of_day_wallpaper) {
    DVLOG(0) << __func__ << " Setting default time of day wallpaper.";
    // Sets the time of day wallpaper as the default wallpaper on active user
    // pref changed during OOBE flow.
    SetTimeOfDayWallpaper(
        account_id,
        base::BindOnce(
            &WallpaperControllerImpl::OnTimeOfDayWallpaperSetAfterOobe,
            weak_factory_.GetWeakPtr()));
  }

  if (wallpaper_controller_client_->IsWallpaperSyncEnabled(account_id)) {
    WallpaperInfo synced_info;
    bool has_synced_info =
        pref_manager_->GetSyncedWallpaperInfo(account_id, &synced_info);
    DVLOG(1) << __func__ << " has_synced_info=" << has_synced_info;
    if (!has_synced_info && has_local_info &&
        WallpaperPrefManager::ShouldSyncOut(local_info)) {
      if (local_info.type == WallpaperType::kCustomized) {
        base::FilePath source = GetCustomWallpaperDir(kOriginalWallpaperSubDir)
                                    .Append(local_info.location);
        SaveWallpaperToDriveFsAndSyncInfo(account_id, source);
      } else {
        pref_manager_->SetSyncedWallpaperInfo(account_id, local_info);
      }
    }

    // Starts watching for sync pref changes.
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(
        Shell::Get()->session_controller()->GetActivePrefService());
    pref_change_registrar_->Add(
        WallpaperPrefManager::GetSyncPrefName(),
        base::BindRepeating(&WallpaperControllerImpl::SyncLocalAndRemotePrefs,
                            weak_factory_.GetWeakPtr(), account_id));
    SyncLocalAndRemotePrefs(account_id);
  }

  // Sends signal for daily refresh check.
  OnCheckpointChanged(daily_refresh_scheduler_.get(),
                      daily_refresh_scheduler_->current_checkpoint());
}

void WallpaperControllerImpl::HandleSyncedWallpaperInfoAfterMigration(
    const AccountId& account_id,
    const std::optional<WallpaperInfo>& synced_info) {
  if (!synced_info) {
    LOG(WARNING) << __func__
                 << " Unable to migrate synced info to the latest version";
    return;
  }

  WallpaperInfo local_info;
  if (!pref_manager_->GetLocalWallpaperInfo(account_id, &local_info)) {
    HandleWallpaperInfoSyncedIn(account_id, *synced_info);
    return;
  }
  // TODO(b/278096886): Move this sync-out logic for `kCustomized` type
  // somewhere else.
  if (!synced_info->MatchesSelection(local_info) &&
      synced_info->date < local_info.date &&
      local_info.type == WallpaperType::kCustomized) {
    // Generally, we handle setting synced_info when local_info is updated.
    // But for custom images, we wait until the image is uploaded to Drive,
    // which may not be available at the time of setting the local_info.
    base::FilePath source = GetCustomWallpaperDir(kOriginalWallpaperSubDir)
                                .Append(local_info.location);
    SaveWallpaperToDriveFsAndSyncInfo(account_id, source);
    return;
  }

  if (!WallpaperPrefManager::ShouldSyncIn(*synced_info, local_info,
                                          IsOobeState())) {
    return;
  }
  HandleWallpaperInfoSyncedIn(account_id, *synced_info);
}

void WallpaperControllerImpl::HandleDeprecatedSyncedWallpaperInfoAfterMigration(
    const AccountId& account_id,
    const std::optional<WallpaperInfo>& synced_info) {
  if (!synced_info) {
    LOG(WARNING) << __func__
                 << " Unable to migrate synced info to the latest version";
    return;
  }

  // Clears the deprecated pref to prevent further sync in the future.
  pref_manager_->ClearDeprecatedPref(account_id);
  HandleSyncedWallpaperInfoAfterMigration(account_id, *synced_info);
}

void WallpaperControllerImpl::OnWallpaperResized() {
  CalculateWallpaperColors();
  compositor_lock_.reset();
  for (auto& observer : observers_) {
    observer.OnWallpaperResized();
  }
}

void WallpaperControllerImpl::OnColorCalculationComplete(
    const WallpaperInfo& info,
    const WallpaperCalculatedColors& wallpaper_calculated_colors) {
  // Since we delete `color_calculator_` in `ShowWallpaperImage()`,
  // `current_wallpaper_` should always be the same as the wallpaper for which
  // color computation has been completed in production.
  DCHECK(current_wallpaper_->wallpaper_info().MatchesAsset(info));

  // Use |WallpaperInfo::location| as the key for storing |prominent_colors_| in
  // the |kWallpaperColors| pref.
  pref_manager_->CacheKMeanColor(info.location,
                                 wallpaper_calculated_colors.k_mean_color);
  pref_manager_->CacheCelebiColor(info.location,
                                  wallpaper_calculated_colors.celebi_color);
  SetCalculatedColors(wallpaper_calculated_colors);

  // Release the color calculator after it has returned a result by calling this
  // callback. There is only ever one calculator and it should always be the one
  // which is fulfilling this callback.
  color_calculator_.reset();
}

void WallpaperControllerImpl::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // It is possible to switch to another user when preview is on. In this case,
  // we should close the preview and show the user's actual wallpaper.
  MaybeClosePreviewWallpaper();

  // We check to make sure the user's Google Photos wallpapers are still valid
  // here, otherwise switching back and forth between users constantly would
  // prevent us from ever checking.
  WallpaperInfo info;
  pref_manager_->GetLocalWallpaperInfo(account_id, &info);
  if (info.type == WallpaperType::kOnceGooglePhotos)
    CheckGooglePhotosStaleness(account_id, info);
}

void WallpaperControllerImpl::OnOobeDialogStateChanged(OobeDialogState state) {
  oobe_state_ = state;
}

void WallpaperControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Replace the device policy wallpaper with a user wallpaper if necessary.
  if (IsDevicePolicyWallpaper() && !ShouldSetDevicePolicyWallpaper())
    ReloadWallpaper(/*clear_cache=*/false);

  // Replace the oobe wallpaper with a user wallpaper if necessary.
  if (IsOobeWallpaper()) {
    ReloadWallpaper(/*clear_cache=*/false);
  }

  CalculateWallpaperColors();

  is_session_active_ = state == session_manager::SessionState::ACTIVE;

  // The wallpaper may be dimmed/blurred based on session state. The color of
  // the dimming overlay depends on the prominent color cached from a previous
  // calculation, or a default color if cache is not available. It should never
  // depend on any in-flight color calculation.
  if (wallpaper_mode_ == WALLPAPER_IMAGE &&
      (state == session_manager::SessionState::ACTIVE ||
       state == session_manager::SessionState::LOCKED ||
       state == session_manager::SessionState::LOGIN_SECONDARY)) {
    UpdateWallpaperForAllRootWindows(/*lock_state_changed=*/true);
  } else {
    // Just update container.
    ReparentWallpaper();
  }
}

void WallpaperControllerImpl::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing when the tablet state is still in the process of transition.
    return;
  }

  RepaintWallpaper();
}

void WallpaperControllerImpl::OnCheckpointChanged(
    const ScheduledFeature* src,
    const ScheduleCheckpoint new_checkpoint) {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return;
  }
  AccountId account_id = GetActiveAccountId();
  WallpaperInfo info;
  if (!pref_manager_->GetUserWallpaperInfo(account_id, &info)) {
    return;
  }

  if (src == daily_refresh_scheduler_.get()) {
    DVLOG(1) << __func__ << " notified by daily_refresh_scheduler_";
    for (auto& observer : observers_) {
      observer.OnDailyRefreshCheckpointChanged();
    }
    if (daily_refresh_scheduler_->ShouldRefreshWallpaper(info)) {
      UpdateDailyRefreshWallpaper();
    }
    if (info.type == WallpaperType::kOnceGooglePhotos) {
      CheckGooglePhotosStaleness(account_id, info);
    }
    return;
  }

  if (!IsOnlineWallpaper(info.type) ||
      src != &GetScheduleForOnlineWallpaper(info.collection_id)) {
    return;
  }

  variant_info_fetcher_.FetchOnlineWallpaper(
      account_id, info,
      base::BindOnce(&WallpaperControllerImpl::RepaintOnlineWallpaper,
                     set_wallpaper_weak_factory_.GetWeakPtr()));
}

void WallpaperControllerImpl::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  RepaintWallpaper();
}

void WallpaperControllerImpl::OnOverviewModeWillStart() {
  // Due to visual glitches when overview mode is activated whilst wallpaper
  // preview is active (http://crbug.com/895265), cancel wallpaper preview and
  // close its front-end before toggling overview mode.
  MaybeClosePreviewWallpaper();
}

void WallpaperControllerImpl::OnOverviewModeStarting() {
  // Only in tablet mode, we need to call `RepaintWallpaper` to update the
  // wallpaper shield on overview mode changes, since in clamshell mode, we
  // don't apply the wallpaper shield no matter it's in overview mode or not.
  // However, in tablet mode, we need to apply the wallpaper shield when it's
  // not in the overview mode.
  if (display::Screen::GetScreen()->InTabletMode()) {
    RepaintWallpaper();
  }
}

void WallpaperControllerImpl::OnOverviewModeEnded() {
  // Refer to the comment in `OnOverviewModeStarting`.
  if (display::Screen::GetScreen()->InTabletMode()) {
    RepaintWallpaper();
  }
}

void WallpaperControllerImpl::CompositorLockTimedOut() {
  compositor_lock_.reset();
}

void WallpaperControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  AccountId account_id = GetActiveAccountId();

  // Tests may not initialize global wallpaper dirs before logging in a user.
  if (sea_pen_wallpaper_manager_.ShouldMigrate(account_id) &&
      !GlobalChromeOSSeaPenWallpaperDir().empty()) {
    sea_pen_wallpaper_manager_.Migrate(
        account_id, GetUserSeaPenWallpaperDir(account_id),
        base::BindOnce(&WallpaperControllerImpl::OnSeaPenFilesMigrated,
                       weak_factory_.GetWeakPtr(), account_id));
  }

  WallpaperInfo local_info;
  if (pref_manager_->GetLocalWallpaperInfo(account_id, &local_info) &&
      wallpaper_info_migrator_.ShouldMigrate(local_info)) {
    wallpaper_info_migrator_.Migrate(
        account_id, local_info,
        base::BindOnce(&WallpaperControllerImpl::SaveMigratedWallpaperInfo,
                       weak_factory_.GetWeakPtr()));
  } else {
    // If no migration is needed, proceed as before
    HandleWallpaperInfoAfterMigration(account_id);
  }
}

void WallpaperControllerImpl::ShowDefaultWallpaperForTesting() {
  SetDefaultWallpaperImpl(user_manager::UserType::kRegular,
                          /*show_wallpaper=*/true, base::DoNothing());
}

void WallpaperControllerImpl::CreateEmptyWallpaperForTesting() {
  ResetCalculatedColors();
  current_wallpaper_.reset();
  wallpaper_mode_ = WALLPAPER_IMAGE;
  UpdateWallpaperForAllRootWindows(/*lock_state_changed=*/false);
  // Simulate default color sampling behavior.
  SetCalculatedColors(WallpaperCalculatedColors(
      /*k_means=*/SK_ColorWHITE,
      /*celebi=*/gfx::kGoogleBlue400));
}

void WallpaperControllerImpl::ReloadWallpaperForTesting(bool clear_cache) {
  ReloadWallpaper(clear_cache);
}

void WallpaperControllerImpl::OverrideDriveFsDelegateForTesting(
    std::unique_ptr<WallpaperDriveFsDelegate> drivefs_delegate) {
  CHECK_IS_TEST();
  drivefs_delegate_ = std::move(drivefs_delegate);
}

void WallpaperControllerImpl::UpdateWallpaperForRootWindow(
    aura::Window* root_window,
    bool lock_state_changed,
    bool new_root) {
  DCHECK_EQ(WALLPAPER_IMAGE, wallpaper_mode_);
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  if (lock_state_changed || new_root) {
    wallpaper_widget_controller->Reparent(GetWallpaperContainerId());
  }
  wallpaper_widget_controller->wallpaper_view()->ClearCachedImage();
  const bool changed = blur_manager_->UpdateBlurForRootWindow(
      root_window, lock_state_changed, new_root, GetWallpaperType());
  if (changed) {
    for (auto& observer : observers_) {
      observer.OnWallpaperBlurChanged();
    }
  }
}

void WallpaperControllerImpl::UpdateWallpaperForAllRootWindows(
    bool lock_state_changed) {
  for (aura::Window* root : Shell::GetAllRootWindows())
    UpdateWallpaperForRootWindow(root, lock_state_changed, /*new_root=*/false);
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool WallpaperControllerImpl::ReparentWallpaper() {
  auto container = GetWallpaperContainerId();

  bool moved = false;
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    moved |= root_window_controller->wallpaper_widget_controller()->Reparent(
        container);
  }
  return moved;
}

int WallpaperControllerImpl::GetWallpaperContainerId() {
  if (is_always_on_top_wallpaper_) {
    return kShellWindowId_AlwaysOnTopWallpaperContainer;
  }

  return is_session_active_ ? kShellWindowId_WallpaperContainer
                            : kShellWindowId_LockScreenWallpaperContainer;
}

void WallpaperControllerImpl::RemoveUserWallpaperImpl(
    const AccountId& account_id,
    base::OnceClosure on_removed) {
  if (wallpaper_controller_client_) {
    wallpaper_controller_client_->GetFilesId(
        account_id,
        base::BindOnce(
            &WallpaperControllerImpl::RemoveUserWallpaperImplWithFilesId,
            weak_factory_.GetWeakPtr(), account_id, std::move(on_removed)));
  } else {
    LOG(ERROR) << "Failed to remove wallpaper. wallpaper_controller_client_ no "
                  "longer exists.";
  }
}

void WallpaperControllerImpl::RemoveUserWallpaperImplWithFilesId(
    const AccountId& account_id,
    base::OnceClosure on_removed,
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

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DeleteWallpaperInList, std::move(files_to_remove)),
      std::move(on_removed));
}

void WallpaperControllerImpl::SetDefaultWallpaperImpl(
    user_manager::UserType user_type,
    bool show_wallpaper,
    SetWallpaperCallback callback) {
  // There is no visible wallpaper in kiosk mode.
  if (IsInKioskMode()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  const bool use_small =
      (GetAppropriateResolution() == WallpaperResolution::kSmall);
  WallpaperLayout layout =
      use_small ? WALLPAPER_LAYOUT_CENTER : WALLPAPER_LAYOUT_CENTER_CROPPED;
  base::FilePath file_path = GetDefaultWallpaperPath(user_type);

  // We need to decode the image if there's no cache, or if the file path
  // doesn't match the cached value (i.e. the cache is outdated). Otherwise,
  // directly run the callback with the cached image.
  if (!cached_default_wallpaper_.image.isNull() &&
      cached_default_wallpaper_.file_path == file_path) {
    OnDefaultWallpaperDecoded(file_path, layout, show_wallpaper,
                              std::move(callback),
                              cached_default_wallpaper_.image);
  } else {
    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperControllerImpl::OnDefaultWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), file_path, layout,
                       show_wallpaper, std::move(callback)),
        file_path);
  }
}

bool WallpaperControllerImpl::WallpaperIsAlreadyLoaded(
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

void WallpaperControllerImpl::ReadAndDecodeWallpaper(
    image_util::DecodeImageCallback callback,
    const base::FilePath& file_path) {
  decode_requests_for_testing_.push_back(file_path);
  if (bypass_decode_for_testing_) {
    std::move(callback).Run(CreateSolidColorWallpaper(kDefaultWallpaperColor));
    return;
  }
  image_util::DecodeImageFile(std::move(callback), file_path);
}

bool WallpaperControllerImpl::SetDefaultWallpaperInfo(
    const AccountId& account_id,
    const base::Time& date) {
  const WallpaperInfo info = {/*in_location=*/std::string(),
                              WALLPAPER_LAYOUT_CENTER_CROPPED,
                              WallpaperType::kDefault, date};
  return SetUserWallpaperInfo(account_id, info);
}

void WallpaperControllerImpl::OnWallpaperVariantsFetched(
    WallpaperType type,
    SetWallpaperCallback callback,
    std::optional<OnlineWallpaperParams> params) {
  DCHECK(IsOnlineWallpaper(type));
  if (params) {
    SetOnlineWallpaper(*params, std::move(callback));
    return;
  }

  // Report that setting the wallpaper failed.
  std::move(callback).Run(/*success=*/false);

  // Log setting wallpaper failure due to fetching request failure.
  wallpaper_metrics_manager_->LogWallpaperResult(
      type, SetWallpaperResult::kRequestFailure);
}

void WallpaperControllerImpl::RepaintOnlineWallpaper(
    std::optional<OnlineWallpaperParams> params) {
  if (!params) {
    LOG(ERROR) << "Fetching online variant failed";
    return;
  }

  std::unique_ptr<WallpaperInfo> new_info = CreateOnlineWallpaperInfo(
      *params, GetScheduleForOnlineWallpaper(params->collection_id), __func__);
  if (!new_info) {
    return;
  }

  if (current_wallpaper_ &&
      current_wallpaper_->wallpaper_info().MatchesAsset(*new_info)) {
    DVLOG(1) << "Detected no change in online wallpaper asset";
    return;
  }

  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  online_wallpaper_manager_.GetOnlineWallpaper(
      GlobalChromeOSWallpapersDir(), params->account_id, *new_info,
      base::BindOnce(&WallpaperControllerImpl::OnOnlineWallpaperDecoded,
                     set_wallpaper_weak_factory_.GetWeakPtr(),
                     params->account_id, params->preview_mode, *new_info,
                     /*callback=*/base::DoNothing()));
}

void WallpaperControllerImpl::OnOnlineWallpaperDecoded(
    const AccountId& account_id,
    bool preview_mode,
    WallpaperInfo wallpaper_info,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK(callback);
  if (image.isNull()) {
    std::move(callback).Run(/*success=*/false);
    wallpaper_metrics_manager_->LogWallpaperResult(
        wallpaper_info.type, SetWallpaperResult::kDecodingError);
    LOG(ERROR) << "Failed to decode online wallpaper.";
    return;
  } else {
    for (auto& observer : observers_) {
      observer.OnUserSetWallpaper(account_id);
    }
    wallpaper_metrics_manager_->LogWallpaperResult(
        wallpaper_info.type, SetWallpaperResult::kSuccess);
  }

  const bool is_active_user = IsActiveUser(account_id);
  if (current_wallpaper_ &&
      current_wallpaper_->wallpaper_info().MatchesSelection(wallpaper_info)) {
    DVLOG(1) << "Detected a change in asset for the same wallpaper.";
    // Keep the current wallpaper info date since the wallpaper doesn't change
    // and only one of its variant gets repainted (ex: dark/light or time of day
    // wallpapers).
    wallpaper_info.date = current_wallpaper_->wallpaper_info().date;
  }
  if (preview_mode) {
    DCHECK(is_active_user);
    std::move(callback).Run(/*success=*/true);
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperControllerImpl::SetWallpaperImpl, base::Unretained(this),
        account_id, wallpaper_info, image, /*show_wallpaper=*/false);
    reload_preview_wallpaper_callback_ =
        base::BindRepeating(&WallpaperControllerImpl::ShowWallpaperImage,
                            base::Unretained(this), image, wallpaper_info,
                            /*preview_mode=*/true, /*is_override=*/false);
    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    std::move(callback).Run(/*success=*/true);
    SetWallpaperImpl(account_id, wallpaper_info, image,
                     /*show_wallpaper=*/is_active_user);
  }
}

void WallpaperControllerImpl::ShowOobeWallpaper() {
  base::FilePath file_path;
  if (features::IsBootAnimationEnabled()) {
    file_path = base::FilePath(
        FILE_PATH_LITERAL("/usr/share/chromeos-assets/animated_splash_screen/"
                          "oobe_wallpaper.jpg"));
  } else {
    file_path = GetDefaultWallpaperPath(user_manager::UserType::kRegular);
  }

  if (!cached_oobe_wallpaper_.image.isNull() &&
      cached_oobe_wallpaper_.file_path == file_path) {
    OnOobeWallpaperDecoded(file_path, cached_oobe_wallpaper_.image);
  } else {
    ReadAndDecodeWallpaper(
        base::BindOnce(&WallpaperControllerImpl::OnOobeWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), file_path),
        file_path);
  }
}

void WallpaperControllerImpl::OnOobeWallpaperDecoded(
    const base::FilePath& path,
    const gfx::ImageSkia& image) {
  if (path.empty() || image.isNull()) {
    LOG(ERROR) << "Failed to decode OOBE wallpaper.";
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kOobe, SetWallpaperResult::kDecodingError);
    cached_oobe_wallpaper_.image =
        CreateSolidColorWallpaper(kOobeWallpaperColor);
    cached_oobe_wallpaper_.file_path.clear();
  } else {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kOobe, SetWallpaperResult::kSuccess);
    cached_oobe_wallpaper_.image = image;
    cached_oobe_wallpaper_.file_path = path;
  }

  const bool use_small =
      (GetAppropriateResolution() == WallpaperResolution::kSmall);
  WallpaperLayout layout =
      use_small ? WallpaperLayout::WALLPAPER_LAYOUT_CENTER
                : WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED;

  WallpaperInfo info(cached_oobe_wallpaper_.file_path.value(), layout,
                     WallpaperType::kOobe, base::Time::Now());
  ShowWallpaperImage(cached_oobe_wallpaper_.image, info,
                     /*preview_mode=*/false, /*is_override=*/false);
}

bool WallpaperControllerImpl::IsOobeWallpaper() const {
  return current_wallpaper_ &&
         current_wallpaper_->wallpaper_info().type == WallpaperType::kOobe;
}

void WallpaperControllerImpl::OnGooglePhotosPhotoFetched(
    GooglePhotosWallpaperParams params,
    SetWallpaperCallback callback,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    bool success) {
  // It should be impossible for us to get back a photo successfully from
  // a request that failed.
  DCHECK(success || !photo);
  // If the request failed, there's nothing to do here, since we can't update
  // the wallpaper but also don't want to delete the cache.
  if (!success) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kOnceGooglePhotos, SetWallpaperResult::kRequestFailure);
    std::move(callback).Run(false);
    return;
  }

  if (photo.is_null()) {
    // The photo doesn't exist, or has been deleted. If this photo is the
    // wallpaper for `params.account_id`, we need to reset to the default.
    WallpaperInfo wallpaper_info;
    if (GetUserWallpaperInfo(params.account_id, &wallpaper_info) &&
        wallpaper_info.location == params.id) {
      sequenced_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DeleteGooglePhotosCache, params.account_id));
      wallpaper_cache_map_.erase(params.account_id);
      SetDefaultWallpaper(params.account_id,
                          /*show_wallpaper=*/IsActiveUser(params.account_id),
                          base::DoNothing());
      return;
    }
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kOnceGooglePhotos, SetWallpaperResult::kFileNotFound);
    std::move(callback).Run(false);
    return;
  }

  params.dedup_key = photo->dedup_key;

  google_photos_wallpaper_manager_.GetGooglePhotosWallpaper(
      GetUserGooglePhotosWallpaperDir(params.account_id), params,
      std::move(photo),
      base::BindOnce(&WallpaperControllerImpl::OnGooglePhotosWallpaperDecoded,
                     set_wallpaper_weak_factory_.GetWeakPtr(), params,
                     std::move(callback)));
}

void WallpaperControllerImpl::OnDailyGooglePhotosPhotoFetched(
    const GooglePhotosWallpaperParams& params,
    RefreshWallpaperCallback callback,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    bool success) {
  // It should be impossible for us to get back a photo successfully from
  // a request that failed.
  DCHECK(success || !photo);
  if (!success || photo.is_null()) {
    std::move(callback).Run(false);
    WallpaperInfo info;
    if (GetUserWallpaperInfo(params.account_id, &info) &&
        info.collection_id == params.id) {
      if (success) {
        wallpaper_metrics_manager_->LogWallpaperResult(
            WallpaperType::kDailyGooglePhotos,
            SetWallpaperResult::kFileNotFound);
        // If the request succeeded, but no photos came back, then the album is
        // empty or deleted. Reset to default as a fallback.
        SetDefaultWallpaper(params.account_id,
                            /*show_wallpaper=*/IsActiveUser(params.account_id),
                            base::DoNothing());
      } else {
        wallpaper_metrics_manager_->LogWallpaperResult(
            WallpaperType::kDailyGooglePhotos,
            SetWallpaperResult::kRequestFailure);
      }
    }
    return;
  }

  auto on_load = base::BindOnce(
      &WallpaperControllerImpl::OnDailyGooglePhotosWallpaperDecoded,
      set_wallpaper_weak_factory_.GetWeakPtr(), params.account_id, photo->id,
      params.id, photo->dedup_key, std::move(callback));
  google_photos_wallpaper_manager_.GetGooglePhotosWallpaper(
      GetUserGooglePhotosWallpaperDir(params.account_id), params,
      std::move(photo), std::move(on_load));
}

void WallpaperControllerImpl::OnDailyGooglePhotosWallpaperDecoded(
    const AccountId& account_id,
    const std::string& photo_id,
    const std::string& album_id,
    std::optional<std::string> dedup_key,
    RefreshWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK(callback);
  if (image.isNull()) {
    std::move(callback).Run(false);
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDailyGooglePhotos, SetWallpaperResult::kDecodingError);
    return;
  }
  // Image returned successfully. We can reliably assume success from here, and
  // we need to call the callback before `ShowWallpaperImage()` to ensure proper
  // propagation of `CurrentWallpaper` to the WebUI.
  std::move(callback).Run(/*success=*/true);
  wallpaper_metrics_manager_->LogWallpaperResult(
      WallpaperType::kDailyGooglePhotos, SetWallpaperResult::kSuccess);

  WallpaperInfo wallpaper_info(
      {account_id, album_id, /*daily_refresh_enabled=*/true,
       ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*dedup_key=*/std::nullopt});
  wallpaper_info.location = photo_id;
  wallpaper_info.dedup_key = dedup_key;

  SetWallpaperImpl(account_id, wallpaper_info, image, /*show_wallpaper=*/true);
}

void WallpaperControllerImpl::OnGooglePhotosWallpaperDecoded(
    const GooglePhotosWallpaperParams& params,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK(callback);
  if (image.isNull()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kOnceGooglePhotos, SetWallpaperResult::kDecodingError);
    std::move(callback).Run(false);
    return;
  }
  // Image returned successfully. We can reliably assume success from here, and
  // we need to call the callback before `ShowWallpaperImage` to ensure proper
  // propagation of `CurrentWallpaper` to the WebUI.
  wallpaper_metrics_manager_->LogWallpaperResult(
      WallpaperType::kOnceGooglePhotos, SetWallpaperResult::kSuccess);
  for (auto& observer : observers_) {
    observer.OnUserSetWallpaper(params.account_id);
  }
  std::move(callback).Run(true);

  bool is_active_user = IsActiveUser(params.account_id);
  WallpaperInfo wallpaper_info(params);
  if (params.preview_mode) {
    DCHECK(is_active_user);
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperControllerImpl::SetWallpaperImpl, base::Unretained(this),
        params.account_id, wallpaper_info, image,
        /*show_wallpaper=*/false);
    reload_preview_wallpaper_callback_ =
        base::BindRepeating(&WallpaperControllerImpl::ShowWallpaperImage,
                            base::Unretained(this), image, wallpaper_info,
                            /*preview_mode=*/true, /*is_override=*/false);

    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SetWallpaperImpl(params.account_id, wallpaper_info, image,
                     /*show_wallpaper=*/is_active_user);
  }
}

void WallpaperControllerImpl::SetWallpaperImpl(
    const AccountId& account_id,
    const WallpaperInfo& wallpaper_info,
    const gfx::ImageSkia& image,
    bool show_wallpaper) {
  DCHECK(!image.isNull()) << " image should not be empty";
  if (!SetUserWallpaperInfo(account_id, wallpaper_info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }

  if (show_wallpaper) {
    ShowWallpaperImage(image, wallpaper_info, /*preview_mode=*/false,
                       /*is_override=*/false);
  }

  // Add current Google Photos wallpaper to in-memory cache.
  wallpaper_cache_map_[account_id] =
      CustomWallpaperElement(base::FilePath(), image);
}

void WallpaperControllerImpl::SetWallpaperFromInfo(const AccountId& account_id,
                                                   const WallpaperInfo& info) {
  if (info.type == WallpaperType::kDefault) {
    // Only used by WallpaperControllerTestApi.
    LOG(WARNING) << "Setting a default wallpaper from info for user: "
                 << account_id.Serialize()
                 << " .This should only happen in test.";
    SetDefaultWallpaperImpl(GetUserType(account_id), /*show_wallpaper=*/true,
                            base::DoNothing());
    return;
  }

  DCHECK(!info.location.empty()) << " location should not be empty";
  if (IsOnlineWallpaper(info.type)) {
    auto wallpaper_path = GetOnlineWallpaperFilePath(
        GlobalChromeOSWallpapersDir(), GURL(info.location),
        GetAppropriateResolution());
    // If the wallpaper exists and it already contains the correct image we
    // can return immediately.
    if (current_wallpaper_ &&
        current_wallpaper_->wallpaper_info().MatchesAsset(info)) {
      return;
    }
    // The online wallpaper must be available in the file path at this time and
    // can be loaded from the path.
    wallpaper_file_manager_->LoadWallpaper(
        info.type, GlobalChromeOSWallpapersDir(), info.location,
        base::BindOnce(&WallpaperControllerImpl::OnWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), account_id, wallpaper_path,
                       info, /*show_wallpaper=*/true));
    return;
  }
  if (info.type == WallpaperType::kOnceGooglePhotos ||
      info.type == WallpaperType::kDailyGooglePhotos) {
    auto path =
        GetUserGooglePhotosWallpaperDir(account_id).Append(info.location);
    // The Google Photos wallpaper must be available in the file path at this
    // time and can be loaded from the path.
    wallpaper_file_manager_->LoadWallpaper(
        info.type, GetUserGooglePhotosWallpaperDir(account_id), info.location,
        base::BindOnce(&WallpaperControllerImpl::OnWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), account_id, path, info,
                       /*show_wallpaper=*/true));
    return;
  }
  if (info.type == WallpaperType::kSeaPen) {
    const auto user_sea_pen_wallpaper_dir =
        GetUserSeaPenWallpaperDir(account_id);
    wallpaper_file_manager_->LoadWallpaper(
        WallpaperType::kSeaPen, user_sea_pen_wallpaper_dir, info.location,
        base::BindOnce(&WallpaperControllerImpl::OnWallpaperDecoded,
                       weak_factory_.GetWeakPtr(), account_id,
                       user_sea_pen_wallpaper_dir.Append(info.location)
                           .ReplaceExtension(".jpg"),
                       info, /*show_wallpaper=*/true));
    return;
  }

  LOG(ERROR) << "Wallpaper reverts to default unexpected.";
  wallpaper_cache_map_.erase(account_id);
  SetDefaultWallpaperImpl(GetUserType(account_id), /*show_wallpaper=*/true,
                          base::DoNothing());
}

void WallpaperControllerImpl::OnDefaultWallpaperDecoded(
    const base::FilePath& path,
    WallpaperLayout layout,
    bool show_wallpaper,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDefault, SetWallpaperResult::kDecodingError);
    // Create a solid color wallpaper if the default wallpaper decoding fails.
    cached_default_wallpaper_.image =
        CreateSolidColorWallpaper(kDefaultWallpaperColor);
    cached_default_wallpaper_.file_path.clear();
  } else {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDefault, SetWallpaperResult::kSuccess);
    cached_default_wallpaper_.image = image;
    cached_default_wallpaper_.file_path = path;
  }

  // Setting default wallpaper always succeeds even if the intended image failed
  // decoding. Run the callback before doing the final step of showing the
  // wallpaper to be consistent with other wallpaper controller methods.
  std::move(callback).Run(/*success=*/true);

  if (show_wallpaper) {
    WallpaperInfo info(cached_default_wallpaper_.file_path.value(), layout,
                       WallpaperType::kDefault, base::Time::Now());
    ShowWallpaperImage(cached_default_wallpaper_.image, info,
                       /*preview_mode=*/false, /*is_override=*/false);
  }
}

void WallpaperControllerImpl::OnSeaPenWallpaperDecoded(
    const AccountId& account_id,
    const uint32_t sea_pen_image_id,
    const bool preview_mode,
    SetWallpaperCallback callback,
    const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kSeaPen, SetWallpaperResult::kDecodingError);
    std::move(callback).Run(false);
    return;
  }

  if (IsEphemeralUser(account_id)) {
    DCHECK(features::IsSeaPenDemoModeEnabled());
    // Demo mode users are eligible for SeaPen but should not save the wallpaper
    // image. Set a fake file path to use for the in memory wallpaper cache.
    const auto cache_file_path =
        base::FilePath("in_memory_cache")
            .Append(wallpaper_constants::kSeaPenWallpaperDirName)
            .Append(base::NumberToString(sea_pen_image_id))
            .AddExtension(".jpg");
    OnSeaPenWallpaperSavedToPublic(account_id, image_skia, sea_pen_image_id,
                                   /*preview_mode=*/false, std::move(callback),
                                   cache_file_path);
    return;
  }

  // Save a copy of the currently selected SeaPen wallpaper in the global
  // wallpaper directory so that it is available on lock screen.
  wallpaper_file_manager_->SaveWallpaperToDisk(
      WallpaperType::kSeaPen, GetUserSeaPenWallpaperDir(account_id),
      base::FilePath(base::NumberToString(sea_pen_image_id))
          .AddExtension(".jpg")
          .value(),
      WALLPAPER_LAYOUT_CENTER_CROPPED, image_skia,
      base::BindOnce(&WallpaperControllerImpl::OnSeaPenWallpaperSavedToPublic,
                     set_wallpaper_weak_factory_.GetWeakPtr(), account_id,
                     image_skia, sea_pen_image_id, preview_mode,
                     std::move(callback)));
}

void WallpaperControllerImpl::OnSeaPenWallpaperSavedToPublic(
    const AccountId& account_id,
    const gfx::ImageSkia& image_skia,
    const uint32_t sea_pen_image_id,
    const bool preview_mode,
    SetWallpaperCallback callback,
    const base::FilePath& file_path) {
  if (file_path.empty()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kSeaPen, SetWallpaperResult::kFileNotFound);
    std::move(callback).Run(false);
    return;
  }
  wallpaper_metrics_manager_->LogWallpaperResult(WallpaperType::kSeaPen,
                                                 SetWallpaperResult::kSuccess);
  for (auto& observer : observers_) {
    observer.OnUserSetWallpaper(account_id);
  }
  std::move(callback).Run(true);

  WallpaperInfo wallpaper_info(base::NumberToString(sea_pen_image_id),
                               WALLPAPER_LAYOUT_CENTER_CROPPED,
                               WallpaperType::kSeaPen, base::Time::Now());

  if (preview_mode) {
    confirm_preview_wallpaper_callback_ = base::BindOnce(
        &WallpaperControllerImpl::SetWallpaperImpl, base::Unretained(this),
        account_id, wallpaper_info, image_skia, /*show_wallpaper=*/false);
    reload_preview_wallpaper_callback_ =
        base::BindRepeating(&WallpaperControllerImpl::ShowWallpaperImage,
                            base::Unretained(this), image_skia, wallpaper_info,
                            /*preview_mode=*/true, /*is_override=*/false);
    // Show the preview wallpaper.
    reload_preview_wallpaper_callback_.Run();
  } else {
    SetWallpaperImpl(account_id, wallpaper_info, image_skia,
                     /*show_wallpaper=*/IsActiveUser(account_id));
  }
}

void WallpaperControllerImpl::OnSeaPenFilesMigrated(const AccountId& account_id,
                                                    const bool success) {
  if (!success) {
    LOG(WARNING) << "Failed to migrate SeaPen files";
    return;
  }

  WallpaperInfo wallpaper_info;
  if (!GetUserWallpaperInfo(account_id, &wallpaper_info)) {
    LOG(WARNING) << "Failed to get user wallpaper info post SeaPen migration";
    return;
  }

  if (wallpaper_info.type != WallpaperType::kSeaPen) {
    DVLOG(0) << "Current wallpaper is not SeaPen, migration complete";
    return;
  }

  std::optional<uint32_t> sea_pen_image_id =
      GetIdFromFileName(base::FilePath(wallpaper_info.location));
  if (!sea_pen_image_id.has_value()) {
    LOG(WARNING) << "Invalid SeaPen info.location";
    SetDefaultWallpaper(account_id, /*show_wallpaper=*/IsActiveUser(account_id),
                        base::DoNothing());
    return;
  }

  SetSeaPenWallpaper(account_id, sea_pen_image_id.value(),
                     /*preview_mode=*/false, base::DoNothing());
}

void WallpaperControllerImpl::SaveAndSetWallpaper(const AccountId& account_id,
                                                  bool is_ephemeral,
                                                  const std::string& file_name,
                                                  const std::string& file_path,
                                                  WallpaperType type,
                                                  WallpaperLayout layout,
                                                  bool show_wallpaper,
                                                  const gfx::ImageSkia& image) {
  SaveAndSetWallpaperWithCompletion(account_id, is_ephemeral, file_name,
                                    file_path, type, layout, show_wallpaper,
                                    image, base::DoNothing());
}

void WallpaperControllerImpl::SaveAndSetWallpaperWithCompletion(
    const AccountId& account_id,
    bool is_ephemeral,
    const std::string& file_name,
    const std::string& file_path,
    WallpaperType type,
    WallpaperLayout layout,
    bool show_wallpaper,
    const gfx::ImageSkia& image,
    FilePathCallback image_saved_callback) {
  if (wallpaper_controller_client_) {
    wallpaper_controller_client_->GetFilesId(
        account_id,
        base::BindOnce(
            &WallpaperControllerImpl::SaveAndSetWallpaperWithCompletionFilesId,
            weak_factory_.GetWeakPtr(), account_id, is_ephemeral, file_name,
            file_path, type, layout, show_wallpaper, image,
            std::move(image_saved_callback)));
  }
}

void WallpaperControllerImpl::SaveAndSetWallpaperWithCompletionFilesId(
    const AccountId& account_id,
    bool is_ephemeral,
    const std::string& file_name,
    const std::string& file_path,
    WallpaperType type,
    WallpaperLayout layout,
    bool show_wallpaper,
    const gfx::ImageSkia& image,
    FilePathCallback image_saved_callback,
    const std::string& wallpaper_files_id) {
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
  WallpaperInfo info = {relative_path, layout, type, base::Time::Now(),
                        file_path};
  if (!SetUserWallpaperInfo(account_id, is_ephemeral, info)) {
    LOG(ERROR) << "Setting user wallpaper info fails. This should never happen "
                  "except in tests.";
  }

  base::FilePath wallpaper_path = GetCustomWallpaperPath(
      kOriginalWallpaperSubDir, wallpaper_files_id, file_name);

  const bool should_save_to_disk =
      !IsEphemeralUser(account_id) && !is_ephemeral;

  if (should_save_to_disk) {
    wallpaper_file_manager_->SaveWallpaperToDisk(
        type, GlobalChromeOSCustomWallpapersDir(), file_name, layout, image,
        std::move(image_saved_callback), wallpaper_files_id);
  }

  if (show_wallpaper) {
    ShowWallpaperImage(image, info, /*preview_mode=*/false,
                       /*is_override=*/false);
  }

  wallpaper_cache_map_[account_id] =
      CustomWallpaperElement(wallpaper_path, image);
}

void WallpaperControllerImpl::OnWallpaperDecoded(const AccountId& account_id,
                                                 const base::FilePath& path,
                                                 const WallpaperInfo& info,
                                                 bool show_wallpaper,
                                                 const gfx::ImageSkia& image) {
  // Empty image indicates decode failure. Use default wallpaper in this case.
  if (image.isNull()) {
    LOG(ERROR) << "Failed to decode user wallpaper at " << path.value()
               << " Falls back to default wallpaper. ";
    wallpaper_cache_map_.erase(account_id);
    SetDefaultWallpaperImpl(GetUserType(account_id), show_wallpaper,
                            base::DoNothing());
    return;
  }

  wallpaper_cache_map_[account_id] = CustomWallpaperElement(path, image);
  if (show_wallpaper) {
    ShowWallpaperImage(image, info, /*preview_mode=*/false,
                       /*is_override=*/false);
  }
}

void WallpaperControllerImpl::ReloadWallpaper(bool clear_cache) {
  const bool was_one_shot_wallpaper = IsOneShotWallpaper();
  const gfx::ImageSkia one_shot_wallpaper =
      was_one_shot_wallpaper
          ? current_wallpaper_->wallpaper_info().one_shot_wallpaper
          : gfx::ImageSkia();

  current_wallpaper_.reset();

  // Cancel any in-flight color calculation.
  color_calculator_.reset();

  if (clear_cache)
    wallpaper_cache_map_.clear();

  if (reload_override_wallpaper_callback_) {
    reload_override_wallpaper_callback_.Run();
  } else if (reload_preview_wallpaper_callback_) {
    reload_preview_wallpaper_callback_.Run();
  } else if (current_account_id_.is_valid()) {
    ShowUserWallpaper(current_account_id_);
  } else if (was_one_shot_wallpaper) {
    ShowOneShotWallpaper(one_shot_wallpaper);
  } else {
    ShowSigninWallpaper();
  }
}

void WallpaperControllerImpl::SetCalculatedColors(
    const WallpaperCalculatedColors& calculated_colors) {
  // Observers should be notified if this is the first call to
  // `SetCalculatedColors` no matter what.
  if (calculated_colors == calculated_colors_) {
    return;
  }

  calculated_colors_ = calculated_colors;
  for (auto& observer : observers_)
    observer.OnWallpaperColorsChanged();
}

void WallpaperControllerImpl::ResetCalculatedColors() {
  calculated_colors_.reset();
}

void WallpaperControllerImpl::CalculateWallpaperColors() {
  // Cancel any in-flight color calculation.
  if (color_calculator_) {
    color_calculator_.reset();
  }

  if (!current_wallpaper_) {
    return;
  }

  std::optional<WallpaperCalculatedColors> colors =
      pref_manager_->GetCachedWallpaperColors(
          current_wallpaper_->wallpaper_info().location);
  if (colors) {
    SetCalculatedColors(std::move(*colors));
    return;
  }

  // Color calculation is only allowed during an active session for performance
  // reasons. Observers outside an active session are notified of the cache, or
  // an invalid color if a previous calculation during active session failed.
  if (!ShouldCalculateColors()) {
    ResetCalculatedColors();
    return;
  }

  color_calculator_ =
      std::make_unique<WallpaperColorCalculator>(GetWallpaper());
  if (!color_calculator_->StartCalculation(base::BindOnce(
          &WallpaperControllerImpl::OnColorCalculationComplete,
          weak_factory_.GetWeakPtr(), current_wallpaper_->wallpaper_info()))) {
    ResetCalculatedColors();
  }
}

bool WallpaperControllerImpl::ShouldCalculateColors() const {
  gfx::ImageSkia image = GetWallpaper();
  if (image.isNull()) {
    return false;
  }
  if (IsOobeState()) {
    return true;
  }
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  // Active session
  if (session_state == session_manager::SessionState::ACTIVE) {
    return true;
  }

  return false;
}

void WallpaperControllerImpl::OnOverrideWallpaperDecoded(
    const WallpaperInfo& info,
    const gfx::ImageSkia& image) {
  // Do nothing if |RemoveOverrideWallpaper()| was called before decoding
  // completes.
  if (!is_override_wallpaper_) {
    return;
  }
  if (image.isNull()) {
    RemoveOverrideWallpaper();
    return;
  }
  reload_override_wallpaper_callback_ =
      base::BindRepeating(&WallpaperControllerImpl::ShowWallpaperImage,
                          weak_factory_.GetWeakPtr(), image, info,
                          /*preview_mode=*/false, /*is_override=*/true);
  reload_override_wallpaper_callback_.Run();
}

bool WallpaperControllerImpl::IsDevicePolicyWallpaper() const {
  return current_wallpaper_ &&
         current_wallpaper_->wallpaper_info().type == WallpaperType::kDevice;
}

bool WallpaperControllerImpl::IsOneShotWallpaper() const {
  return current_wallpaper_ &&
         current_wallpaper_->wallpaper_info().type == WallpaperType::kOneShot;
}

bool WallpaperControllerImpl::ShouldSetDevicePolicyWallpaper() const {
  // Only allow the device wallpaper if the policy is in effect for enterprise
  // managed devices.
  if (device_policy_wallpaper_path_.empty())
    return false;

  // Only set the device wallpaper if we're at the login screen.
  return Shell::Get()->session_controller()->GetSessionState() ==
         session_manager::SessionState::LOGIN_PRIMARY;
}

void WallpaperControllerImpl::SetDevicePolicyWallpaper() {
  DCHECK(ShouldSetDevicePolicyWallpaper());
  ReadAndDecodeWallpaper(
      base::BindOnce(&WallpaperControllerImpl::OnDevicePolicyWallpaperDecoded,
                     weak_factory_.GetWeakPtr()),
      device_policy_wallpaper_path_);
}

void WallpaperControllerImpl::OnDevicePolicyWallpaperDecoded(
    const gfx::ImageSkia& image) {
  // It might be possible that the device policy controlled wallpaper finishes
  // decoding after the user logs in. In this case do nothing.
  if (!ShouldSetDevicePolicyWallpaper()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDevice, SetWallpaperResult::kPermissionDenied);
    return;
  }

  if (image.isNull()) {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDevice, SetWallpaperResult::kDecodingError);
    // If device policy wallpaper failed decoding, fall back to the default
    // wallpaper.
    // TODO(crbug.com/1329567): Decide if the regular default is correct.  But
    // this is the current behavior for EmptyAccountId.
    SetDefaultWallpaperImpl(user_manager::UserType::kRegular,
                            /*show_wallpaper=*/true, base::DoNothing());
  } else {
    wallpaper_metrics_manager_->LogWallpaperResult(
        WallpaperType::kDevice, SetWallpaperResult::kSuccess);
    WallpaperInfo info = {device_policy_wallpaper_path_.value(),
                          WALLPAPER_LAYOUT_CENTER_CROPPED,
                          WallpaperType::kDevice, base::Time::Now()};
    ShowWallpaperImage(image, info, /*preview_mode=*/false,
                       /*is_override=*/false);
  }
}

void WallpaperControllerImpl::GetInternalDisplayCompositorLock() {
  if (!display::HasInternalDisplay())
    return;

  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(display::Display::InternalDisplayId());
  if (!root_window)
    return;

  compositor_lock_ = root_window->layer()->GetCompositor()->GetCompositorLock(
      this, kCompositorLockTimeout);
}

void WallpaperControllerImpl::RepaintWallpaper() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    auto* wallpaper_view =
        root_window_controller->wallpaper_widget_controller()->wallpaper_view();
    if (wallpaper_view)
      wallpaper_view->SchedulePaint();
  }
}

void WallpaperControllerImpl::HandleWallpaperInfoSyncedIn(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  if (!CanSetUserWallpaper(account_id))
    return;
  // We don't sync for background users because we don't want to update the
  // wallpaper for background users. Instead, we call
  // HandleWallpaperInfoSyncedIn again in OnActiveUserPrefServiceChanged.
  if (!IsActiveUser(account_id))
    return;
  switch (info.type) {
    case WallpaperType::kCustomized:
      HandleCustomWallpaperInfoSyncedIn(account_id, info);
      break;
    case WallpaperType::kDaily:
      HandleDailyWallpaperInfoSyncedIn(account_id, info);
      break;
    case WallpaperType::kOnline:
      HandleSettingOnlineWallpaperFromWallpaperInfo(account_id, info);
      break;
    case WallpaperType::kOnceGooglePhotos:
    case WallpaperType::kDailyGooglePhotos:
      HandleGooglePhotosWallpaperInfoSyncedIn(account_id, info);
      break;
    case WallpaperType::kDefault:
    case WallpaperType::kPolicy:
    case WallpaperType::kThirdParty:
    case WallpaperType::kDevice:
    case WallpaperType::kOneShot:
    case WallpaperType::kOobe:
    case WallpaperType::kSeaPen:
    case WallpaperType::kCount:
      DCHECK(false) << "Synced in an unsyncable wallpaper type";
      break;
  }
}

void WallpaperControllerImpl::OnTimeOfDayWallpaperSetAfterOobe(bool success) {
  wallpaper_metrics_manager_->LogSettingTimeOfDayWallpaperAfterOobe(success);
}

void WallpaperControllerImpl::OnDailyRefreshWallpaperUpdated(
    RefreshWallpaperCallback callback,
    bool success) {
  if (success) {
    // Updates the check times based on when the daily wallpaper is refreshed.
    // First check time is roughly 24 hours from now and the second check
    // (retry) time is 25 hours (or 1 hour) from now.";
    auto first_check_time = base::Time::Now();
    auto second_check_time = first_check_time + base::Hours(1);
    if (features::IsWallpaperFastRefreshEnabled()) {
      first_check_time = base::Time::Now() + base::Minutes(1);
      second_check_time = first_check_time + base::Minutes(1);
    }
    DVLOG(1) << __func__
             << " updating check times - first_check_time=" << first_check_time
             << " - second_check_time=" << second_check_time;
    daily_refresh_scheduler_->SetCustomStartTime(
        TimeOfDay::FromTime(first_check_time));
    daily_refresh_scheduler_->SetCustomEndTime(
        TimeOfDay::FromTime(second_check_time));
  }
  std::move(callback).Run(success);
  // Resume observing daily refresh scheduler if necessary.
  if (!daily_refresh_observation_.IsObserving()) {
    daily_refresh_observation_.Observe(daily_refresh_scheduler_.get());
  }
}

void WallpaperControllerImpl::SetDailyRefreshCollectionId(
    const AccountId& account_id,
    const std::string& collection_id) {
  if (!CanSetUserWallpaper(account_id)) {
    LOG(WARNING) << "Invalid request to set daily refresh collection id";
    return;
  }

  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info))
    return;

  // If daily refresh is being enabled.
  if (!collection_id.empty()) {
    info.type = WallpaperType::kDaily;
    info.collection_id = collection_id;
  }

  // If Daily Refresh is disabled without selecting another wallpaper, we should
  // keep the current wallpaper and change to type `WallpaperType::kOnline`, so
  // daily refreshes stop.
  if (collection_id.empty() && info.type == WallpaperType::kDaily) {
    info.type = WallpaperType::kOnline;
  }
  SetUserWallpaperInfo(account_id, info);
}

std::string WallpaperControllerImpl::GetDailyRefreshCollectionId(
    const AccountId& account_id) const {
  WallpaperInfo info;
  if (!GetUserWallpaperInfo(account_id, &info))
    return std::string();
  if (info.type != WallpaperType::kDaily)
    return std::string();
  return info.collection_id;
}

void WallpaperControllerImpl::SyncLocalAndRemotePrefs(
    const AccountId& account_id) {
  // Check if the synced info was set by another device, and if we have already
  // handled it locally.
  WallpaperInfo synced_info;
  auto on_synced_info_migrated = base::BindOnce(
      &WallpaperControllerImpl::HandleSyncedWallpaperInfoAfterMigration,
      weak_factory_.GetWeakPtr(), account_id);
  if (!pref_manager_->GetSyncedWallpaperInfo(account_id, &synced_info)) {
    if (!features::IsVersionWallpaperInfoEnabled()) {
      return;
    }
    // Attempts to show the user's wallpaper from the previous pref.
    if (!pref_manager_->GetSyncedWallpaperInfoFromDeprecatedPref(
            account_id, &synced_info)) {
      return;
    }
    on_synced_info_migrated =
        base::BindOnce(&WallpaperControllerImpl::
                           HandleDeprecatedSyncedWallpaperInfoAfterMigration,
                       weak_factory_.GetWeakPtr(), account_id);
  }
  if (wallpaper_info_migrator_.ShouldMigrate(synced_info)) {
    wallpaper_info_migrator_.Migrate(account_id, synced_info,
                                     std::move(on_synced_info_migrated));
  } else {
    // If no migration is needed, proceed as before
    HandleSyncedWallpaperInfoAfterMigration(account_id, synced_info);
  }
}

const AccountId& WallpaperControllerImpl::CurrentAccountId() const {
  return current_account_id_;
}

bool WallpaperControllerImpl::IsDailyRefreshEnabled() const {
  return !GetDailyRefreshCollectionId(GetActiveAccountId()).empty();
}

bool WallpaperControllerImpl::IsDailyGooglePhotosWallpaperSelected() {
  auto info = GetActiveUserWallpaperInfo();
  return (info && info->type == WallpaperType::kDailyGooglePhotos);
}

bool WallpaperControllerImpl::IsGooglePhotosWallpaperSet() const {
  auto info = GetActiveUserWallpaperInfo();
  return (info && info->type == WallpaperType::kOnceGooglePhotos);
}

void WallpaperControllerImpl::UpdateDailyRefreshWallpaper(
    RefreshWallpaperCallback callback) {
  // Invalidate weak ptrs to cancel prior requests to set wallpaper.
  set_wallpaper_weak_factory_.InvalidateWeakPtrs();
  if (!IsDailyRefreshEnabled() && !IsDailyGooglePhotosWallpaperSelected()) {
    std::move(callback).Run(false);
    return;
  }

  // Temporarily pause observing the scheduler to avoid unnecessary
  // `OnCheckpointChanged()` call after `on_done` callback is run.
  daily_refresh_observation_.Reset();
  auto on_done =
      base::BindOnce(&WallpaperControllerImpl::OnDailyRefreshWallpaperUpdated,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  AccountId account_id = GetActiveAccountId();
  WallpaperInfo info;

  // |wallpaper_controller_cient_| has a slightly shorter lifecycle than
  // wallpaper controller.
  if (wallpaper_controller_client_ && GetUserWallpaperInfo(account_id, &info)) {
    if (info.type == WallpaperType::kDailyGooglePhotos) {
      SetGooglePhotosWallpaper(
          GooglePhotosWallpaperParams(
              account_id, info.collection_id,
              /*daily_refresh_enabled=*/true, info.layout,
              /*preview_mode=*/false, /*dedup_key=*/std::nullopt),
          std::move(on_done));
    } else {
      DCHECK_EQ(info.type, WallpaperType::kDaily);
      OnlineWallpaperVariantInfoFetcher::FetchParamsCallback fetch_callback =
          base::BindOnce(&WallpaperControllerImpl::OnWallpaperVariantsFetched,
                         set_wallpaper_weak_factory_.GetWeakPtr(), info.type,
                         std::move(on_done));
      // Fetch can fail if wallpaper_controller_client has been cleared or
      // |info| is malformed.
      if (!variant_info_fetcher_.FetchDailyWallpaper(
              account_id, info, std::move(fetch_callback))) {
        // Could not start fetch of wallpaper variants. Likely because the
        // chrome client isn't ready. Schedule for later.
        NOTREACHED() << "Failed to initiate daily wallpaper fetch";
      }
    }
  } else {
    std::move(on_done).Run(false);
  }
}

void WallpaperControllerImpl::CheckGooglePhotosStaleness(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  DCHECK_EQ(info.type, WallpaperType::kOnceGooglePhotos);
  wallpaper_controller_client_->FetchGooglePhotosPhoto(
      account_id, info.location,
      base::BindOnce(&WallpaperControllerImpl::HandleGooglePhotosStalenessCheck,
                     set_wallpaper_weak_factory_.GetWeakPtr(), account_id));
}

void WallpaperControllerImpl::HandleGooglePhotosStalenessCheck(
    const AccountId& account_id,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    bool success) {
  // If `success` is false, then we failed to connect to the Google Photos API
  // for some reason. We're already set to try again later, since the timer was
  // reset to one hour in `CheckGooglePhotosStaleness`.
  if (!success)
    return;

  if (!photo) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteGooglePhotosCache, account_id));
    SetDefaultWallpaper(account_id, /*show_wallpaper=*/true, base::DoNothing());
  }
}

void WallpaperControllerImpl::SaveWallpaperToDriveFsAndSyncInfo(
    const AccountId& account_id,
    const base::FilePath& origin_path) {
  if (!wallpaper_controller_client_)
    return;
  if (!wallpaper_controller_client_->IsWallpaperSyncEnabled(account_id))
    return;
  drivefs_delegate_->SaveWallpaper(
      account_id, origin_path,
      base::BindOnce(&WallpaperControllerImpl::WallpaperSavedToDriveFS,
                     weak_factory_.GetWeakPtr(), account_id));
}

void WallpaperControllerImpl::WallpaperSavedToDriveFS(
    const AccountId& account_id,
    bool success) {
  if (!success)
    return;
  WallpaperInfo local_info;
  CHECK(pref_manager_->GetLocalWallpaperInfo(account_id, &local_info));
  pref_manager_->SetSyncedWallpaperInfo(account_id, local_info);
}

void WallpaperControllerImpl::HandleCustomWallpaperInfoSyncedIn(
    const AccountId& account_id,
    const WallpaperInfo& wallpaper_info) {
  drivefs_delegate_->GetWallpaperModificationTime(
      account_id,
      base::BindOnce(
          &WallpaperControllerImpl::OnGetDriveFsWallpaperModificationTime,
          weak_factory_.GetWeakPtr(), account_id, wallpaper_info));
}

void WallpaperControllerImpl::OnGetDriveFsWallpaperModificationTime(
    const AccountId& account_id,
    const WallpaperInfo& wallpaper_info,
    base::Time modification_time) {
  if (modification_time.is_null() || modification_time < wallpaper_info.date) {
    // If the drivefs image modification time is null, watch DriveFS for the
    // file being created. If the file exists but is older than synced wallpaper
    // info, watch for the file being updated by the other device.
    DVLOG(1) << "Skip syncing custom wallpaper from DriveFS. Wallpaper "
                "modification_time: "
             << modification_time;
    drivefs_delegate_->WaitForWallpaperChange(
        account_id,
        base::BindOnce(&WallpaperControllerImpl::OnDriveFsWallpaperChange,
                       weak_factory_.GetWeakPtr(), account_id));
    return;
  }
  base::FilePath path_in_prefs = base::FilePath(wallpaper_info.location);
  std::string file_name = path_in_prefs.BaseName().value();
  std::string file_path = wallpaper_info.user_file_path;

  drivefs_delegate_->DownloadAndDecodeWallpaper(
      account_id,
      base::BindOnce(&WallpaperControllerImpl::SaveAndSetWallpaper,
                     weak_factory_.GetWeakPtr(), account_id,
                     IsEphemeralUser(account_id), file_name, file_path,
                     WallpaperType::kCustomized, wallpaper_info.layout,
                     /*show_wallpaper=*/true));
}

void WallpaperControllerImpl::OnDriveFsWallpaperChange(
    const AccountId& account_id,
    bool success) {
  if (success) {
    SyncLocalAndRemotePrefs(account_id);
  }
}

void WallpaperControllerImpl::HandleDailyWallpaperInfoSyncedIn(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  DCHECK(info.type == WallpaperType::kDaily);
  std::string old_collection_id = GetDailyRefreshCollectionId(account_id);
  if (info.collection_id == old_collection_id)
    return;
  OnlineWallpaperVariantInfoFetcher::FetchParamsCallback callback =
      base::BindOnce(&WallpaperControllerImpl::OnWallpaperVariantsFetched,
                     weak_factory_.GetWeakPtr(), info.type, base::DoNothing());
  if (!variant_info_fetcher_.FetchDailyWallpaper(account_id, info,
                                                 std::move(callback))) {
    NOTREACHED() << "Fetch of daily wallpaper info failed.";
  }
}

void WallpaperControllerImpl::HandleGooglePhotosWallpaperInfoSyncedIn(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  bool daily_refresh_enabled = info.type == WallpaperType::kDailyGooglePhotos;
  if (daily_refresh_enabled) {
    // We only want to update the user's `WallpaperInfo` if this is a new
    // album.  Otherwise, each time the daily refresh timer expires on multiple
    // devices we could trigger devices to refresh multiple times.
    WallpaperInfo old_info;
    if (GetUserWallpaperInfo(account_id, &old_info) &&
        old_info.collection_id != info.collection_id) {
      SetGooglePhotosWallpaper(
          GooglePhotosWallpaperParams(
              account_id, info.collection_id,
              /*daily_refresh_enabled=*/true, info.layout,
              /*preview_mode=*/false, /*dedup_key=*/std::nullopt),
          base::DoNothing());
    }
  } else {
    SetGooglePhotosWallpaper(GooglePhotosWallpaperParams(
                                 account_id, info.location,
                                 /*daily_refresh_enabled=*/false, info.layout,
                                 /*preview_mode=*/false, info.dedup_key),
                             base::DoNothing());
  }
}

void WallpaperControllerImpl::HandleSettingOnlineWallpaperFromWallpaperInfo(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  DCHECK(IsOnlineWallpaper(info.type));

  OnlineWallpaperVariantInfoFetcher::FetchParamsCallback callback =
      base::BindOnce(&WallpaperControllerImpl::OnWallpaperVariantsFetched,
                     set_wallpaper_weak_factory_.GetWeakPtr(), info.type,
                     base::DoNothing());

  variant_info_fetcher_.FetchOnlineWallpaper(account_id, info,
                                             std::move(callback));
}

void WallpaperControllerImpl::CleanUpBeforeSettingUserWallpaperInfo(
    const AccountId& account_id,
    const WallpaperInfo& info) {
  std::vector<base::FilePath> directories_to_remove;
  if (account_id.HasAccountIdKey() &&
      info.type != WallpaperType::kOnceGooglePhotos &&
      info.type != WallpaperType::kDailyGooglePhotos) {
    directories_to_remove.push_back(
        GetUserGooglePhotosWallpaperDir(account_id));
  }
  if (account_id.HasAccountIdKey() && info.type != WallpaperType::kSeaPen) {
    directories_to_remove.push_back(GetUserSeaPenWallpaperDir(account_id));
  }
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteWallpaperInList, std::move(directories_to_remove)));
}

bool WallpaperControllerImpl::IsOobeState() const {
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  // Default OOBE flow
  const bool is_default_oobe_flow =
      session_state == session_manager::SessionState::OOBE;
  // OOBE enterprise enrollment -> add person flow
  const bool is_add_person_flow =
      session_state == session_manager::SessionState::LOGIN_PRIMARY &&
      oobe_state_ != OobeDialogState::HIDDEN;
  DVLOG(1) << __func__ << " is_default_oobe_flow=" << is_default_oobe_flow
           << " is_add_person_flow=" << is_add_person_flow;
  return is_default_oobe_flow || is_add_person_flow;
}

const ScheduledFeature& WallpaperControllerImpl::GetScheduleForOnlineWallpaper(
    const std::string& collection_id) const {
  if (::ash::IsTimeOfDayWallpaper(collection_id) &&
      features::IsTimeOfDayWallpaperEnabled()) {
    return *time_of_day_scheduler_;
  } else {
    return *Shell::Get()->dark_light_mode_controller();
  }
}

}  // namespace ash
