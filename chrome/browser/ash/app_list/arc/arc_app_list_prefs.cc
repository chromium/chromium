// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/compat_mode/arc_resize_lock_manager.h"
#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "ash/components/arc/net/arc_net_host_impl.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_metrics_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_scoped_pref_update.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ash/app_list/arc/arc_package_install_priority_handler.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_initial_optin_metrics_recorder.h"
#include "chrome/browser/ash/arc/session/arc_initial_optin_metrics_recorder_factory.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/icu/source/common/unicode/localebuilder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"

using arc::mojom::AppCategory;

namespace {

constexpr char kActivity[] = "activity";
constexpr char kFrameworkPackageName[] = "android";
constexpr char kResizeLockState[] = "resize_lock_state";
constexpr char kResizeLockNeedsConfirmation[] =
    "resize_lock_needs_confirmation";
constexpr char kGameControlsOptOut[] = "game_controls_opt_out";
constexpr char kIconResourceId[] = "icon_resource_id";
constexpr char kIconVersion[] = "icon_version";
constexpr char kInstallTime[] = "install_time";
constexpr char kIntentUri[] = "intent_uri";
constexpr char kLastBackupAndroidId[] = "last_backup_android_id";
constexpr char kLastBackupTime[] = "last_backup_time";
constexpr char kLastLaunchTime[] = "lastlaunchtime";
constexpr char kLaunchable[] = "launchable";
constexpr char kNeedFixup[] = "need_fixup";
constexpr char kName[] = "name";
constexpr char kNotificationsEnabled[] = "notifications_enabled";
constexpr char kPackageName[] = "package_name";
constexpr char kPackageVersion[] = "package_version";
constexpr char kPinIndex[] = "pin_index";
constexpr char kPermissionStates[] = "permission_states";
constexpr char kPreinstalled[] = "preinstalled";
constexpr char kSticky[] = "sticky";
constexpr char kShortcut[] = "shortcut";
constexpr char kShouldSync[] = "should_sync";
constexpr char kSuspended[] = "suspended";
constexpr char kUninstalled[] = "uninstalled";
constexpr char kVPNProvider[] = "vpnprovider";
constexpr char kPermissionStateGranted[] = "granted";
constexpr char kPermissionStateManaged[] = "managed";
constexpr char kPermissionStateDetails[] = "details";
constexpr char kPermissionStateOneTime[] = "one_time";
constexpr char kWebAppInfo[] = "web_app_info";
constexpr char kTitle[] = "title";
constexpr char kStartUrl[] = "start_url";
constexpr char kScopeUrl[] = "scope_url";
constexpr char kThemeColor[] = "theme_color";
constexpr char kIsWebOnlyTwa[] = "is_web_only_twa";
constexpr char kCertificateSha256Fingerprint[] =
    "certificate_sha256_fingerprint";
constexpr char kWindowLayout[] = "window_layout";
constexpr char kWindowSizeType[] = "window_layout_type";
constexpr char kWindowResizability[] = "window_resizability";
constexpr char kWindowBounds[] = "window_bounds";
constexpr char kVersionName[] = "version_name";
constexpr char kAppSizeBytesString[] = "app_size_bytes_string";
constexpr char kDataSizeBytesString[] = "data_size_bytes_string";
constexpr char kAppCategory[] = "app_category";
constexpr char kLocaleInfo[] = "locale_info";
constexpr char kSupportedLocales[] = "supported_locales";
constexpr char kSelectedLocale[] = "selected_locale";
// Deprecated perfs fields.
constexpr char kDeprecatePackagePrefsSystem[] = "system";

// Defines maximum number of showing splash screen per user.
const int kMaxNumSplashScreen = 2;

// Defines current version for app icons. This is used for invalidation icons in
// case we change how app icons are produced on Android side. Can be updated in
// unit tests.
int current_icons_version = 1;

// Set of default app icon dips that are required to support ARC icons in all
// usage cases.
constexpr int default_app_icon_dip_sizes[] = {16, 32, 48, 64};

constexpr base::TimeDelta kDetectDefaultAppAvailabilityTimeout =
    base::Minutes(1);

// Constants for UMA metrics.
constexpr const char kAppCountUmaPrefix[] = "Arc.AppCount.";
// Do not increase. See base/metrics/histogram_functions.h.
constexpr int kAppCountUmaExclusiveMax = 101;
// A smaller bucket size for apps with a lower count.
constexpr int kAppCountUmaExclusiveMaxLower = 20;

// Constants for "Arc.Data.AppCategory.{Target}.DataSize" UMA metric.
constexpr int kUmaDataSizeNumBuckets = 50;
constexpr int kUmaDataSizeInMBMin = 1;
constexpr int kUmaDataSizeInMBMax = 1000000;  // 1 TB.

// Accessor for deferred set notifications enabled requests in prefs.
class NotificationsEnabledDeferred {
 public:
  explicit NotificationsEnabledDeferred(PrefService* prefs) : prefs_(prefs) {}

  void Put(const std::string& app_id, bool enabled) {
    ScopedDictPrefUpdate update(
        prefs_, arc::prefs::kArcSetNotificationsEnabledDeferred);
    update->Set(app_id, enabled);
  }

  bool Get(const std::string& app_id) {
    const base::Value::Dict& dict =
        prefs_->GetDict(arc::prefs::kArcSetNotificationsEnabledDeferred);
    return dict.FindBool(app_id).value_or(false);
  }

  void Remove(const std::string& app_id) {
    ScopedDictPrefUpdate update(
        prefs_, arc::prefs::kArcSetNotificationsEnabledDeferred);
    update->Remove(app_id);
  }

 private:
  const raw_ptr<PrefService> prefs_;
};

bool WriteIconFile(const base::FilePath& icon_path,
                   const std::vector<uint8_t>& icon_png_data) {
  if (icon_png_data.empty())
    return false;

  base::CreateDirectory(icon_path.DirName());

  if (!base::WriteFile(icon_path, icon_png_data)) {
    VLOG(2) << "Failed to write ARC icon file: " << icon_path.MaybeAsASCII()
            << ".";
    if (!base::DeleteFile(icon_path)) {
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII()
              << ".";
    }
    return false;
  }
  return true;
}

bool InstallIconFromFileThread(const base::FilePath& icon_path,
                               const base::FilePath& foreground_icon_path,
                               const base::FilePath& background_icon_path,
                               arc::mojom::RawIconPngDataPtr icon) {
  const std::vector<uint8_t>& icon_png_data = icon->icon_png_data.value();
  DCHECK(!icon_png_data.empty());

  if (!WriteIconFile(icon_path, icon_png_data))
    return false;

  if (!icon->is_adaptive_icon) {
    // For non-adaptive icon, save the |icon_png_data| to the
    // |foreground_icon_path|, to identify the difference between migrating to
    // the adaptive icon feature enabled and the non-adaptive icon case. If
    // there is a |foreground_icon_path| file without a |background_icon_path|
    // file, that means the icon is a non-adaptive icon. Otherwise, if there is
    // no |foreground_icon_path| file, that means we haven't fetched the
    // adaptive icon yet, then we should request the icon.
    if (!WriteIconFile(foreground_icon_path, icon->icon_png_data.value())) {
      return false;
    }

    return true;
  }

  if (!WriteIconFile(foreground_icon_path,
                     icon->foreground_icon_png_data.value())) {
    return false;
  }

  if (!WriteIconFile(background_icon_path,
                     icon->background_icon_png_data.value())) {
    return false;
  }

  return true;
}

void DeleteAppFolderFromFileThread(const base::FilePath& path) {
  DCHECK(path.DirName().BaseName().MaybeAsASCII() == arc::prefs::kArcApps &&
         (!base::PathExists(path) || base::DirectoryExists(path)));
  const bool deleted = base::DeletePathRecursively(path);
  DCHECK(deleted);
}

// TODO(crbug.com/40497410): Due to shutdown procedure dependency,
// ArcAppListPrefs may try to touch ArcSessionManager related stuff.
// Specifically, this returns false on shutdown phase.
// Remove this check after the shutdown behavior is fixed.
bool IsArcAlive() {
  const auto* arc_session_manager = arc::ArcSessionManager::Get();
  return arc_session_manager && arc_session_manager->IsAllowed();
}

// Returns true if ARC Android instance is supposed to be enabled for the
// profile.  This can happen for if the user has opted in for the given profile,
// or when ARC always starts after login.
bool IsArcAndroidEnabledForProfile(const Profile* profile) {
  return arc::ShouldArcAlwaysStart() ||
         arc::IsArcPlayStoreEnabledForProfile(profile);
}

bool GetInt64FromPref(const base::Value::Dict* dict,
                      const std::string& key,
                      int64_t* value) {
  DCHECK(dict);
  const std::string* value_str = dict->FindString(key);
  if (!value_str) {
    VLOG(2) << "Can't find key in local pref dictionary. Invalid key: " << key
            << ".";
    return false;
  }

  if (!base::StringToInt64(*value_str, value)) {
    VLOG(2) << "Can't change string to int64_t. Invalid string value: "
            << *value_str << ".";
    return false;
  }

  return true;
}

// Converts |rect| to base::Value, e.g. { 0, 100, 200, 300 }.
base::Value RectToValueDict(const gfx::Rect& rect) {
  base::Value::Dict dict;
  dict.Set("x", rect.x());
  dict.Set("y", rect.y());
  dict.Set("width", rect.width());
  dict.Set("height", rect.height());
  return base::Value(std::move(dict));
}

// Gets gfx::Rect from base::Value, e.g. { 0, 100, 200, 300 } returns
// gfx::Rect(0, 100, 200, 300). If the Value does not contains valid rect,
// returns std::nullopt.
std::optional<gfx::Rect> RectFromDictValue(const base::Value* rect_dict) {
  if (!rect_dict)
    return std::nullopt;
  auto x = rect_dict->GetDict().FindInt("x");
  auto y = rect_dict->GetDict().FindInt("y");
  auto width = rect_dict->GetDict().FindInt("width");
  auto height = rect_dict->GetDict().FindInt("height");
  if (!x.has_value() || !y.has_value() || !width.has_value() ||
      !height.has_value()) {
    return std::nullopt;
  }
  return gfx::Rect(x.value(), y.value(), width.value(), height.value());
}

base::Value WindowLayoutToDict(
    const ArcAppListPrefs::WindowLayout& window_layout) {
  base::Value::Dict dict;
  dict.Set(kWindowSizeType, static_cast<int32_t>(window_layout.type));
  dict.Set(kWindowResizability, window_layout.resizable);
  if (window_layout.bounds.has_value())
    dict.Set(kWindowBounds, RectToValueDict(window_layout.bounds.value()));

  return base::Value(std::move(dict));
}

ArcAppListPrefs::WindowLayout WindowLayoutFromDict(
    const base::Value::Dict* dict) {
  if (!dict)
    return ArcAppListPrefs::WindowLayout();

  const base::Value* window_bounds = dict->Find(kWindowBounds);
  return ArcAppListPrefs::WindowLayout(
      static_cast<arc::mojom::WindowSizeType>(
          dict->FindInt(kWindowSizeType).value_or(0)),
      dict->FindBool(kWindowResizability).value_or(true),
      RectFromDictValue(window_bounds));
}

ArcAppListPrefs::WindowLayout WindowLayoutFromApp(
    const arc::mojom::AppInfo& app) {
  if (app.initial_layout.is_null())
    return ArcAppListPrefs::WindowLayout();
  return ArcAppListPrefs::WindowLayout(app.initial_layout->type,
                                       app.initial_layout->resizable,
                                       app.initial_layout->bounds);
}

// Returns true if one of state of |info1| does not match the same state in
// |info2|.
bool AreAppStatesChanged(const ArcAppListPrefs::AppInfo& info1,
                         const ArcAppListPrefs::AppInfo& info2) {
  return info1.sticky != info2.sticky ||
         info1.notifications_enabled != info2.notifications_enabled ||
         info1.resize_lock_state != info2.resize_lock_state ||
         info1.resize_lock_needs_confirmation !=
             info2.resize_lock_needs_confirmation ||
         info1.ready != info2.ready || info1.suspended != info2.suspended ||
         info1.show_in_launcher != info2.show_in_launcher ||
         info1.launchable != info2.launchable ||
         info1.need_fixup != info2.need_fixup ||
         info1.version_name != info2.version_name ||
         info1.app_size_in_bytes != info2.app_size_in_bytes ||
         info1.data_size_in_bytes != info2.data_size_in_bytes;
}

// We have only fixed icon dimensions for default apps, 32, 48 and 64. If
// requested dimension does not exist, use bigger one that can be downsized.
// In case requested dimension is bigger than 64, use largest possible size that
// can be upsized.
ArcAppIconDescriptor MapDefaultAppIconDescriptor(
    const ArcAppIconDescriptor& descriptor) {
  int default_app_dip_size;
  if (descriptor.dip_size <= 32)
    default_app_dip_size = 32;
  else if (descriptor.dip_size <= 48)
    default_app_dip_size = 48;
  else
    default_app_dip_size = 64;
  return ArcAppIconDescriptor(default_app_dip_size, descriptor.scale_factor);
}

// Whether skip install_time for comparing two |AppInfo|.
bool ignore_compare_app_info_install_time = false;

// Reason for installation enumeration; Used for UMA counter for reason for
// install.
enum class InstallationCounterReasonEnum {
  USER = 0,     // Application installed by user.
  DEFAULT = 1,  // Application part of the default set.
  OEM = 2,      // OEM application.
  POLICY = 3,   // Installed by policy.
  UNKNOWN = 4,
  kMaxValue = UNKNOWN,
};

// Reasons for uninstalls. Only one, USER, for now.
enum class UninstallCounterReasonEnum {
  USER = 0,  // Uninstall triggered by user.
  kMaxValue = USER
};

// Remove deprecated package prefs. Otherwise deprecated fields will stay on
// disks.
void MaybeRemoveDeprecatedPackagePrefs(arc::ArcAppScopedPrefUpdate&& update) {
  update.Get().Remove(kDeprecatePackagePrefsSystem);
}

// Validate |locale_tag| based on IETF BCP 47 language tag.
bool IsLocaleTagValid(const std::string& locale_tag) {
  UErrorCode error = U_ZERO_ERROR;
  icu::LocaleBuilder().setLanguageTag(locale_tag.c_str()).build(error);
  return error == U_ZERO_ERROR;
}

// In some cases when ARC is not ready (e.g. ARC hasn't booted / ARC failed to
// boot), users are still allowed to change App Settings from ChromeOS Settings
// page. Hence, there might be synchronization issue between ARC and ChromeOS
// and we should eventually re-sync them.
bool IsSelectedLocaleResyncRequired(
    const base::Value::Dict& saved_package_dict,
    const arc::mojom::PackageLocaleInfo& arc_locale_info,
    const UpdatePackagePrefsReason& update_reason) {
  // Only checks for ARC-boot package refresh.
  if (update_reason != UpdatePackagePrefsReason::kOnPackageListRefreshed) {
    return false;
  }
  const base::Value::Dict* locale_info_dict =
      saved_package_dict.FindDict(kLocaleInfo);
  if (!locale_info_dict) {
    return false;
  }
  // selected_locale always exists if locale_info is present.
  const std::string* saved_selected_locale =
      locale_info_dict->FindString(kSelectedLocale);
  CHECK(saved_selected_locale)
      << "selected_locale always exists if locale_info is present.";
  // Validates if there's a mismatch between ChromeOS' saved `selected_locale`
  // and ARC's previous `selected_locale`
  return *saved_selected_locale != arc_locale_info.selected_locale;
}

void OnArcAppListRefreshed(Profile* profile) {
  if (!arc::IsArcPlayStoreEnabledForProfile(profile))
    return;

  if (!arc::ArcInitialOptInMetricsRecorderFactory::GetForBrowserContext(profile)
           ->NeedReportArcAppListReady()) {
    return;
  }

  DCHECK_EQ(ProfileManager::GetPrimaryUserProfile(), profile);
  auto* prefs = ArcAppListPrefs::Get(profile);
  if (!prefs)
    return;

  const std::vector<std::string> app_ids = prefs->GetAppIds();
  int launchable = 0;
  int ready = 0;
  int error = 0;
  for (const auto& app_id : app_ids) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      if (app_info->launchable)
        ++launchable;

      if (app_info->ready)
        ++ready;
    } else {
      ++error;
    }
  }
  if (ready + error >= launchable) {
    arc::ArcInitialOptInMetricsRecorderFactory::GetForBrowserContext(profile)
        ->OnArcAppListReady();
  }
}

void RecordAppCategoryDataSizeUma(const std::string& category,
                                  uint64_t data_size_in_bytes) {
  const std::string metrics =
      base::StringPrintf("Arc.Data.AppCategory.%s.DataSize", category);
  base::UmaHistogramCustomCounts(metrics, data_size_in_bytes / 1000000,
                                 kUmaDataSizeInMBMin, kUmaDataSizeInMBMax,
                                 kUmaDataSizeNumBuckets);
}

}  // namespace

// static
ArcAppListPrefs* ArcAppListPrefs::Create(Profile* profile) {
  return new ArcAppListPrefs(profile, nullptr);
}

// static
ArcAppListPrefs* ArcAppListPrefs::Create(
    Profile* profile,
    arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
        app_connection_holder_for_testing) {
  DCHECK(app_connection_holder_for_testing);
  return new ArcAppListPrefs(profile, app_connection_holder_for_testing);
}

// static
void ArcAppListPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(arc::prefs::kArcApps);
  registry->RegisterDictionaryPref(arc::prefs::kArcPackages);
  registry->RegisterBooleanPref(arc::prefs::kArcPackagesIsUpToDate, false);
  registry->RegisterIntegerPref(arc::prefs::kArcFrameworkVersion,
                                -1 /* default_value */);
  registry->RegisterDictionaryPref(
      arc::prefs::kArcSetNotificationsEnabledDeferred);
  registry->RegisterIntegerPref(
      arc::prefs::kArcShowResizeLockSplashScreenLimits, kMaxNumSplashScreen);
  ArcDefaultAppList::RegisterProfilePrefs(registry);
}

// static
ArcAppListPrefs* ArcAppListPrefs::Get(content::BrowserContext* context) {
  return ArcAppListPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

// static
std::string ArcAppListPrefs::GetAppId(const std::string& package_name,
                                      const std::string& activity) {
  if (package_name == arc::kPlayStorePackage &&
      activity == arc::kPlayStoreActivity) {
    return arc::kPlayStoreAppId;
  }
  const std::string input = package_name + "#" + activity;
  const std::string app_id = crx_file::id_util::GenerateId(input);
  return app_id;
}

// static
void ArcAppListPrefs::UprevCurrentIconsVersionForTesting() {
  ++current_icons_version;
}

std::string ArcAppListPrefs::GetAppIdByPackageName(
    const std::string& package_name) const {
  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);

  for (const auto it : apps) {
    const base::Value& value = it.second;
    const std::string* installed_package_name =
        value.GetDict().FindString(kPackageName);
    if (!installed_package_name || *installed_package_name != package_name)
      continue;

    const std::string* activity_name = value.GetDict().FindString(kActivity);
    return activity_name ? GetAppId(package_name, *activity_name)
                         : std::string();
  }
  return std::string();
}

ArcAppListPrefs::ArcAppListPrefs(
    Profile* profile,
    arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
        app_connection_holder_for_testing)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      app_connection_holder_for_testing_(app_connection_holder_for_testing),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  VLOG(1) << "ARC app list prefs created";
  DCHECK(profile);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const base::FilePath& base_path = profile->GetPath();
  base_path_ = base_path.AppendASCII(arc::prefs::kArcApps);
  arc_app_metrics_util_ = std::make_unique<arc::ArcAppMetricsUtil>();

  // Once default apps are ready OnDefaultAppsReady is called.
  default_apps_ = std::make_unique<ArcDefaultAppList>(
      profile, base::BindOnce(&ArcAppListPrefs::OnDefaultAppsReady,
                              base::Unretained(this)));

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager) {
    VLOG(1) << "ARC session manager is not available";
    return;
  }

  DCHECK(arc::IsArcAllowedForProfile(profile));

  const std::vector<std::string> existing_app_ids = GetAppIds();
  tracked_apps_.insert(existing_app_ids.begin(), existing_app_ids.end());

  // Not always set in unit_tests
  arc::ArcPolicyBridge* policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  if (policy_bridge)
    policy_bridge->AddObserver(this);

  arc::ArcResizeLockManager* resize_lock_manager =
      arc::ArcResizeLockManager::GetForBrowserContext(profile_);
  if (resize_lock_manager)
    resize_lock_manager->SetPrefDelegate(this);

  arc::ArcNetHostImpl* net_host =
      arc::ArcNetHostImpl::GetForBrowserContext(profile_);
  if (net_host) {
    net_host->SetArcAppMetadataProvider(this);
  }

  if (base::FeatureList::IsEnabled(arc::kSyncInstallPriority)) {
    install_priority_handler_ =
        std::make_unique<arc::ArcPackageInstallPriorityHandler>(profile);
  }
}

ArcAppListPrefs::~ArcAppListPrefs() {
  for (auto& observer : observer_list_)
    observer.OnArcAppListPrefsDestroyed();

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager)
    return;
  DCHECK(arc::ArcServiceManager::Get());
  arc_session_manager->RemoveObserver(this);
  app_connection_holder()->RemoveObserver(this);
}

void ArcAppListPrefs::StartPrefs() {
  // Don't tie ArcAppListPrefs created with sync test profile in sync
  // integration test to ArcSessionManager.
  if (!ArcAppListPrefsFactory::IsFactorySetForSyncTest()) {
    arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
    CHECK(arc_session_manager);

    if (arc_session_manager->profile()) {
      // Note: If ArcSessionManager has profile, it should be as same as the one
      // this instance has, because ArcAppListPrefsFactory creates an instance
      // only if the given Profile meets ARC's requirement.
      // Anyway, just in case, check it here and log. Only some browser tests
      // will log the error. If you see the log outside browser_tests, something
      // unexpected may have happened.
      if (profile_ != arc_session_manager->profile()) {
        LOG(ERROR)
            << "This object's profile_ and ArcSessionManager's don't match.";
      }
      OnArcPlayStoreEnabledChanged(
          arc::IsArcPlayStoreEnabledForProfile(profile_));
    }
    arc_session_manager->AddObserver(this);
  }

  VLOG(1) << "Registering host...";

  app_connection_holder()->SetHost(this);
  app_connection_holder()->AddObserver(this);
  if (!app_connection_holder()->IsConnected())
    OnConnectionClosed();
}

base::FilePath ArcAppListPrefs::GetAppPath(const std::string& app_id) const {
  return base_path_.AppendASCII(app_id);
}

base::FilePath ArcAppListPrefs::MaybeGetIconPathForDefaultApp(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const ArcDefaultAppList::AppInfo* default_app = default_apps_->GetApp(app_id);
  if (!default_app || default_app->app_path.empty())
    return base::FilePath();

  return default_app->app_path.AppendASCII(
      MapDefaultAppIconDescriptor(descriptor).GetName());
}

base::FilePath ArcAppListPrefs::MaybeGetForegroundIconPathForDefaultApp(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const ArcDefaultAppList::AppInfo* default_app = default_apps_->GetApp(app_id);
  if (!default_app || default_app->app_path.empty())
    return base::FilePath();

  return default_app->app_path.AppendASCII(
      MapDefaultAppIconDescriptor(descriptor).GetForegroundIconName());
}

base::FilePath ArcAppListPrefs::MaybeGetBackgroundIconPathForDefaultApp(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const ArcDefaultAppList::AppInfo* default_app = default_apps_->GetApp(app_id);
  if (!default_app || default_app->app_path.empty())
    return base::FilePath();

  return default_app->app_path.AppendASCII(
      MapDefaultAppIconDescriptor(descriptor).GetBackgroundIconName());
}

base::FilePath ArcAppListPrefs::GetIconPath(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) {
  // TODO(khmel): Add DCHECK(GetApp(app_id));
  active_icons_[app_id].insert(descriptor);
  return GetAppPath(app_id).AppendASCII(descriptor.GetName());
}

base::FilePath ArcAppListPrefs::GetForegroundIconPath(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) {
  active_icons_[app_id].insert(descriptor);
  return GetAppPath(app_id).AppendASCII(descriptor.GetForegroundIconName());
}

base::FilePath ArcAppListPrefs::GetBackgroundIconPath(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) {
  active_icons_[app_id].insert(descriptor);
  return GetAppPath(app_id).AppendASCII(descriptor.GetBackgroundIconName());
}

bool ArcAppListPrefs::IsIconRequestRecorded(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const auto iter = request_icon_recorded_.find(app_id);
  if (iter == request_icon_recorded_.end())
    return false;
  return iter->second.count(descriptor);
}

void ArcAppListPrefs::MaybeRemoveIconRequestRecord(const std::string& app_id) {
  request_icon_recorded_.erase(app_id);
}

void ArcAppListPrefs::ClearIconRequestRecord() {
  request_icon_recorded_.clear();
}

void ArcAppListPrefs::RequestIcon(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor,
    base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)> callback) {
  DCHECK_NE(app_id, arc::kPlayStoreAppId);

  // ArcSessionManager can be terminated during test tear down, before callback
  // into this function.
  // TODO(victorhsieh): figure out the best way/place to handle this situation.
  if (arc::ArcSessionManager::Get() == nullptr) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to load icon for non-registered app: " << app_id << ".";
    std::move(callback).Run(nullptr);
    return;
  }

  // In case app is not ready, recorded request will be send to ARC when app
  // becomes ready.
  // This record will prevent ArcAppIcon from resending request to ARC for app
  // icon when icon file decode failure is suffered in case app sends bad icon.
  request_icon_recorded_[app_id].insert(descriptor);

  if (!ready_apps_.count(app_id)) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!app_connection_holder()->IsConnected()) {
    // AppInstance should be ready since we have app_id in ready_apps_. This
    // can happen in browser_tests.
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    std::move(callback).Run(nullptr);
    return;
  }

  SendIconRequest(app_id, *app_info, descriptor, std::move(callback));
}

void ArcAppListPrefs::SendIconRequest(
    const std::string& app_id,
    const AppInfo& app_info,
    const ArcAppIconDescriptor& descriptor,
    base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)>
        icon_data_callback) {
  auto callback =
      base::BindOnce(&ArcAppListPrefs::OnIcon, weak_ptr_factory_.GetWeakPtr(),
                     app_id, descriptor, std::move(icon_data_callback));
  if (app_info.icon_resource_id.empty()) {
    auto* app_instance =
        ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(), GetAppIcon);
    if (!app_instance)
      return;  // Error is logged in macro.

    app_instance->GetAppIcon(app_info.package_name, app_info.activity,
                             descriptor.GetSizeInPixels(), std::move(callback));
  } else {
    auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(),
                                                     GetAppShortcutIcon);
    if (!app_instance)
      return;  // Error is logged in macro.

    app_instance->GetAppShortcutIcon(app_info.icon_resource_id,
                                     descriptor.GetSizeInPixels(),
                                     std::move(callback));
  }
}

void ArcAppListPrefs::RequestRawIconData(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor,
    base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)> callback) {
  RequestIcon(app_id, descriptor, std::move(callback));
}

void ArcAppListPrefs::MaybeRequestIcon(const std::string& app_id,
                                       const ArcAppIconDescriptor& descriptor) {
  if (!IsIconRequestRecorded(app_id, descriptor)) {
    RequestIcon(
        app_id, descriptor,
        base::BindOnce(&ArcAppListPrefs::InstallIcon,
                       weak_ptr_factory_.GetWeakPtr(), app_id, descriptor));
  }
}

void ArcAppListPrefs::SetNotificationsEnabled(const std::string& app_id,
                                              bool enabled) {
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to set notifications enabled flag for non-registered "
            << "app:" << app_id << ".";
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return;
  }

  // In case app is not ready, defer this request.
  if (!ready_apps_.count(app_id)) {
    NotificationsEnabledDeferred(prefs_).Put(app_id, enabled);
    for (auto& observer : observer_list_)
      observer.OnNotificationsEnabledChanged(app_info->package_name, enabled);
    return;
  }

  auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(),
                                                   SetNotificationsEnabled);
  if (!app_instance)
    return;

  NotificationsEnabledDeferred(prefs_).Remove(app_id);
  app_instance->SetNotificationsEnabled(app_info->package_name, enabled);
}

void ArcAppListPrefs::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcAppListPrefs::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ArcAppListPrefs::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

base::RepeatingCallback<std::string(const std::string&)>
ArcAppListPrefs::GetAppIdByPackageNameCallback() {
  return base::BindRepeating(
      [](base::WeakPtr<ArcAppListPrefs> self, const std::string& package_name) {
        if (!self)
          return std::string();
        return self->GetAppIdByPackageName(package_name);
      },
      weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<ArcAppListPrefs::PackageInfo> ArcAppListPrefs::GetPackage(
    const std::string& package_name) const {
  if (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_))
    return nullptr;

  const base::Value::Dict& packages = prefs_->GetDict(arc::prefs::kArcPackages);

  const base::Value::Dict* package = packages.FindDict(package_name);
  if (!package)
    return nullptr;

  if (package->FindBool(kUninstalled).value_or(false))
    return nullptr;

  int64_t last_backup_android_id = 0;
  int64_t last_backup_time = 0;
  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions;

  GetInt64FromPref(package, kLastBackupAndroidId, &last_backup_android_id);
  GetInt64FromPref(package, kLastBackupTime, &last_backup_time);
  const base::Value* permission_val = package->Find(kPermissionStates);
  if (permission_val) {
    const base::Value::Dict* permission_dict = permission_val->GetIfDict();
    DCHECK(permission_dict);

    for (const auto iter : *permission_dict) {
      int64_t permission_type = -1;
      base::StringToInt64(iter.first, &permission_type);
      DCHECK_NE(-1, permission_type);

      const base::Value& permission_state = iter.second;

      const base::Value::Dict* permission_state_dict =
          permission_state.GetIfDict();
      if (permission_state_dict) {
        bool granted = permission_state_dict->FindBool(kPermissionStateGranted)
                           .value_or(false);
        bool managed = permission_state_dict->FindBool(kPermissionStateManaged)
                           .value_or(false);
        const std::string* details =
            permission_state_dict->FindString(kPermissionStateDetails);

        std::optional<std::string> details_opt;
        if (details != nullptr) {
          details_opt = *details;
        }

        bool one_time = permission_state_dict->FindBool(kPermissionStateOneTime)
                            .value_or(false);

        arc::mojom::AppPermission permission =
            static_cast<arc::mojom::AppPermission>(permission_type);
        permissions.emplace(permission,
                            arc::mojom::PermissionState::New(
                                granted, managed, details_opt, one_time));
      } else {
        LOG(ERROR) << "Permission state was not a dictionary.";
      }
    }
  }

  arc::mojom::WebAppInfoPtr web_app_info;
  if (const base::Value* web_app_info_value = package->Find(kWebAppInfo)) {
    const base::Value::Dict& web_app_info_dict = web_app_info_value->GetDict();
    web_app_info = arc::mojom::WebAppInfo::New();
    web_app_info->title = *web_app_info_dict.FindString(kTitle);
    web_app_info->start_url = *web_app_info_dict.FindString(kStartUrl);
    web_app_info->scope_url = *web_app_info_dict.FindString(kScopeUrl);
    bool must_convert_to_int = base::StringToInt64(
        *web_app_info_dict.FindString(kThemeColor), &web_app_info->theme_color);
    DCHECK(must_convert_to_int);
    web_app_info->is_web_only_twa = *web_app_info_dict.FindBool(kIsWebOnlyTwa);
    if (const std::string* fingerprint =
            web_app_info_dict.FindString(kCertificateSha256Fingerprint)) {
      web_app_info->certificate_sha256_fingerprint = *fingerprint;
    }
  }
  arc::mojom::PackageLocaleInfoPtr locale_info;
  if (const base::Value* locale_info_value = package->Find(kLocaleInfo)) {
    const base::Value::Dict& locale_info_dict = locale_info_value->GetDict();
    if (const base::Value::List* supported_locales =
            locale_info_dict.FindList(kSupportedLocales)) {
      locale_info = arc::mojom::PackageLocaleInfo::New();

      locale_info->supported_locales.reserve(supported_locales->size());
      for (const base::Value& locale : *supported_locales) {
        locale_info->supported_locales.emplace_back(locale.GetString());
      }

      locale_info->selected_locale =
          *locale_info_dict.FindString(kSelectedLocale);
    }
  }

  return std::make_unique<PackageInfo>(
      package_name, package->FindInt(kPackageVersion).value_or(0),
      last_backup_android_id, last_backup_time,
      package->FindBool(kShouldSync).value_or(false),
      package->FindBool(kVPNProvider).value_or(false),
      package->FindBool(kPreinstalled).value_or(false),
      package->FindBool(kGameControlsOptOut).value_or(false),
      std::move(permissions), std::move(web_app_info), std::move(locale_info));
}

bool ArcAppListPrefs::IsPackageInstalled(
    const std::string& package_name) const {
  return GetPackage(package_name) != nullptr;
}

std::vector<std::string> ArcAppListPrefs::GetAppIds() const {
  if (arc::ShouldArcAlwaysStart())
    return GetAppIdsNoArcEnabledCheck();

  if (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) {
    // Default ARC apps available before OptIn.
    std::vector<std::string> ids;
    for (const auto& default_app : default_apps_->GetActiveApps()) {
      // Default apps are iteratively added to prefs. That generates
      // |OnAppRegistered| event per app. Consumer may use this event to request
      // list of all apps. Although this practice is discouraged due the
      // performance reason, let be safe and in order to prevent listing of not
      // yet registered apps, filter out default apps based of tracked state.
      if (tracked_apps_.count(default_app.first))
        ids.push_back(default_app.first);
    }
    return ids;
  }
  return GetAppIdsNoArcEnabledCheck();
}

std::vector<std::string> ArcAppListPrefs::GetAppIdsNoArcEnabledCheck() const {
  std::vector<std::string> ids;
  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);

  // crx_file::id_util is de-facto utility for id generation.
  for (const auto app : apps) {
    if (!crx_file::id_util::IdIsValid(app.first))
      continue;

    ids.push_back(app.first);
  }

  return ids;
}

std::unique_ptr<ArcAppListPrefs::AppInfo> ArcAppListPrefs::GetApp(
    const std::string& app_id) const {
  // Information for default app is available before ARC enabled.
  if ((!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) &&
      !default_apps_->HasApp(app_id)) {
    return nullptr;
  }

  return GetAppFromPrefs(app_id);
}

std::unique_ptr<ArcAppListPrefs::AppInfo> ArcAppListPrefs::GetAppFromPrefs(
    const std::string& app_id) const {
  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);
  const base::Value::Dict* app_dict = apps.FindDict(app_id);
  if (!app_dict)
    return nullptr;

  bool notifications_enabled =
      app_dict->FindBool(kNotificationsEnabled).value_or(true);
  auto resize_lock_state = static_cast<arc::mojom::ArcResizeLockState>(
      app_dict->FindInt(kResizeLockState)
          .value_or(
              static_cast<int32_t>(arc::mojom::ArcResizeLockState::UNDEFINED)));
  const bool shortcut = app_dict->FindBool(kShortcut).value_or(false);
  const bool launchable = app_dict->FindBool(kLaunchable).value_or(true);
  const bool need_fixup = app_dict->FindBool(kNeedFixup).value_or(false);

  const std::string* maybe_name = app_dict->FindString(kName);
  const std::string* maybe_package_name = app_dict->FindString(kPackageName);
  const std::string* maybe_activity = app_dict->FindString(kActivity);
  const std::string* maybe_intent_uri = app_dict->FindString(kIntentUri);
  const std::string* maybe_icon_resource_id =
      app_dict->FindString(kIconResourceId);
  const std::string* maybe_version_name = app_dict->FindString(kVersionName);

  std::string name = maybe_name ? *maybe_name : std::string();
  std::string package_name =
      maybe_package_name ? *maybe_package_name : std::string();
  std::string activity = maybe_activity ? *maybe_activity : std::string();
  std::string intent_uri = maybe_intent_uri ? *maybe_intent_uri : std::string();
  std::string icon_resource_id =
      maybe_icon_resource_id ? *maybe_icon_resource_id : std::string();

  std::optional<std::string> version_name = std::nullopt;
  if (maybe_version_name && *maybe_version_name != std::string())
    version_name = *maybe_version_name;

  DCHECK(!name.empty());
  DCHECK(!shortcut || activity.empty());
  DCHECK(!shortcut || !intent_uri.empty());

  int64_t last_launch_time_internal = 0;
  base::Time last_launch_time;
  if (GetInt64FromPref(app_dict, kLastLaunchTime, &last_launch_time_internal)) {
    last_launch_time = base::Time::FromInternalValue(last_launch_time_internal);
  }

  std::optional<uint64_t> app_size_in_bytes;
  std::optional<uint64_t> data_size_in_bytes;

  auto* app_size_entry = app_dict->FindString(kAppSizeBytesString);
  if (app_size_entry != nullptr && !app_size_entry->empty()) {
    uint64_t app_size = 0;
    if (base::StringToUint64(*app_size_entry, &app_size))
      app_size_in_bytes = app_size;
  }

  auto* data_size_entry = app_dict->FindString(kDataSizeBytesString);
  if (data_size_entry != nullptr && !data_size_entry->empty()) {
    uint64_t data_size = 0;
    if (base::StringToUint64(*data_size_entry, &data_size))
      data_size_in_bytes = data_size;
  }

  const bool deferred = NotificationsEnabledDeferred(prefs_).Get(app_id);
  if (deferred)
    notifications_enabled = deferred;

  WindowLayout window_layout =
      WindowLayoutFromDict(app_dict->FindDict(kWindowLayout));

  const auto app_category = static_cast<arc::mojom::AppCategory>(
      app_dict->FindInt(kAppCategory)
          .value_or(static_cast<int32_t>(arc::mojom::AppCategory::kUndefined)));

  return std::make_unique<AppInfo>(
      name, package_name, activity, intent_uri, icon_resource_id, version_name,
      last_launch_time, GetInstallTime(app_id),
      app_dict->FindBool(kSticky).value_or(false), notifications_enabled,
      resize_lock_state,
      app_dict->FindBool(kResizeLockNeedsConfirmation).value_or(true),
      window_layout, ready_apps_.count(app_id) > 0 /* ready */,
      app_dict->FindBool(kSuspended).value_or(false),
      launchable && arc::ShouldShowInLauncher(app_id), shortcut, launchable,
      need_fixup, app_size_in_bytes, data_size_in_bytes, app_category);
}

bool ArcAppListPrefs::IsRegistered(const std::string& app_id) const {
  if ((!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) &&
      !default_apps_->HasApp(app_id))
    return false;

  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);
  return apps.FindDict(app_id);
}

bool ArcAppListPrefs::IsDefault(const std::string& app_id) const {
  return default_apps_->HasApp(app_id);
}

bool ArcAppListPrefs::IsOem(const std::string& app_id) const {
  const ArcDefaultAppList::AppInfo* app_info = default_apps_->GetApp(app_id);
  return app_info && app_info->oem;
}

bool ArcAppListPrefs::IsShortcut(const std::string& app_id) const {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
  return app_info && app_info->shortcut;
}

bool ArcAppListPrefs::IsControlledByPolicy(
    const std::string& package_name) const {
  return packages_by_policy_.count(package_name);
}

bool ArcAppListPrefs::IsAbleToBeLaunched(const std::string& app_id) const {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
  return app_info && !app_info->suspended && app_info->ready &&
         !app_info->need_fixup;
}

base::Time ArcAppListPrefs::PollLaunchRequestTime(const std::string& app_id) {
  if (!launch_request_times_.count(app_id))
    return base::Time();

  const base::Time last_launch_time = launch_request_times_[app_id];
  // This value should only be used once per launch.
  launch_request_times_.erase(app_id);
  return last_launch_time;
}

void ArcAppListPrefs::SetLaunchRequestTimeForTesting(const std::string& app_id,
                                                     base::Time timestamp) {
  launch_request_times_[app_id] = timestamp;
}
void ArcAppListPrefs::SetLastLaunchTime(const std::string& app_id) {
  if (!IsRegistered(app_id)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  launch_request_times_[app_id] = base::Time::Now();
  SetLastLaunchTimeInternal(app_id);
}

void ArcAppListPrefs::SetLastLaunchTimeInternal(const std::string& app_id) {
  // Usage time on hidden should not be tracked.
  if (!arc::ShouldShowInLauncher(app_id))
    return;

  const base::Time time = base::Time::Now();
  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::Value::Dict& app_dict = update.Get();
  const std::string string_value = base::NumberToString(time.ToInternalValue());
  app_dict.Set(kLastLaunchTime, string_value);

  for (auto& observer : observer_list_)
    observer.OnAppLastLaunchTimeUpdated(app_id);

  if (first_launch_app_request_) {
    first_launch_app_request_ = false;
    // UI Shown time may not be set in unit tests.
    const user_manager::UserManager* user_manager =
        user_manager::UserManager::Get();
    if (arc::ArcSessionManager::Get()->skipped_terms_of_service_negotiation() &&
        !user_manager->IsLoggedInAsKioskApp() &&
        !ash::UserSessionManager::GetInstance()->ui_shown_time().is_null()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Arc.FirstAppLaunchRequest.TimeDelta",
          time - ash::UserSessionManager::GetInstance()->ui_shown_time(),
          base::Seconds(1), base::Minutes(2), 20);
    }
  }
}

void ArcAppListPrefs::SetLastLaunchTimeForTesting(const std::string& app_id,
                                                  base::Time timestamp) {
  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::Value::Dict& app_dict = update.Get();
  const std::string string_value =
      base::NumberToString(timestamp.ToInternalValue());
  app_dict.Set(kLastLaunchTime, string_value);
}

void ArcAppListPrefs::DisableAllApps() {
  std::unordered_set<std::string> old_ready_apps;
  old_ready_apps.swap(ready_apps_);
  for (auto& app_id : old_ready_apps)
    NotifyAppStatesChanged(app_id);
}

void ArcAppListPrefs::NotifyRegisteredApps() {
  if (apps_restored_)
    return;

  DCHECK(ready_apps_.empty());
  std::vector<std::string> app_ids = GetAppIdsNoArcEnabledCheck();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<AppInfo> app_info = GetApp(app_id);
    if (!app_info) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    // Default apps are reported earlier.
    if (tracked_apps_.insert(app_id).second) {
      for (auto& observer : observer_list_)
        observer.OnAppRegistered(app_id, *app_info);
    }
  }

  apps_restored_ = true;
}

void ArcAppListPrefs::RemoveAllAppsAndPackages() {
  std::vector<std::string> app_ids = GetAppIdsNoArcEnabledCheck();
  for (const auto& app_id : app_ids) {
    if (!default_apps_->HasApp(app_id)) {
      RemoveApp(app_id);
    } else {
      if (ready_apps_.count(app_id)) {
        ready_apps_.erase(app_id);
        NotifyAppStatesChanged(app_id);
      }
    }
  }
  DCHECK(ready_apps_.empty());

  const std::vector<std::string> package_names_to_remove =
      GetPackagesFromPrefs(false /* check_arc_alive */, true /* installed */);
  for (const auto& package_name : package_names_to_remove) {
    RemovePackageFromPrefs(package_name);
    for (auto& observer : observer_list_)
      observer.OnPackageRemoved(package_name, false);
  }

  if (!remove_all_callback_for_testing_.is_null())
    std::move(remove_all_callback_for_testing_).Run();
  is_remove_all_in_progress_ = false;
}

void ArcAppListPrefs::OnArcPlayStoreEnabledChanged(bool enabled) {
  SetDefaultAppsFilterLevel();

  // TODO(victorhsieh): Implement opt-in and opt-out.
  if (arc::ShouldArcAlwaysStart())
    return;

  if (enabled) {
    NotifyRegisteredApps();
  } else {
    is_remove_all_in_progress_ = true;
    // Call RemoveAllAppsAndPackages asynchronous to ensure the
    // arc::prefs::kArcEnabled pref change callbacks are called for all other
    // components before calling RemoveAllAppsAndPackages for other components
    // to prepare for ARC apps removal.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ArcAppListPrefs::RemoveAllAppsAndPackages,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcAppListPrefs::OnArcSessionStopped(arc::ArcStopReason stop_reason) {
  arc_app_metrics_util_->reportMetrics();
}

void ArcAppListPrefs::SetDefaultAppsFilterLevel() {
  // There is no a blocklisting mechanism for Android apps. Until there is
  // one, we have no option but to ban all pre-installed apps on Android side.
  // Match this requirement and don't show pre-installed apps for managed users
  // in app list.
  if (arc::policy_util::IsAccountManaged(profile_)) {
    if (profile_->IsChild() || ash::switches::IsTabletFormFactor()) {
      // For child accounts, filter only optional apps.
      // For tablet form factor devices, filter only optional apps.
      default_apps_->set_filter_level(
          ArcDefaultAppList::FilterLevel::OPTIONAL_APPS);
    } else {
      default_apps_->set_filter_level(
          arc::IsArcPlayStoreEnabledForProfile(profile_)
              ? ArcDefaultAppList::FilterLevel::OPTIONAL_APPS
              : ArcDefaultAppList::FilterLevel::ALL);
    }
  } else {
    default_apps_->set_filter_level(ArcDefaultAppList::FilterLevel::NOTHING);
  }

  // Register default apps if it was not registered before.
  RegisterDefaultApps();
}

void ArcAppListPrefs::OnDefaultAppsReady() {
  VLOG(1) << "Default apps ready";

  SetDefaultAppsFilterLevel();
  default_apps_ready_ = true;
  if (!default_apps_ready_callback_.is_null())
    std::move(default_apps_ready_callback_).Run();

  StartPrefs();
  RecordAppIdsUma();
}

void ArcAppListPrefs::RecordAppIdsUma() {
  // Default apps are the ones that have app icons even before opting into ARC.
  // Play Store, Play Games, and PAI apps are good examples. This one can be
  // 1 or more even when ARC is opted out.
  size_t num_default_apps = 0;
  // Sticky apps are the ones in either system or vendor image. They are called
  // "sticky" because uninstalling them is not possible. 0 for opt-out users.
  size_t num_sticky_apps = 0;
  // "Installed" apps are the ones that the user has manually installed. This
  // includes apps installed by Chrome's app sync feature. 0 for opt-out users.
  size_t num_installed_apps = 0;
  // Number of apps that is a vpn_provider.
  size_t num_vpn_apps = 0;

  const std::vector<std::string> app_ids = GetAppIds();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<AppInfo> app_info = GetApp(app_id);
    DCHECK(app_info) << app_id;
    if (!app_info)
      continue;
    const bool is_default = IsDefault(app_id);
    const bool is_sticky = app_info->sticky;
    DVLOG(1) << "App ID on startup: name=" << app_info->name
             << ", package=" << app_info->package_name
             << ", activity=" << app_info->activity << ", sticky=" << is_sticky
             << ", default=" << is_default;
    if (is_default || is_sticky) {
      // Some apps, such as com.android.vending, can be both default and sticky.
      if (is_default)
        ++num_default_apps;
      if (is_sticky)
        ++num_sticky_apps;
    } else {
      ++num_installed_apps;
    }
    auto package = GetPackage(app_info->package_name);
    if (package && package->vpn_provider) {
      ++num_vpn_apps;
    }
  }

  const bool has_installed_apps = num_installed_apps;
  VLOG(1) << "Non-PAI (aka non-default) and non-sticky (aka"
          << " not-in-system/vendor-images) ARC app(s) are "
          << (has_installed_apps ? "" : "not ") << "found.";

  // Record the UMA. For more context of the metrics, see b/219115916.
  base::UmaHistogramExactLinear(
      base::StrCat({kAppCountUmaPrefix, "DefaultApp"}), num_default_apps,
      kAppCountUmaExclusiveMax);
  base::UmaHistogramExactLinear(base::StrCat({kAppCountUmaPrefix, "StickyApp"}),
                                num_sticky_apps, kAppCountUmaExclusiveMax);
  base::UmaHistogramExactLinear(
      base::StrCat({kAppCountUmaPrefix, "InstalledApp"}), num_installed_apps,
      kAppCountUmaExclusiveMax);
  base::UmaHistogramExactLinear(base::StrCat({kAppCountUmaPrefix, "VpnApp"}),
                                num_vpn_apps, kAppCountUmaExclusiveMaxLower);
  base::UmaHistogramBoolean(
      base::StrCat({kAppCountUmaPrefix, "HasInstalledOrUnknownApp"}),
      has_installed_apps);
}

void ArcAppListPrefs::RecordAppCategoryDataSizeListUma(
    std::vector<arc::mojom::AppInfoPtr> apps) {
  if (app_category_data_size_uma_recorded_) {
    // "Arc.Data.AppCategory.{Target}.DataSize" should be recorded only once
    // per session.
    return;
  }

  // Calculate combined data bytes for each app category.
  std::map<arc::mojom::AppCategory, uint64_t> data_bytes_map;
  for (const auto& app : apps) {
    if (app->app_storage.is_null()) {
      continue;
    }
    data_bytes_map[app->app_category] += app->app_storage->data_size_in_bytes;
  }

  VLOG(1) << "Recording Arc.Data.AppCategory.{Target}.DataSize UMA";
  RecordAppCategoryDataSizeUma("Game", data_bytes_map[AppCategory::kGame]);
  RecordAppCategoryDataSizeUma("Audio", data_bytes_map[AppCategory::kAudio]);
  RecordAppCategoryDataSizeUma("Video", data_bytes_map[AppCategory::kVideo]);
  RecordAppCategoryDataSizeUma("Image", data_bytes_map[AppCategory::kImage]);
  RecordAppCategoryDataSizeUma("Social", data_bytes_map[AppCategory::kSocial]);
  RecordAppCategoryDataSizeUma("Productivity",
                               data_bytes_map[AppCategory::kProductivity]);
  const uint64_t data_bytes_for_other_category =
      data_bytes_map[AppCategory::kUndefined] +
      data_bytes_map[AppCategory::kNews] + data_bytes_map[AppCategory::kMaps] +
      data_bytes_map[AppCategory::kAccessibility];
  RecordAppCategoryDataSizeUma("Other", data_bytes_for_other_category);

  app_category_data_size_uma_recorded_ = true;
}

void ArcAppListPrefs::OnPolicySent(const std::string& policy) {
  // Update set of packages installed by policy.
  packages_by_policy_ =
      arc::policy_util::GetRequestedPackagesFromArcPolicy(policy);
}

arc::mojom::ArcResizeLockState ArcAppListPrefs::GetResizeLockState(
    const std::string& app_id) const {
  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return arc::mojom::ArcResizeLockState::UNDEFINED;
  }

  return app_info->resize_lock_state;
}

arc::mojom::AppCategory ArcAppListPrefs::GetAppCategory(
    const std::string& app_id) const {
  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return arc::mojom::AppCategory::kUndefined;
  }
  return app_info->app_category;
}

arc::ArcPackageInstallPriorityHandler*
ArcAppListPrefs::GetInstallPriorityHandler() {
  return install_priority_handler_.get();
}

void ArcAppListPrefs::SetAppLocale(const std::string& package_name,
                                   const std::string& selected_locale) {
  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  base::Value::Dict& package_dict = update.Get();
  package_dict.EnsureDict(kLocaleInfo)->Set(kSelectedLocale, selected_locale);

  const std::string& app_id = GetAppIdByPackageName(package_name);
  NotifyAppStatesChanged(app_id);
}

void ArcAppListPrefs::SetResizeLockState(const std::string& app_id,
                                         arc::mojom::ArcResizeLockState state) {
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to set ret resize lock for non-registered app:"
            << app_id << ".";
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* compatibility_mode =
      arc_service_manager->arc_bridge_service()->compatibility_mode();
  if (!compatibility_mode->IsConnected())
    return;
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(compatibility_mode, SetResizeLockState);
  if (!instance)
    return;

  instance->SetResizeLockState(app_info->package_name, state);

  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::Value::Dict& app_dict = update.Get();
  app_dict.Set(kResizeLockState, static_cast<int32_t>(state));

  // If the app is not "ready", we shouldn't fire the AppStatesChanged
  // callbacks. Otherwise, it would cause a crash (See crbug.com/1276603). When
  // the app is changed to "ready", ArcAppListPrefs sends the notifications
  // afterwards so it's fine not to fire it here.
  if (app_info->ready)
    NotifyAppStatesChanged(app_id);
}

bool ArcAppListPrefs::GetResizeLockNeedsConfirmation(
    const std::string& app_id) {
  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return true;
  }

  return app_info->resize_lock_needs_confirmation;
}

void ArcAppListPrefs::SetResizeLockNeedsConfirmation(const std::string& app_id,
                                                     bool is_needed) {
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to set resize lock confirmation for non-registered app:"
            << app_id << ".";
    return;
  }

  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::Value::Dict& app_dict = update.Get();
  app_dict.Set(kResizeLockNeedsConfirmation, is_needed);
}

int ArcAppListPrefs::GetShowSplashScreenDialogCount() const {
  return profile_->GetPrefs()->GetInteger(
      arc::prefs::kArcShowResizeLockSplashScreenLimits);
}

void ArcAppListPrefs::SetShowSplashScreenDialogCount(int count) {
  profile_->GetPrefs()->SetInteger(
      arc::prefs::kArcShowResizeLockSplashScreenLimits, count);
}

std::string ArcAppListPrefs::GetAppPackageName(const std::string& app_id) {
  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return std::string();
  }
  return app_info->package_name;
}

void ArcAppListPrefs::Shutdown() {
  arc::ArcPolicyBridge* policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  if (policy_bridge)
    policy_bridge->RemoveObserver(this);

  // TODO(lgcheng) remove the check once the feature is enabled.
  if (install_priority_handler_) {
    install_priority_handler_->Shutdown();
  }
}

void ArcAppListPrefs::RegisterDefaultApps() {
  // Report default apps first, note, app_map includes uninstalled and filtered
  // out apps as well.
  for (const auto& default_app : default_apps_->GetActiveApps()) {
    const std::string& app_id = default_app.first;
    DCHECK(default_apps_->HasApp(app_id));
    // Skip already tracked app.
    if (tracked_apps_.count(app_id)) {
      // Notify that icon is ready for default app.
      for (auto& observer : observer_list_) {
        for (const auto& descriptor : active_icons_[app_id])
          observer.OnAppIconUpdated(app_id, descriptor);
      }
      continue;
    }

    const ArcDefaultAppList::AppInfo& app_info = *default_app.second;
    AddAppAndShortcut(
        app_info.name, app_info.package_name, app_info.activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        std::nullopt /* version name */, false /* sticky */,
        false /* notifications_enabled */, false /* app_ready */,
        false /* suspended */, false /* shortcut */, true /* launchable */,
        false /* need_fixup */, ArcAppListPrefs::WindowLayout(),
        std::nullopt /* app_size */, std::nullopt /* data_size */,
        GetAppCategory(app_id));
  }
}

base::Value* ArcAppListPrefs::GetPackagePrefs(const std::string& package_name,
                                              const std::string& key) {
  if (!GetPackage(package_name)) {
    LOG(ERROR) << package_name << " can not be found.";
    return nullptr;
  }
  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  return update->Find(key);
}

void ArcAppListPrefs::SetPackagePrefs(const std::string& package_name,
                                      const std::string& key,
                                      base::Value value) {
  if (!GetPackage(package_name)) {
    LOG(ERROR) << package_name << " can not be found.";
    return;
  }
  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  update->Set(key, std::move(value));
}

void ArcAppListPrefs::SetDefaultAppsReadyCallback(base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  DCHECK(default_apps_ready_callback_.is_null());
  default_apps_ready_callback_ = std::move(callback);
  if (default_apps_ready_)
    std::move(default_apps_ready_callback_).Run();
}

void ArcAppListPrefs::SimulateDefaultAppAvailabilityTimeoutForTesting() {
  if (!detect_default_app_availability_timeout_.IsRunning())
    return;
  detect_default_app_availability_timeout_.Stop();
  DetectDefaultAppAvailability();
}

void ArcAppListPrefs::SetRemoveAllCallbackForTesting(
    base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  remove_all_callback_for_testing_ = std::move(callback);
}

void ArcAppListPrefs::OnConnectionReady() {
  VLOG(1) << "App instance connection is ready.";
  // Note, sync_service_ may be nullptr in testing.
  sync_service_ = arc::ArcPackageSyncableService::Get(profile_);
  is_initialized_ = false;

  if (!app_list_refreshed_callback_.is_null())
    std::move(app_list_refreshed_callback_).Run();
  for (auto& observer : observer_list_)
    observer.OnAppConnectionReady();
}

void ArcAppListPrefs::OnConnectionClosed() {
  VLOG(1) << "App instance connection is closed.";
  DisableAllApps();
  installing_packages_count_ = 0;
  packages_to_be_added_.clear();
  apps_installations_.clear();
  CancelDefaultAppLoadingTimeout();
  ClearIconRequestRecord();

  if (sync_service_) {
    sync_service_->StopSyncing(syncer::ARC_PACKAGE);
    sync_service_ = nullptr;
  }

  is_initialized_ = false;
  package_list_initial_refreshed_ = false;
  app_list_refreshed_callback_.Reset();

  // TODO(lgcheng) remove the check once the feature is enabled.
  if (install_priority_handler_) {
    install_priority_handler_->Clear();
  }

  for (auto& observer : observer_list_)
    observer.OnAppConnectionClosed();
}

void ArcAppListPrefs::HandleTaskCreated(const std::optional<std::string>& name,
                                        const std::string& package_name,
                                        const std::string& activity) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  const std::string app_id = GetAppId(package_name, activity);
  if (IsRegistered(app_id)) {
    SetLastLaunchTimeInternal(app_id);
  } else {
    // Create runtime app entry that is valid for the current user session. This
    // entry is not shown in App Launcher and only required for shelf
    // integration.
    AddAppAndShortcut(
        name.value_or(std::string()), package_name, activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        std::nullopt /* version_name */, false /* sticky */,
        false /* notifications_enabled */, true /* app_ready */,
        false /* suspended */, false /* shortcut */, false /* launchable */,
        false /* need_fixup */, ArcAppListPrefs::WindowLayout(),
        std::nullopt /* app_size */, std::nullopt /* data_size */,
        GetAppCategory(app_id));
  }
}

void ArcAppListPrefs::AddAppAndShortcut(
    const std::string& name,
    const std::string& package_name,
    const std::string& activity,
    const std::string& intent_uri,
    const std::string& icon_resource_id,
    const std::optional<std::string>& version_name,
    const bool sticky,
    const bool notifications_enabled,
    const bool app_ready,
    const bool suspended,
    const bool shortcut,
    const bool launchable,
    const bool need_fixup,
    const WindowLayout& initial_window_layout,
    const std::optional<uint64_t> app_size_in_bytes,
    const std::optional<uint64_t> data_size_in_bytes,
    const arc::mojom::AppCategory app_category) {
  const std::string app_id = shortcut ? GetAppId(package_name, intent_uri)
                                      : GetAppId(package_name, activity);

  // Do not add Play Store in certain conditions.
  if (app_id == arc::kPlayStoreAppId) {
    // Users can only use admin-approved and installed apps on the
    // reven board, not from the Play Store.
    if (ash::switches::IsRevenBranding()) {
      return;
    }

    // TODO(khmel): Use show_in_launcher flag to hide the Play Store app.
    // Display Play Store if we are in Demo Mode.
    // TODO(b/154290639): Remove check for |IsDemoModeOfflineEnrolled| when
    //                    fixed in Play Store.
    if (arc::IsRobotOrOfflineDemoAccountMode() &&
        !(ash::DemoSession::IsDeviceInDemoMode() &&
          ash::features::ShouldShowPlayStoreInDemoMode())) {
      return;
    }
  }

  std::string updated_name = name;
  // Add "(beta)" string to Play Store. See crbug.com/644576 for details.
  if (app_id == arc::kPlayStoreAppId)
    updated_name = l10n_util::GetStringUTF8(IDS_ARC_PLAYSTORE_ICON_TITLE_BETA);

  base::Time last_launch_time;
  const bool was_tracked = tracked_apps_.count(app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_old_info;
  if (was_tracked) {
    app_old_info = GetApp(app_id);
    DCHECK(app_old_info);
    DCHECK(launchable);
    last_launch_time = app_old_info->last_launch_time;
    if (updated_name != app_old_info->name) {
      for (auto& observer : observer_list_)
        observer.OnAppNameUpdated(app_id, updated_name);
    }
  }

  // Ensure to query the resize lock state from the prefs as we don't want the
  // default resize lock value (UNDEFINED) to override the existing value.
  const auto resize_lock_state = GetResizeLockState(app_id);
  const auto resize_lock_needs_confirmation =
      GetResizeLockNeedsConfirmation(app_id);

  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::Value::Dict& app_dict = update.Get();
  app_dict.Set(kName, updated_name);
  app_dict.Set(kPackageName, package_name);
  app_dict.Set(kActivity, activity);
  app_dict.Set(kIntentUri, intent_uri);
  app_dict.Set(kIconResourceId, icon_resource_id);
  app_dict.Set(kVersionName, version_name.value_or(std::string()));
  app_dict.Set(kSuspended, suspended);
  app_dict.Set(kSticky, sticky);
  app_dict.Set(kNotificationsEnabled, notifications_enabled);
  app_dict.Set(kResizeLockState, static_cast<int32_t>(resize_lock_state));
  app_dict.Set(kResizeLockNeedsConfirmation, resize_lock_needs_confirmation);
  app_dict.Set(kShortcut, shortcut);
  app_dict.Set(kLaunchable, launchable);
  app_dict.Set(kNeedFixup, need_fixup);
  app_dict.Set(kAppCategory, static_cast<int32_t>(app_category));

  app_dict.Set(kWindowLayout, WindowLayoutToDict(initial_window_layout));

  if (app_size_in_bytes.has_value())
    app_dict.Set(kAppSizeBytesString,
                 base::NumberToString(app_size_in_bytes.value()));
  if (data_size_in_bytes.has_value())
    app_dict.Set(kDataSizeBytesString,
                 base::NumberToString(data_size_in_bytes.value()));

  // Note the install time is the first time the Chrome OS sees the app, not the
  // actual install time in Android side.
  if (GetInstallTime(app_id).is_null()) {
    std::string install_time_str =
        base::NumberToString(base::Time::Now().ToInternalValue());
    app_dict.Set(kInstallTime, install_time_str);
  }

  const bool was_disabled = ready_apps_.count(app_id) == 0;
  DCHECK(!(!was_disabled && !app_ready));
  if (was_disabled && app_ready)
    ready_apps_.insert(app_id);

  AppInfo app_info(
      updated_name, package_name, activity, intent_uri, icon_resource_id,
      version_name, last_launch_time, GetInstallTime(app_id), sticky,
      notifications_enabled, resize_lock_state, resize_lock_needs_confirmation,
      initial_window_layout, app_ready, suspended,
      launchable && arc::ShouldShowInLauncher(app_id), shortcut, launchable,
      need_fixup, app_size_in_bytes, data_size_in_bytes, app_category);

  if (was_tracked) {
    if (AreAppStatesChanged(*app_old_info, app_info)) {
      for (auto& observer : observer_list_)
        observer.OnAppStatesChanged(app_id, app_info);
    }
  } else {
    for (auto& observer : observer_list_)
      observer.OnAppRegistered(app_id, app_info);
    default_apps_->SetAppHidden(app_id, false);
    tracked_apps_.insert(app_id);

    // Newly installed apps are subject to ARC++ resize lock. Set the state to
    // READY so the lock will be turned on next time they are launched.
    SetResizeLockState(app_id, arc::mojom::ArcResizeLockState::READY);
  }

  // Send pending requests in case app becomes visible.
  if (!app_old_info || !app_old_info->ready) {
    for (const auto& descriptor : request_icon_recorded_[app_id])
      RequestIcon(
          app_id, descriptor,
          base::BindOnce(&ArcAppListPrefs::InstallIcon,
                         weak_ptr_factory_.GetWeakPtr(), app_id, descriptor));
  }

  if (app_ready) {
    const bool deferred_notifications_enabled =
        NotificationsEnabledDeferred(prefs_).Get(app_id);
    if (deferred_notifications_enabled)
      SetNotificationsEnabled(app_id, deferred_notifications_enabled);

    // Invalidate app icons in case it was already registered, becomes ready and
    // icon version is updated. This allows to use previous icons until new
    // icons are been prepared.
    const base::Value* existing_version = app_dict.Find(kIconVersion);
    if (was_tracked && (!existing_version ||
                        existing_version->GetInt() != current_icons_version)) {
      VLOG(1) << "Invalidate icons for " << app_id << " from "
              << (existing_version ? existing_version->GetInt() : -1) << " to "
              << current_icons_version;
      InvalidateAppIcons(app_id);
    }

    app_dict.Set(kIconVersion, base::Value(current_icons_version));

    if (arc::IsArcForceCacheAppIcon() && app_id != arc::kPlayStoreAppId) {
      // Request full set of app icons.
      VLOG(1) << "Requested full set of app icons " << app_id;
      for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
        for (int dip_size : default_app_icon_dip_sizes) {
          MaybeRequestIcon(app_id,
                           ArcAppIconDescriptor(dip_size, scale_factor));
        }
      }
    }
  }
  OnArcAppListRefreshed(profile_);
}

void ArcAppListPrefs::RemoveApp(const std::string& app_id) {
  // Delete cached icon if there is any.
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
  if (app_info && !app_info->icon_resource_id.empty())
    arc::RemoveCachedIcon(app_info->icon_resource_id);

  MaybeRemoveIconRequestRecord(app_id);

  // From now, app is not available.
  ready_apps_.erase(app_id);
  active_icons_.erase(app_id);

  // In case default app, mark it as hidden.
  default_apps_->SetAppHidden(app_id, true);

  // Remove asyncronously local data on file system.
  ScheduleAppFolderDeletion(app_id);

  // Remove from prefs.
  ScopedDictPrefUpdate apps_update(prefs_, arc::prefs::kArcApps);
  const bool removed = apps_update->Remove(app_id);
  DCHECK(removed);

  // |tracked_apps_| contains apps that are reported externally as available.
  // However, in case ARC++ appears as disbled on next start and had some apps
  // left in prefs from the previous session, app clean up is performed on very
  // early stage. Don't report |OnAppRemoved| in this case once the app was not
  // reported as available for the current session.
  if (!tracked_apps_.count(app_id))
    return;

  for (auto& observer : observer_list_)
    observer.OnAppRemoved(app_id);
  tracked_apps_.erase(app_id);
}

arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
ArcAppListPrefs::app_connection_holder() {
  // Some tests set their own holder. If it's set, return the holder.
  if (app_connection_holder_for_testing_)
    return app_connection_holder_for_testing_;
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  // The null check is for unit tests. On production, |arc_service_manager| is
  // always non-null.
  if (!arc_service_manager)
    return nullptr;
  return arc_service_manager->arc_bridge_service()->app();
}

void ArcAppListPrefs::AddOrUpdatePackagePrefs(
    const arc::mojom::ArcPackageInfo& package,
    const UpdatePackagePrefsReason& update_reason) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  const std::string& package_name = package.package_name;

  if (package_name.empty()) {
    VLOG(2) << "Package name cannot be empty.";
    return;
  }

  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  base::Value::Dict& package_dict = update.Get();
  const std::string id_str =
      base::NumberToString(package.last_backup_android_id);
  const std::string time_str = base::NumberToString(package.last_backup_time);

  int old_package_version = package_dict.FindInt(kPackageVersion).value_or(-1);
  package_dict.Set(kShouldSync, package.sync);
  package_dict.Set(kPackageVersion, package.package_version);
  package_dict.Set(kLastBackupAndroidId, id_str);
  package_dict.Set(kLastBackupTime, time_str);
  package_dict.Set(kUninstalled, false);
  package_dict.Set(kVPNProvider, package.vpn_provider);
  package_dict.Set(kPreinstalled, package.preinstalled);
  package_dict.Set(kGameControlsOptOut, package.game_controls_opt_out);
  if (package.version_name)
    package_dict.Set(kVersionName, package.version_name.value());
  else
    package_dict.Set(kVersionName, std::string());

  base::Value::Dict permissions_dict;
  if (package.permission_states.has_value()) {
    for (const auto& [permission_type, permission_state] :
         package.permission_states.value()) {
      base::Value::Dict permission_state_dict;
      permission_state_dict.Set(kPermissionStateGranted,
                                permission_state->granted);
      permission_state_dict.Set(kPermissionStateManaged,
                                permission_state->managed);

      if (permission_state->details.has_value()) {
        permission_state_dict.Set(kPermissionStateDetails,
                                  permission_state->details.value());
      }
      permission_state_dict.Set(kPermissionStateOneTime,
                                permission_state->one_time);

      permissions_dict.Set(
          base::NumberToString(static_cast<int64_t>(permission_type)),
          std::move(permission_state_dict));
    }
    package_dict.Set(kPermissionStates, std::move(permissions_dict));
  } else {
    // Remove kPermissionStates from dict if there are no permissions.
    package_dict.Remove(kPermissionStates);
  }

  if (package.web_app_info) {
    const arc::mojom::WebAppInfo& web_app_info = *package.web_app_info;
    base::Value::Dict web_app_info_dict;
    web_app_info_dict.Set(kTitle, web_app_info.title);
    web_app_info_dict.Set(kStartUrl, web_app_info.start_url);
    web_app_info_dict.Set(kScopeUrl, web_app_info.scope_url);
    web_app_info_dict.Set(kThemeColor,
                          base::NumberToString(web_app_info.theme_color));
    web_app_info_dict.Set(kIsWebOnlyTwa, web_app_info.is_web_only_twa);
    if (const auto& fingerprint = web_app_info.certificate_sha256_fingerprint) {
      web_app_info_dict.Set(kCertificateSha256Fingerprint, *fingerprint);
    }
    package_dict.Set(kWebAppInfo, std::move(web_app_info_dict));
  } else {
    package_dict.Remove(kWebAppInfo);
  }

  if (package.locale_info &&
      base::FeatureList::IsEnabled(arc::kPerAppLanguage)) {
    if (IsSelectedLocaleResyncRequired(package_dict, *package.locale_info,
                                       update_reason)) {
      // Rejects ARC prefs and sends the correct locale back to Android to
      // ensure eventual correctness.
      const base::Value::Dict* locale_info_dict =
          package_dict.EnsureDict(kLocaleInfo);
      const std::string* saved_selected_locale =
          locale_info_dict->FindString(kSelectedLocale);
      arc::mojom::AppInstance* app_instance =
          (arc::ArcServiceManager::Get()
               ? ARC_GET_INSTANCE_FOR_METHOD(
                     arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                     SetAppLocale)
               : nullptr);
      if (app_instance) {
        app_instance->SetAppLocale(package_name, *saved_selected_locale);
      }
    } else {
      // Accepts ARC prefs and save to dict.
      base::Value::List supported_locales;
      const arc::mojom::PackageLocaleInfo& package_locale_info =
          *package.locale_info;
      for (const std::string& supported_locale :
           package_locale_info.supported_locales) {
        if (IsLocaleTagValid(supported_locale)) {
          supported_locales.Append(supported_locale);
        }
      }
      const auto& selected_locale = package_locale_info.selected_locale;
      package_dict.Set(
          kLocaleInfo,
          base::Value::Dict()
              .Set(kSupportedLocales, std::move(supported_locales))
              .Set(kSelectedLocale,
                   IsLocaleTagValid(selected_locale) ? selected_locale : ""));
    }
  } else {
    package_dict.Remove(kLocaleInfo);
  }

  if (old_package_version == -1 ||
      old_package_version == package.package_version) {
    return;
  }

  InvalidatePackageIcons(package_name);
}

void ArcAppListPrefs::RemovePackageFromPrefs(const std::string& package_name) {
  ScopedDictPrefUpdate(prefs_, arc::prefs::kArcPackages)->Remove(package_name);
  OnArcAppListRefreshed(profile_);
}

void ArcAppListPrefs::OnAppListRefreshed(
    std::vector<arc::mojom::AppInfoPtr> apps) {
  DCHECK(app_list_refreshed_callback_.is_null());
  if (!app_connection_holder()->IsConnected()) {
    LOG(ERROR) << "App instance is not connected. Delaying app list refresh. "
               << "See b/70566216.";
    app_list_refreshed_callback_ =
        base::BindOnce(&ArcAppListPrefs::OnAppListRefreshed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(apps));
    return;
  }

  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  std::vector<std::string> old_apps = GetAppIds();

  ready_apps_.clear();
  for (const auto& app : apps) {
    std::optional<uint64_t> app_size_in_bytes;
    std::optional<uint64_t> data_size_in_bytes;

    if (!app->app_storage.is_null()) {
      app_size_in_bytes = app->app_storage->app_size_in_bytes;
      data_size_in_bytes = app->app_storage->data_size_in_bytes;
    }

    AddAppAndShortcut(
        app->name, app->package_name, app->activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        app->version_name, app->sticky, app->notifications_enabled,
        true /* app_ready */, app->suspended, false /* shortcut */,
        true /* launchable */, app->need_fixup, WindowLayoutFromApp(*app),
        app_size_in_bytes, data_size_in_bytes, app->app_category);
  }

  // Detect removed ARC apps after current refresh.
  for (const auto& app_id : old_apps) {
    if (ready_apps_.count(app_id))
      continue;

    if (IsShortcut(app_id)) {
      // If this is a shortcut, we just mark it as ready.
      ready_apps_.insert(app_id);
      NotifyAppStatesChanged(app_id);
    } else {
      // Default apps may not be installed yet at this moment.
      if (!default_apps_->HasApp(app_id))
        RemoveApp(app_id);
    }
  }

  if (!is_initialized_) {
    is_initialized_ = true;

    UMA_HISTOGRAM_COUNTS_1000("Arc.AppsInstalledAtStartup", ready_apps_.size());

    arc::ArcPaiStarter* pai_starter =
        arc::ArcSessionManager::Get()->pai_starter();

    if (pai_starter) {
      pai_starter->AddOnStartCallback(
          base::BindOnce(&ArcAppListPrefs::MaybeSetDefaultAppLoadingTimeout,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      MaybeSetDefaultAppLoadingTimeout();
    }
  }

  RecordAppCategoryDataSizeListUma(std::move(apps));

  OnArcAppListRefreshed(profile_);
}

void ArcAppListPrefs::DetectDefaultAppAvailability() {
  for (const auto& package : default_apps_->GetActivePackages()) {
    // Check if already installed or installation in progress.
    if (!GetPackage(package) && !apps_installations_.count(package))
      HandlePackageRemoved(package);
  }
}

void ArcAppListPrefs::MaybeSetDefaultAppLoadingTimeout() {
  // Don't check if anything is installing or scheduled right now.
  if (installing_packages_count_)
    return;

  // Find at least one not installed default app package.
  for (const auto& package : default_apps_->GetActivePackages()) {
    if (!GetPackage(package)) {
      detect_default_app_availability_timeout_.Start(
          FROM_HERE, kDetectDefaultAppAvailabilityTimeout, this,
          &ArcAppListPrefs::DetectDefaultAppAvailability);
      break;
    }
  }
}

void ArcAppListPrefs::CancelDefaultAppLoadingTimeout() {
  detect_default_app_availability_timeout_.Stop();
}

void ArcAppListPrefs::AddApp(const arc::mojom::AppInfo& app_info) {
  if ((app_info.name.empty() || app_info.package_name.empty() ||
       app_info.activity.empty())) {
    VLOG(2) << "App Name, package name, and activity cannot be empty.";
    return;
  }

  std::optional<uint64_t> app_size_in_bytes;
  std::optional<uint64_t> data_size_in_bytes;

  if (!app_info.app_storage.is_null()) {
    app_size_in_bytes = app_info.app_storage->app_size_in_bytes;
    data_size_in_bytes = app_info.app_storage->data_size_in_bytes;
  }

  AddAppAndShortcut(
      app_info.name, app_info.package_name, app_info.activity,
      std::string() /* intent_uri */, std::string() /* icon_resource_id */,
      app_info.version_name, app_info.sticky, app_info.notifications_enabled,
      true /* app_ready */, app_info.suspended, false /* shortcut */,
      true /* launchable */, app_info.need_fixup, WindowLayoutFromApp(app_info),
      app_size_in_bytes, data_size_in_bytes, app_info.app_category);
}

void ArcAppListPrefs::OnAppAddedDeprecated(arc::mojom::AppInfoPtr app) {
  AddApp(*app);
}

void ArcAppListPrefs::InvalidateAppIcons(const std::string& app_id) {
  // Ignore Play Store app since we provide its icon in Chrome resources.
  if (app_id == arc::kPlayStoreAppId)
    return;

  // Clean up previous icon records. They may refer to outdated icons.
  MaybeRemoveIconRequestRecord(app_id);

  // Clear icon cache that contains outdated icons.
  ScheduleAppFolderDeletion(app_id);

  // Re-request active icons.
  for (const auto& descriptor : active_icons_[app_id])
    MaybeRequestIcon(app_id, descriptor);
}

void ArcAppListPrefs::InvalidatePackageIcons(const std::string& package_name) {
  if (package_name == kFrameworkPackageName) {
    VLOG(1)
        << "Android framework was changed, refreshing icons for all packages";
    for (const auto& package_name_to_invalidate : GetPackagesFromPrefs()) {
      if (package_name_to_invalidate != kFrameworkPackageName)
        InvalidatePackageIcons(package_name_to_invalidate);
    }
  }
  for (const std::string& app_id : GetAppsForPackage(package_name))
    InvalidateAppIcons(app_id);
}

void ArcAppListPrefs::ScheduleAppFolderDeletion(const std::string& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteAppFolderFromFileThread, GetAppPath(app_id)));
}

void ArcAppListPrefs::OnPackageAppListRefreshed(
    const std::string& package_name,
    std::vector<arc::mojom::AppInfoPtr> apps) {
  if (package_name.empty()) {
    VLOG(2) << "Package name cannot be empty.";
    return;
  }

  std::unordered_set<std::string> apps_to_remove =
      GetAppsAndShortcutsForPackage(package_name,
                                    true, /* include_only_launchable_apps */
                                    false /* include_shortcuts */);

  for (const auto& app : apps) {
    const std::string app_id = GetAppId(app->package_name, app->activity);
    apps_to_remove.erase(app_id);

    AddApp(*app);
  }

  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  base::Value::Dict& package_dict = update.Get();
  if (!apps_to_remove.empty()) {
    auto* shelf_controller = ChromeShelfController::instance();
    if (shelf_controller) {
      int pin_index =
          shelf_controller->PinnedItemIndexByAppID(*apps_to_remove.begin());
      package_dict.Set(kPinIndex, pin_index);
    }
  }

  for (const auto& app_id : apps_to_remove)
    RemoveApp(app_id);
}

void ArcAppListPrefs::OnInstallShortcut(arc::mojom::ShortcutInfoPtr shortcut) {
  if ((shortcut->name.empty() || shortcut->intent_uri.empty())) {
    VLOG(2) << "Shortcut Name, and intent_uri cannot be empty.";
    return;
  }

  AddAppAndShortcut(
      shortcut->name, shortcut->package_name, std::string() /* activity */,
      shortcut->intent_uri, shortcut->icon_resource_id,
      std::nullopt /* version_name */, false /* sticky */,
      false /* notifications_enabled */, true /* app_ready */,
      false /* suspended */, true /* shortcut */, true /* launchable */,
      false /* need_fixup */, ArcAppListPrefs::WindowLayout(),
      std::nullopt /* app_size */, std::nullopt /* data_size */,
      GetAppCategory(GetAppId(shortcut->package_name, shortcut->intent_uri)));
}

void ArcAppListPrefs::OnUninstallShortcut(const std::string& package_name,
                                          const std::string& intent_uri) {
  std::vector<std::string> shortcuts_to_remove;
  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);
  for (const auto app : apps) {
    if (!app.second.is_dict()) {
      VLOG(2) << "Failed to extract information for " << app.first << ".";
      continue;
    }

    const std::string* installed_package_name =
        app.second.GetDict().FindString(kPackageName);
    const std::string* installed_intent_uri =
        app.second.GetDict().FindString(kIntentUri);
    if (!installed_package_name || !installed_intent_uri) {
      VLOG(2) << "Failed to extract information for " << app.first << ".";
      continue;
    }
    const bool shortcut =
        app.second.GetDict().FindBool(kShortcut).value_or(false);
    if (!shortcut || *installed_package_name != package_name ||
        *installed_intent_uri != intent_uri) {
      continue;
    }

    shortcuts_to_remove.push_back(app.first);
  }

  for (const auto& shortcut_id : shortcuts_to_remove)
    RemoveApp(shortcut_id);
}

std::unordered_set<std::string> ArcAppListPrefs::GetAppsForPackage(
    const std::string& package_name) const {
  return GetAppsAndShortcutsForPackage(package_name,
                                       false, /* include_only_launchable_apps */
                                       false /* include_shortcuts */);
}

std::unordered_set<std::string> ArcAppListPrefs::GetAppsAndShortcutsForPackage(
    const std::string& package_name,
    bool include_only_launchable_apps,
    bool include_shortcuts) const {
  std::unordered_set<std::string> app_set;
  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);
  for (const auto app : apps) {
    if (!crx_file::id_util::IdIsValid(app.first))
      continue;

    if (!app.second.is_dict()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    const std::string* app_package =
        app.second.GetDict().FindString(kPackageName);
    if (!app_package) {
      LOG(ERROR) << "App is malformed: " << app.first;
      continue;
    }

    if (package_name != *app_package)
      continue;

    if (!include_shortcuts) {
      if (app.second.GetDict().FindBool(kShortcut).value_or(false))
        continue;
    }

    if (include_only_launchable_apps) {
      // Filter out non-lauchable apps.
      if (!app.second.GetDict().FindBool(kLaunchable).value_or(false))
        continue;
    }

    app_set.insert(app.first);
  }

  return app_set;
}

void ArcAppListPrefs::HandlePackageRemoved(const std::string& package_name) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  const std::unordered_set<std::string> apps_to_remove =
      GetAppsAndShortcutsForPackage(package_name,
                                    false /* include_only_launchable_apps */,
                                    true /* include_shortcuts */);
  for (const auto& app_id : apps_to_remove)
    RemoveApp(app_id);

  RemovePackageFromPrefs(package_name);
}

void ArcAppListPrefs::OnPackageRemoved(const std::string& package_name) {
  UMA_HISTOGRAM_ENUMERATION("Arc.AppUninstallReason",
                            UninstallCounterReasonEnum::USER);
  HandlePackageRemoved(package_name);

  for (auto& observer : observer_list_)
    observer.OnPackageRemoved(package_name, true);
}

void ArcAppListPrefs::OnIcon(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor,
    base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)> callback,
    arc::mojom::RawIconPngDataPtr icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!icon || !icon->icon_png_data.has_value() ||
      icon->icon_png_data->empty()) {
    LOG(WARNING) << "Cannot fetch icon for " << app_id;
    std::move(callback).Run(nullptr);
    return;
  }

  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to update icon for non-registered app: " << app_id;
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(icon));
}

void ArcAppListPrefs::OnTaskCreated(int32_t task_id,
                                    const std::string& package_name,
                                    const std::string& activity,
                                    const std::optional<std::string>& name,
                                    const std::optional<std::string>& intent,
                                    int32_t session_id) {
  HandleTaskCreated(name, package_name, activity);
  for (auto& observer : observer_list_) {
    observer.OnTaskCreated(task_id, package_name, activity,
                           intent.value_or(std::string()), session_id);
  }
}

void ArcAppListPrefs::OnTaskDescriptionUpdated(
    int32_t task_id,
    const std::string& label,
    const std::vector<uint8_t>& icon_png_data) {
  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New();
  icon->is_adaptive_icon = false;
  icon->icon_png_data =
      std::vector<uint8_t>(icon_png_data.begin(), icon_png_data.end());
  for (auto& observer : observer_list_)
    observer.OnTaskDescriptionChanged(task_id, label, *icon, 0, 0);
}

void ArcAppListPrefs::OnTaskDescriptionChanged(
    int32_t task_id,
    const std::string& label,
    arc::mojom::RawIconPngDataPtr icon,
    uint32_t primary_color,
    uint32_t status_bar_color) {
  for (auto& observer : observer_list_) {
    observer.OnTaskDescriptionChanged(task_id, label, *icon, primary_color,
                                      status_bar_color);
  }
}

void ArcAppListPrefs::OnTaskDestroyed(int32_t task_id) {
  for (auto& observer : observer_list_)
    observer.OnTaskDestroyed(task_id);
}

void ArcAppListPrefs::OnTaskSetActive(int32_t task_id) {
  for (auto& observer : observer_list_)
    observer.OnTaskSetActive(task_id);
}

void ArcAppListPrefs::OnNotificationsEnabledChanged(
    const std::string& package_name,
    bool enabled) {
  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);
  for (const auto app : apps) {
    if (!app.second.is_dict()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    const std::string* app_package_name =
        app.second.GetDict().FindString(kPackageName);
    if (!app_package_name) {
      LOG(ERROR) << "App is malformed: " << app.first;
      continue;
    }
    if (*app_package_name != package_name) {
      continue;
    }
    arc::ArcAppScopedPrefUpdate update(prefs_, app.first, arc::prefs::kArcApps);
    base::Value::Dict& updating_app_dict = update.Get();
    updating_app_dict.Set(kNotificationsEnabled, enabled);
  }
  for (auto& observer : observer_list_)
    observer.OnNotificationsEnabledChanged(package_name, enabled);
}

bool ArcAppListPrefs::IsDefaultPackage(const std::string& package_name) const {
  DCHECK(default_apps_ready_);
  return default_apps_->HasPackage(package_name) ||
         default_apps_->HasHiddenPackage(package_name);
}

void ArcAppListPrefs::OnPackageAdded(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));

  AddOrUpdatePackagePrefs(*package_info,
                          UpdatePackagePrefsReason::kOnPackageAdded);

  packages_to_be_added_.erase(package_info->package_name);
  UpdateArcPackagesIsUpToDatePref();

  // TODO(lgcheng) remove the check once the feature is enabled.
  if (install_priority_handler_) {
    install_priority_handler_->ClearPackage(package_info->package_name);
  }

  for (auto& observer : observer_list_)
    observer.OnPackageInstalled(*package_info);
}

void ArcAppListPrefs::OnPackageModified(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  AddOrUpdatePackagePrefs(*package_info,
                          UpdatePackagePrefsReason::kOnPackageModified);
  for (auto& observer : observer_list_)
    observer.OnPackageModified(*package_info);
}

void ArcAppListPrefs::OnPackageListRefreshed(
    std::vector<arc::mojom::ArcPackageInfoPtr> packages) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));

  const base::flat_set<std::string> old_packages(GetPackagesFromPrefs());
  std::set<std::string> current_packages;

  for (const auto& package : packages) {
    AddOrUpdatePackagePrefs(*package,
                            UpdatePackagePrefsReason::kOnPackageListRefreshed);
    if (!base::Contains(old_packages, package->package_name)) {
      for (auto& observer : observer_list_)
        observer.OnPackageInstalled(*package);
    } else {
      MaybeRemoveDeprecatedPackagePrefs(arc::ArcAppScopedPrefUpdate(
          prefs_, package->package_name, arc::prefs::kArcPackages));
    }
    current_packages.insert(package->package_name);
  }

  for (const auto& package_name : old_packages) {
    if (!base::Contains(current_packages, package_name)) {
      RemovePackageFromPrefs(package_name);
      for (auto& observer : observer_list_)
        observer.OnPackageRemoved(package_name, false);
    }
  }

  UpdateArcPackagesIsUpToDatePref();

  package_list_initial_refreshed_ = true;
  for (auto& observer : observer_list_)
    observer.OnPackageListInitialRefreshed();
}

std::vector<std::string> ArcAppListPrefs::GetPackagesFromPrefs() const {
  return GetPackagesFromPrefs(true /* check_arc_alive */, true /* installed */);
}

std::vector<std::string> ArcAppListPrefs::GetPackagesFromPrefs(
    bool check_arc_alive,
    bool installed) const {
  std::vector<std::string> packages;
  if (check_arc_alive &&
      (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_))) {
    return packages;
  }

  const base::Value::Dict& package_prefs =
      prefs_->GetDict(arc::prefs::kArcPackages);
  for (const auto package : package_prefs) {
    if (!package.second.is_dict()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    const bool uninstalled =
        package.second.GetDict().FindBool(kUninstalled).value_or(false);
    if (installed != !uninstalled)
      continue;

    packages.push_back(package.first);
  }

  return packages;
}

base::Time ArcAppListPrefs::GetInstallTime(const std::string& app_id) const {
  const base::Value::Dict& apps = prefs_->GetDict(arc::prefs::kArcApps);

  const base::Value::Dict* app = apps.FindDict(app_id);
  if (!app)
    return base::Time();

  const std::string* install_time_str = app->FindString(kInstallTime);
  if (!install_time_str)
    return base::Time();

  int64_t install_time_i64;
  if (!base::StringToInt64(*install_time_str, &install_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(install_time_i64);
}

void ArcAppListPrefs::InstallIcon(const std::string& app_id,
                                  const ArcAppIconDescriptor& descriptor,
                                  arc::mojom::RawIconPngDataPtr icon) {
  if (!icon) {
    return;
  }

  const base::FilePath icon_path = GetIconPath(app_id, descriptor);
  const base::FilePath foreground_icon_path =
      GetForegroundIconPath(app_id, descriptor);
  const base::FilePath background_icon_path =
      GetBackgroundIconPath(app_id, descriptor);
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InstallIconFromFileThread, icon_path,
                     foreground_icon_path, background_icon_path,
                     std::move(icon)),
      base::BindOnce(&ArcAppListPrefs::OnIconInstalled,
                     weak_ptr_factory_.GetWeakPtr(), app_id, descriptor));
}

void ArcAppListPrefs::OnIconInstalled(const std::string& app_id,
                                      const ArcAppIconDescriptor& descriptor,
                                      bool install_succeed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!install_succeed)
    return;

  for (auto& observer : observer_list_)
    observer.OnAppIconUpdated(app_id, descriptor);
}

void ArcAppListPrefs::OnInstallationStarted(
    const std::optional<std::string>& package_name) {
  ++installing_packages_count_;
  CancelDefaultAppLoadingTimeout();
  UpdateArcPackagesIsUpToDatePref();

  if (!package_name.has_value())
    return;

  apps_installations_.insert(*package_name);

  // Track install start time IFF app sync metrics are also being recorded
  // and app is not a synced or default app. App sync metrics are only
  // recorded if this is the initial session after opting in during the sync
  // consent screen
  if (prefs_->GetBoolean(ash::prefs::kRecordArcAppSyncMetrics) &&
      !(sync_service_ && sync_service_->IsPackageSyncing(*package_name)) &&
      !IsDefaultPackage(*package_name)) {
    arc_app_metrics_util_->recordAppInstallStartTime(
        *package_name, IsControlledByPolicy(*package_name));
  }
  for (auto& observer : observer_list_)
    observer.OnInstallationStarted(*package_name);
}

void ArcAppListPrefs::OnInstallationProgressChanged(
    const std::string& package_name,
    float progress) {
  for (auto& observer : observer_list_) {
    observer.OnInstallationProgressChanged(package_name, progress);
  }
}

void ArcAppListPrefs::OnInstallationActiveChanged(
    const std::string& package_name,
    bool active) {
  for (auto& observer : observer_list_) {
    observer.OnInstallationActiveChanged(package_name, active);
  }
}

void ArcAppListPrefs::OnInstallationFinished(
    arc::mojom::InstallationResultPtr result) {
  if (result) {
    apps_installations_.erase(result->package_name);
    if (default_apps_->HasPackage(result->package_name) && !result->success &&
        !GetPackage(result->package_name)) {
      HandlePackageRemoved(result->package_name);
    }
    for (auto& observer : observer_list_)
      observer.OnInstallationFinished(result->package_name, result->success,
                                      result->is_launchable_app);
    if (result->success) {
      InstallationCounterReasonEnum reason =
          InstallationCounterReasonEnum::USER;
      std::string app_id = GetAppIdByPackageName(result->package_name);
      if (IsOem(app_id)) {
        reason = InstallationCounterReasonEnum::OEM;
      } else if (IsDefault(app_id)) {
        reason = InstallationCounterReasonEnum::DEFAULT;
      } else if (IsControlledByPolicy(result->package_name)) {
        reason = InstallationCounterReasonEnum::POLICY;
      }
      UMA_HISTOGRAM_ENUMERATION("Arc.AppInstalledReason", reason);
      arc_app_metrics_util_->maybeReportInstallTimeDelta(
          result->package_name, IsControlledByPolicy(result->package_name));
      packages_to_be_added_.insert(result->package_name);
    }
  }

  if (!installing_packages_count_) {
    VLOG(2) << "Received unexpected installation finished event";
    return;
  }
  --installing_packages_count_;
  MaybeSetDefaultAppLoadingTimeout();
  UpdateArcPackagesIsUpToDatePref();
}

void ArcAppListPrefs::NotifyAppStatesChanged(const std::string& app_id) {
  std::unique_ptr<AppInfo> app_info = GetAppFromPrefs(app_id);
  CHECK(app_info);
  for (auto& observer : observer_list_)
    observer.OnAppStatesChanged(app_id, *app_info);
}

// static
void ArcAppListPrefs::AppInfo::SetIgnoreCompareInstallTimeForTesting(
    bool ignore) {
  ignore_compare_app_info_install_time = ignore;
}

void ArcAppListPrefs::UpdateArcPackagesIsUpToDatePref() {
  // Set kArcPackagesIsUpToDate to true if there is no active install and all
  // installed packages are added to the prefs.
  prefs_->SetBoolean(
      arc::prefs::kArcPackagesIsUpToDate,
      installing_packages_count_ == 0 && packages_to_be_added_.empty());
}

ArcAppListPrefs::AppInfo::AppInfo(
    const std::string& name,
    const std::string& package_name,
    const std::string& activity,
    const std::string& intent_uri,
    const std::string& icon_resource_id,
    const std::optional<std::string>& version_name,
    const base::Time& last_launch_time,
    const base::Time& install_time,
    bool sticky,
    bool notifications_enabled,
    arc::mojom::ArcResizeLockState resize_lock_state,
    bool resize_lock_needs_confirmation,
    const WindowLayout& initial_window_layout,
    bool ready,
    bool suspended,
    bool show_in_launcher,
    bool shortcut,
    bool launchable,
    bool need_fixup,
    const std::optional<uint64_t> app_size_in_bytes,
    const std::optional<uint64_t> data_size_in_bytes,
    arc::mojom::AppCategory app_category)
    : name(name),
      package_name(package_name),
      activity(activity),
      intent_uri(intent_uri),
      icon_resource_id(icon_resource_id),
      version_name(version_name),
      last_launch_time(last_launch_time),
      install_time(install_time),
      sticky(sticky),
      notifications_enabled(notifications_enabled),
      resize_lock_state(resize_lock_state),
      resize_lock_needs_confirmation(resize_lock_needs_confirmation),
      initial_window_layout(initial_window_layout),
      ready(ready),
      suspended(suspended),
      show_in_launcher(show_in_launcher),
      shortcut(shortcut),
      launchable(launchable),
      need_fixup(need_fixup),
      app_size_in_bytes(app_size_in_bytes),
      data_size_in_bytes(data_size_in_bytes),
      app_category(app_category) {
  // If app is not launchable it also does not show in launcher.
  DCHECK(launchable || !show_in_launcher);
}

ArcAppListPrefs::AppInfo::AppInfo(AppInfo&& other) = default;

ArcAppListPrefs::AppInfo& ArcAppListPrefs::AppInfo::operator=(AppInfo&& other) =
    default;

// Need to add explicit destructor for chromium style checker error:
// Complex class/struct needs an explicit out-of-line destructor
ArcAppListPrefs::AppInfo::~AppInfo() = default;

bool ArcAppListPrefs::AppInfo::operator==(const AppInfo& other) const {
  return name == other.name && package_name == other.package_name &&
         activity == other.activity && intent_uri == other.intent_uri &&
         icon_resource_id == other.icon_resource_id &&
         version_name == other.version_name &&
         last_launch_time == other.last_launch_time &&
         (ignore_compare_app_info_install_time ||
          install_time == other.install_time) &&
         sticky == other.sticky &&
         notifications_enabled == other.notifications_enabled &&
         resize_lock_state == other.resize_lock_state &&
         resize_lock_needs_confirmation ==
             other.resize_lock_needs_confirmation &&
         initial_window_layout == other.initial_window_layout &&
         ready == other.ready && suspended == other.suspended &&
         show_in_launcher == other.show_in_launcher &&
         shortcut == other.shortcut && launchable == other.launchable &&
         need_fixup == other.need_fixup &&
         app_size_in_bytes == other.app_size_in_bytes &&
         data_size_in_bytes == other.data_size_in_bytes &&
         app_category == other.app_category;
}

ArcAppListPrefs::PackageInfo::PackageInfo(
    const std::string& package_name,
    int32_t package_version,
    int64_t last_backup_android_id,
    int64_t last_backup_time,
    bool should_sync,
    bool vpn_provider,
    bool preinstalled,
    bool game_controls_opt_out,
    base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
        permissions,
    arc::mojom::WebAppInfoPtr web_app_info,
    arc::mojom::PackageLocaleInfoPtr locale_info)
    : package_name(package_name),
      package_version(package_version),
      last_backup_android_id(last_backup_android_id),
      last_backup_time(last_backup_time),
      should_sync(should_sync),
      vpn_provider(vpn_provider),
      preinstalled(preinstalled),
      game_controls_opt_out(game_controls_opt_out),
      permissions(std::move(permissions)),
      web_app_info(std::move(web_app_info)),
      locale_info(std::move(locale_info)) {}

// Need to add explicit destructor for chromium style checker error:
// Complex class/struct needs an explicit out-of-line destructor
ArcAppListPrefs::PackageInfo::~PackageInfo() = default;

ArcAppListPrefs::WindowLayout::WindowLayout()
    : WindowLayout(arc::mojom::WindowSizeType::kUnknown, true, std::nullopt) {}

ArcAppListPrefs::WindowLayout::WindowLayout(arc::mojom::WindowSizeType type,
                                            bool resizable,
                                            std::optional<gfx::Rect> bounds)
    : type(type), resizable(resizable), bounds(std::move(bounds)) {}

ArcAppListPrefs::WindowLayout::WindowLayout(
    const ArcAppListPrefs::WindowLayout& other) = default;

ArcAppListPrefs::WindowLayout::~WindowLayout() = default;

bool ArcAppListPrefs::WindowLayout::operator==(
    const WindowLayout& other) const {
  return type == other.type && resizable == other.resizable &&
         bounds == other.bounds;
}
ArcAppListPrefs::Observer::~Observer() {
  CHECK(!IsInObserverList());
}
