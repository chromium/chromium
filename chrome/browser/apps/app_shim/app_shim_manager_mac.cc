// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "apps/app_lifetime_monitor_factory.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/barrier_closure.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/mac/code_signature.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"
#include "chrome/browser/apps/app_shim/code_signature_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/notifications/mac/notification_platform_bridge_mac.h"
#include "chrome/browser/notifications/mac/notification_utils.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/filename_util.h"

namespace {

// A feature to control whether or not the profile icons are sent over mojo.
// This is used to debug crashes that are only seen in release builds.
// https://crbug.com/1274236
BASE_FEATURE(kAppShimProfileMenuIcons,
             "AppShimProfileMenuIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A crash key that is used when dumping because of errors when building and
// verifying the app shim requirement.
crash_reporter::CrashKeyString<256> app_shim_requirement_crash_key(
    "AppShimRequirement");

// This function logs the status and error_details using OSSTATUS_LOG(). It also
// calls base::debug::DumpWithoutCrashing() using app_shim_requirement_crash_key
// as a crash key. The status and error_details are appended to the crash key.
void DumpOSStatusError(OSStatus status, std::string error_details) {
  OSSTATUS_LOG(ERROR, status) << error_details;
  crash_reporter::ScopedCrashKeyString crash_key_value(
      &app_shim_requirement_crash_key,
      base::StringPrintf("%s: %s (%d)", error_details.c_str(),
                         logging::DescriptionFromOSStatus(status).c_str(),
                         status));
  base::debug::DumpWithoutCrashing();
}

// This function is similar to DumpOSStatusError(), however it operates without
// an OSStatus.
void DumpError(std::string error_details) {
  LOG(ERROR) << error_details;
  crash_reporter::ScopedCrashKeyString crash_key_value(
      &app_shim_requirement_crash_key, error_details);
  base::debug::DumpWithoutCrashing();
}

// Creates a requirement for the app shim based on the framework bundle's
// designated requirement.
//
// Returns a non-null requirement or the reason why the requirement could not
// be created.
base::expected<base::apple::ScopedCFTypeRef<SecRequirementRef>,
               apps::MissingRequirementReason>
CreateAppShimRequirement() {
  ASSIGN_OR_RETURN(auto framework_requirement_string,
                   apps::FrameworkBundleDesignatedRequirementString());

  base::apple::ScopedCFTypeRef<CFStringRef> app_shim_requirement_string =
      apps::AppShimManager::
          BuildAppShimRequirementStringFromFrameworkRequirementString(
              framework_requirement_string.get());
  if (!app_shim_requirement_string) {
    return base::unexpected(apps::MissingRequirementReason::Error);
  }

  return apps::RequirementFromString(app_shim_requirement_string.get());
}

// Returns whether |app_shim_audit_token|'s code signature is trusted:
// - True if the framework bundle is unsigned (there's nothing to verify).
// - True if |app_shim_audit_token| satisfies the constructed designated
// requirement tailored for the app shim based on the framework bundle's
// requirement.
// - False otherwise (|app_shim_audit_token| does not satisfy the constructed
// designated requirement).
//
// This is used prior to macOS 11.7 where it is not possible to ad-hoc code sign
// the app shim at runtime.
bool IsAcceptablyCodeSignedLegacy(audit_token_t app_shim_audit_token) {
  static base::NoDestructor<
      base::expected<base::apple::ScopedCFTypeRef<SecRequirementRef>,
                     apps::MissingRequirementReason>>
      app_shim_requirement(CreateAppShimRequirement());
  if (!app_shim_requirement->has_value()) {
    switch (app_shim_requirement->error()) {
      case apps::MissingRequirementReason::NoOrAdHocSignature:
        // App shim validation is not required because framework bundle is not
        // code-signed or is ad-hoc code-signed.
        return true;
      case apps::MissingRequirementReason::Error:
        // Framework bundle is code-signed however we were unable to create the
        // app shim requirement. Deny.
        // apps::AppShimManager::BuildAppShimRequirementStringFromFrameworkRequirementString
        // already did the base::debug::DumpWithoutCrashing, possibly on a
        // previous call. We can return false here without any additional
        // explanation.
        return false;
    }
  }

  OSStatus status = base::mac::ProcessIsSignedAndFulfillsRequirement(
      app_shim_audit_token, app_shim_requirement->value().get());
  if (status != errSecSuccess) {
    if (status == errSecCSReqFailed &&
        AppShimRegistry::Get()->HasSavedAnyCdHashes()) {
      // errSecCSReqFailed is most likely a result of opening an ad-hoc signed
      // app shim after leaving the ad-hoc signing experiment group.
      // Log the error but skip `DumpWithoutCrashing`.
      OSSTATUS_LOG(ERROR, status) << "SecCodeCheckValidity";
    } else {
      DumpOSStatusError(status, "SecCodeCheckValidity");
    }
    return false;
  }
  return true;
}

// Returns whether |app_shim_code|'s code directory hash matches the value
// that was saved when the app was signed.
bool VerifyCodeDirectoryHash(
    base::apple::ScopedCFTypeRef<SecCodeRef> app_shim_code) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> app_shim_info;
  OSStatus status = SecCodeCopySigningInformation(
      app_shim_code.get(), kSecCSSigningInformation,
      app_shim_info.InitializeInto());
  if (status != errSecSuccess) {
    DumpOSStatusError(status, "SecCodeCopySigningInformation");
    return false;
  }

  CFDataRef cd_hash = base::apple::GetValueFromDictionary<CFDataRef>(
      app_shim_info.get(), kSecCodeInfoUnique);

  CFDictionaryRef info_plist =
      base::apple::GetValueFromDictionary<CFDictionaryRef>(app_shim_info.get(),
                                                           kSecCodeInfoPList);
  if (!info_plist) {
    return false;
  }

  CFStringRef app_id = base::apple::GetValueFromDictionary<CFStringRef>(
      info_plist, CFSTR("CrAppModeShortcutID"));
  if (!app_id) {
    return false;
  }

  return AppShimRegistry::Get()->VerifyCdHashForApp(
      base::SysCFStringRefToUTF8(app_id), base::apple::CFDataToSpan(cd_hash));
}

// Returns whether |app_shim_audit_token|'s code signature is trusted. Since an
// ad-hoc code signature is used on macOS 11.7 and above, the verification
// consists of:
//  - verifying the signature is valid.
//  - verifying the code directory hash in the signature matches the value
//    stored for this app at signing time.
bool IsAcceptablyAdHocCodeSigned(audit_token_t app_shim_audit_token) {
  base::apple::ScopedCFTypeRef<CFDataRef> audit_token_cf(CFDataCreate(
      nullptr, reinterpret_cast<const UInt8*>(&app_shim_audit_token),
      sizeof(audit_token_t)));
  const void* app_shim_attribute_keys[] = {kSecGuestAttributeAudit};
  const void* app_shim_attribute_values[] = {audit_token_cf.get()};
  base::apple::ScopedCFTypeRef<CFDictionaryRef> app_shim_attributes(
      CFDictionaryCreate(
          nullptr, app_shim_attribute_keys, app_shim_attribute_values,
          std::size(app_shim_attribute_keys), &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks));
  base::apple::ScopedCFTypeRef<SecCodeRef> app_shim_code;
  OSStatus status = SecCodeCopyGuestWithAttributes(
      nullptr, app_shim_attributes.get(), kSecCSDefaultFlags,
      app_shim_code.InitializeInto());
  if (status != errSecSuccess) {
    DumpOSStatusError(status, "SecCodeCopyGuestWithAttributes");
    return false;
  }
  status =
      SecCodeCheckValidity(app_shim_code.get(), kSecCSDefaultFlags, nullptr);
  if (status != errSecSuccess) {
    DumpOSStatusError(status, "SecCodeCheckValidity");
    return false;
  }

  return VerifyCodeDirectoryHash(app_shim_code);
}

bool ProfileMenuItemComparator(const chrome::mojom::ProfileMenuItemPtr& a,
                               const chrome::mojom::ProfileMenuItemPtr& b) {
  return a->menu_index < b->menu_index;
}

// Used by tests to be informed when launching an app shim has finished.
base::OnceClosure& GetShimStartupDoneCallback() {
  static base::NoDestructor<base::OnceClosure> instance;
  return *instance;
}

base::OnceClosure TakeShimStartupDoneCallbackOrDoNothing() {
  if (GetShimStartupDoneCallback()) {
    return std::move(GetShimStartupDoneCallback());
  }
  return base::DoNothing();
}

}  // namespace

namespace apps {

bool AppShimManager::AppShimObserver::OnNotificationAction(
    mac_notifications::mojom::NotificationActionInfoPtr& info) {
  return true;
}

void SetMacShimStartupDoneCallbackForTesting(base::OnceClosure callback) {
  DCHECK(!GetShimStartupDoneCallback());
  GetShimStartupDoneCallback() = std::move(callback);
}

base::OnceClosure TakeShimStartupDoneCallbackForTesting() {
  return std::move(GetShimStartupDoneCallback());
}

// The state for an individual (app, Profile) pair. This includes the
// AppShimHost.
struct AppShimManager::ProfileState {
  ProfileState(AppShimManager::AppState* in_app_state,
               std::unique_ptr<AppShimHost> in_single_profile_host);
  ProfileState(const ProfileState&) = delete;
  ProfileState& operator=(const ProfileState&) = delete;
  ~ProfileState() = default;

  AppShimHost* GetHost() const;

  // Weak, owns |this|.
  const raw_ptr<AppShimManager::AppState> app_state;

  // The AppShimHost for apps that are not multi-profile.
  const std::unique_ptr<AppShimHost> single_profile_host;

  // All browser instances for this (app, Profile) pair.
  std::set<Browser*> browsers;

  // The current BadgeValue for this (app, Profile) pair.
  std::optional<badging::BadgeManager::BadgeValue> badge;
};

// The state for an individual app. This includes the state for all
// profiles that are using the app.
struct AppShimManager::AppState {
  AppState(const webapps::AppId& app_id,
           std::unique_ptr<AppShimHost> multi_profile_host)
      : app_id(app_id), multi_profile_host(std::move(multi_profile_host)) {}
  AppState(const AppState&) = delete;
  AppState& operator=(const AppState&) = delete;
  ~AppState() = default;

  bool IsMultiProfile() const;

  // Return true if the app state should be deleted (e.g, because all profiles
  // have closed).
  bool ShouldDeleteAppState() const;

  // Mark the last-active profiles in AppShimRegistry, so that they will re-open
  // when the app is started next. Does nothing if this isn't a multi-profile
  // app, or if `did_save_last_active_profiles_on_terminate` is true.
  void MaybeSaveLastActiveProfiles() const;

  const std::string app_id;

  // Multi-profile apps share the same shim process across multiple profiles.
  const std::unique_ptr<AppShimHost> multi_profile_host;

  // The profile state for the profiles currently running this app.
  std::map<Profile*, std::unique_ptr<ProfileState>> profiles;

  // When an app is terminated, we only want to save the last active profiles
  // once. This field is set to true when a clean shutdown has already saved
  // last active profiles, to prevent the code that exists to handle unclean
  // shutdowns from overwriting the last active profiles. In case of a clean
  // shutdown some browser windows/profiles might have already closed by the
  // time OnShimProcessDisconnected runs.
  bool did_save_last_active_profiles_on_terminate = false;

  // Sometimes, for example when we have a pending notification permission
  // prompt, we want to keep alive an app shim process even though no windows
  // are open. This counter keep tracks of the number of outstanding
  // ScopedAppShimKeepAlive instances.
  int keep_alive_count = 0;
};

AppShimManager::ProfileState::ProfileState(
    AppShimManager::AppState* in_app_state,
    std::unique_ptr<AppShimHost> in_single_profile_host)
    : app_state(in_app_state),
      single_profile_host(std::move(in_single_profile_host)) {
  // Assert that the ProfileState and AppState agree about whether or not this
  // is a multi-profile shim.
  DCHECK_NE(!!single_profile_host, !!app_state->multi_profile_host);
}

AppShimHost* AppShimManager::ProfileState::GetHost() const {
  if (app_state->multi_profile_host)
    return app_state->multi_profile_host.get();
  return single_profile_host.get();
}

bool AppShimManager::AppState::IsMultiProfile() const {
  return multi_profile_host.get();
}

bool AppShimManager::AppState::ShouldDeleteAppState() const {
  // The new behavior for multi-profile apps is to not close the app based on
  // which windows are open. Rather, the app must be explicitly closed via
  // the Quit menu, which will terminate the app (and the browser will be
  // notified of the closed mojo pipe). The app is closed automatically when
  // it has been uninstalled for all profiles.
  // https://crbug.com/1080729 for new behavior.
  // https://crbug.com/1139254,1132223 for closing when profiles close.
  if (IsMultiProfile() &&
      base::FeatureList::IsEnabled(features::kAppShimNewCloseBehavior)) {
    // This might get called late enough during shutdown for ProfileManager to
    // no longer exist. GetInstalledProfilesForApp requires ProfileManager to
    // still exist, so if we're shutting down, just return true.
    if (g_browser_process->IsShuttingDown()) {
      return true;
    }
    return profiles.empty() &&
           AppShimRegistry::Get()->GetInstalledProfilesForApp(app_id).empty();
  }

  // The old behavior, and the behavior for single-profile apps, is to close
  // only when all profiles are closed.
  return profiles.empty() && keep_alive_count == 0;
}

void AppShimManager::AppState::MaybeSaveLastActiveProfiles() const {
  if (!IsMultiProfile() || did_save_last_active_profiles_on_terminate) {
    return;
  }
  std::set<base::FilePath> last_active_profile_paths;
  for (auto iter_profile = profiles.begin(); iter_profile != profiles.end();
       ++iter_profile) {
    last_active_profile_paths.insert(iter_profile->first->GetPath());
  }
  AppShimRegistry::Get()->SaveLastActiveProfilesForApp(
      app_id, last_active_profile_paths);
}

class ScopedAppShimKeepAlive {
 public:
  ScopedAppShimKeepAlive(AppShimManager* manager, const webapps::AppId& app_id);
  ~ScopedAppShimKeepAlive();

  ScopedAppShimKeepAlive(const ScopedAppShimKeepAlive&) = delete;
  ScopedAppShimKeepAlive& operator=(const ScopedAppShimKeepAlive&) = delete;

 private:
  base::WeakPtr<AppShimManager> manager_;
  const webapps::AppId app_id_;
};

ScopedAppShimKeepAlive::ScopedAppShimKeepAlive(AppShimManager* manager,
                                               const webapps::AppId& app_id)
    : manager_(manager->weak_factory_.GetWeakPtr()), app_id_(app_id) {
  auto app = manager_->apps_.find(app_id_);
  CHECK(app != manager_->apps_.end());
  app->second->keep_alive_count++;
}

ScopedAppShimKeepAlive::~ScopedAppShimKeepAlive() {
  if (manager_) {
    auto app = manager_->apps_.find(app_id_);
    if (app != manager_->apps_.end()) {
      CHECK_GT(app->second->keep_alive_count, 0);
      app->second->keep_alive_count--;
    }
  }
}

AppShimManager::AppShimManager(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      profile_manager_(g_browser_process->profile_manager()),
      weak_factory_(this) {
  AppShimHostBootstrap::SetClient(this);
  if (profile_manager_)
    profile_manager_->AddObserver(this);
  BrowserList::AddObserver(this);
}

AppShimManager::~AppShimManager() {
  BrowserList::RemoveObserver(this);
  AppShimHostBootstrap::SetClient(nullptr);
}

void AppShimManager::OnProfileManagerDestroying() {
  avatar_menu_.reset();
  if (profile_manager_)
    profile_manager_->RemoveObserver(this);
  profile_manager_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
}

AppShimHost* AppShimManager::FindHost(Profile* profile,
                                      const webapps::AppId& app_id) {
  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end())
    return nullptr;
  AppState* app_state = found_app->second.get();
  auto found_profile = app_state->profiles.find(profile);
  if (found_profile == app_state->profiles.end())
    return nullptr;
  ProfileState* profile_state = found_profile->second.get();
  return profile_state->GetHost();
}

bool AppShimManager::HasNonBookmarkAppWindowsOpen() {
  return delegate_->HasNonBookmarkAppWindowsOpen();
}

void AppShimManager::UpdateAppBadge(
    Profile* profile,
    const webapps::AppId& app_id,
    const std::optional<badging::BadgeManager::BadgeValue>& badge) {
  // TODO(crbug.com/40761338): Support updating the app badge for apps
  // that aren't currently running.
  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end()) {
    return;
  }
  AppState* app_state = found_app->second.get();
  DCHECK(app_state);
  auto found_profile = app_state->profiles.find(profile);
  if (found_profile == app_state->profiles.end()) {
    return;
  }
  ProfileState* profile_state = found_profile->second.get();
  DCHECK(profile_state);

  profile_state->badge = badge;
  UpdateApplicationBadge(profile_state);
}

mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
AppShimManager::LaunchNotificationProvider(const webapps::AppId& app_id) {
  CHECK(
      base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution));

  mojo::Remote<mac_notifications::mojom::MacNotificationProvider> remote;
  auto bind_provider = base::BindOnce(
      [](mojo::PendingReceiver<
             mac_notifications::mojom::MacNotificationProvider> receiver,
         base::WeakPtr<AppShimManager> manager, AppShimHost* host) {
        if (!host) {
          LOG(ERROR) << "Failed to launch app shim for notifications";
          if (manager) {
            manager->dummy_notification_provider_receivers_.Add(
                manager.get(), std::move(receiver));
          }
          return;
        }
        host->GetAppShim()->BindNotificationProvider(std::move(receiver));
      },
      remote.BindNewPipeAndPassReceiver(), weak_factory_.GetWeakPtr());

  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end()) {
    // To check or display a notification associated with a specific app, calls
    // to the notifications API need to happen from within that app. If we don't
    // already have a running app shim, launch a new one, but launch it in
    // "background" mode, so to the user it isn't noticeable that this is
    // happening.
    LaunchShimInBackgroundMode(app_id, std::move(bind_provider));
    return remote;
  }

  AppState* app_state = found_app->second.get();
  CHECK(app_state->IsMultiProfile());
  AppShimHost* shim = app_state->multi_profile_host.get();
  std::move(bind_provider).Run(shim);
  return remote;
}

void AppShimManager::ShowNotificationPermissionRequest(
    const webapps::AppId& app_id,
    RequestNotificationPermissionCallback callback) {
  CHECK(
      base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution));

  if (notification_permission_result_for_testing_.has_value()) {
    std::move(callback).Run(*notification_permission_result_for_testing_);
    return;
  }

  auto request_permission = base::BindOnce(
      [](base::WeakPtr<AppShimManager> manager, const webapps::AppId& app_id,
         RequestNotificationPermissionCallback callback, AppShimHost* host) {
        if (!host) {
          LOG(ERROR)
              << "Failed to launch app shim for notifications permissions";
          std::move(callback).Run(mac_notifications::mojom::
                                      RequestPermissionResult::kRequestFailed);
          return;
        }
        std::unique_ptr<ScopedAppShimKeepAlive> keep_alive;
        if (manager) {
          keep_alive =
              std::make_unique<ScopedAppShimKeepAlive>(manager.get(), app_id);
        }
        // Wrap callback with default invoke to correctly report a failure if
        // the app shim fails to launch.
        host->GetAppShim()->RequestNotificationPermission(
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                std::move(callback).Then(base::OnceClosure(
                    base::DoNothingWithBoundArgs(std::move(keep_alive)))),
                mac_notifications::mojom::RequestPermissionResult::
                    kRequestFailed));
      },
      weak_factory_.GetWeakPtr(), app_id, std::move(callback));

  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end()) {
    LaunchShimInBackgroundMode(app_id, std::move(request_permission));
    return;
  }

  AppState* app_state = found_app->second.get();
  CHECK(app_state->IsMultiProfile());
  std::move(request_permission).Run(app_state->multi_profile_host.get());
}

Profile* AppShimManager::ProfileForBackgroundShimLaunch(
    const webapps::AppId& app_id) {
  if (profile_manager_) {
    for (Profile* p : profile_manager_->GetLoadedProfiles()) {
      if (!p->IsRegularProfile()) {
        continue;
      }
      if (delegate_->AppIsInstalled(p, app_id)) {
        return p;
      }
    }
  }
  return nullptr;
}

void AppShimManager::LaunchShimInBackgroundMode(
    const webapps::AppId& app_id,
    base::OnceCallback<void(AppShimHost*)> callback) {
  // A shim can only be launched through an active profile, so find a profile
  // through which to do the launch. This method should only be called for
  // multi-profile apps, for which an arbitrary profile is good enough.
  Profile* profile = ProfileForBackgroundShimLaunch(app_id);

  if (!profile) {
    LOG(ERROR) << "Failed to find loaded profile with " << app_id
               << " installed";
    std::move(callback).Run(nullptr);
    return;
  }

  CHECK(delegate_->AppIsMultiProfile(profile, app_id));
  auto* profile_state = GetOrCreateProfileState(profile, app_id);
  std::move(callback).Run(profile_state->GetHost());
  profile_state->GetHost()->LaunchShim(web_app::ShimLaunchMode::kBackground);
}

void AppShimManager::BindNotificationService(
    mojo::PendingReceiver<mac_notifications::mojom::MacNotificationService>
        service,
    mojo::PendingRemote<mac_notifications::mojom::MacNotificationActionHandler>
        handler) {
  // Dummy MacNotificationProvider implementation. The notifications code that
  // ends up calling LaunchNotificationProvider expects to always get a
  // bound/connected MacNotificationProvider remote, so if we don't have an
  // app shim process to connect to, instead a remote bound to this is returned.
}

void AppShimManager::OnNotificationAction(
    mac_notifications::mojom::NotificationActionInfoPtr info) {
  if (!app_shim_observer_ || app_shim_observer_->OnNotificationAction(info)) {
    ProcessMacNotificationResponse(
        mac_notifications::NotificationStyle::kAppShim, std::move(info),
        notification_action_handler_receivers_.current_context());
  }

  auto it = bootstraps_pending_notification_actions_.find(
      notification_action_handler_receivers_.current_receiver());
  if (it != bootstraps_pending_notification_actions_.end()) {
    // ProcessMacNotificationResponse posts a task to the UI thread to handle
    // the response. OnShimProcessConnectedForRegisterOnly needs to run after
    // that task, so post a task here as well.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppShimManager::OnShimProcessConnectedForRegisterOnly,
                       base::Unretained(this), std::move(it->second)));
    bootstraps_pending_notification_actions_.erase(it);
  }
}

void AppShimManager::UpdateApplicationBadge(ProfileState* profile_state) {
  if (profile_state->single_profile_host &&
      profile_state->single_profile_host->GetAppShim()) {
    profile_state->single_profile_host->GetAppShim()->SetBadgeLabel(
        profile_state->badge
            ? badging::GetBadgeString(profile_state->badge.value())
            : "");
  } else if (profile_state->app_state->multi_profile_host &&
             profile_state->app_state->multi_profile_host->GetAppShim()) {
    std::optional<badging::BadgeManager::BadgeValue> combined_badge;
    for (const auto& [profile, state] : profile_state->app_state->profiles) {
      if (state->badge) {
        if (!combined_badge) {
          combined_badge.emplace();
        }
        if (state->badge->has_value()) {
          // Number badge, add to combined badge.
          if (!combined_badge->has_value()) {
            combined_badge->emplace(0);
          }
          combined_badge->value() += state->badge->value();
        }
      }
    }
    profile_state->app_state->multi_profile_host->GetAppShim()->SetBadgeLabel(
        combined_badge ? badging::GetBadgeString(combined_badge.value()) : "");
  }
}

AppShimHost* AppShimManager::GetHostForRemoteCocoaBrowser(Browser* browser) {
  const std::string app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  if (!delegate_->AppUsesRemoteCocoa(browser->profile(), app_id))
    return nullptr;
  auto* profile_state = GetOrCreateProfileState(browser->profile(), app_id);
  if (!profile_state)
    return nullptr;
  return profile_state->GetHost();
}

bool AppShimManager::BrowserUsesRemoteCocoa(Browser* browser) {
  const std::string app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  if (web_app::AppShimCreationAndLaunchDisabledForTest()) {
    return false;
  }
  return delegate_->AppUsesRemoteCocoa(browser->profile(), app_id);
}

void AppShimManager::OnShimLaunchRequested(
    AppShimHost* host,
    web_app::LaunchShimUpdateBehavior update_behavior,
    web_app::ShimLaunchMode launch_mode,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  // A shim can only be launched through an active profile, so find a profile
  // through which to do the launch. For multi-profile apps, select one
  // arbitrarily. For non-multi-profile apps, select the specified profile.
  Profile* profile = nullptr;
  {
    auto found_app = apps_.find(host->GetAppId());
    CHECK(found_app != apps_.end(), base::NotFatalUntil::M130);
    AppState* app_state = found_app->second.get();
    if (app_state->IsMultiProfile()) {
      // It is possible for `profiles` to be empty if the profile was closed
      // while an initial launch attempt took place (and then failed, triggering
      // a second launch attempt). In that case, simply fail the second launch
      // as well.
      if (app_state->profiles.empty()) {
        LOG(ERROR)
            << "Attempting to launch shim for which no profiles are loaded.";
        std::move(terminated_callback).Run();
        return;
      }
      DCHECK(!app_state->profiles.empty());
      profile = app_state->profiles.begin()->first;
    } else {
      profile = ProfileForPath(host->GetProfilePath());
    }
  }

  // If `update_behavior` was set to possible recreate shims, it can happen that
  // the app got uninstalled while an initial launch attempt took place (and
  // failed). So check first if the app is still installed.
  // TODO(mek): Rather than this workaround, we should make sure to destroy
  // AppShimHost and terminate app shims when an app is uninstalled.
  if (web_app::RecreateShimsRequested(update_behavior) &&
      (!delegate_->AppIsInstalled(profile, host->GetAppId()) ||
       !AppShimRegistry::Get()->IsAppInstalledInProfile(host->GetAppId(),
                                                        profile->GetPath()))) {
    LOG(ERROR)
        << "Attempting to launch shim for an app that is no longer installed.";
    std::move(terminated_callback).Run();
    return;
  }

  delegate_->LaunchShim(profile, host->GetAppId(), update_behavior, launch_mode,
                        std::move(launched_callback),
                        std::move(terminated_callback));
}

void AppShimManager::OnShimProcessConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  DCHECK(crx_file::id_util::IdIsValid(bootstrap->GetAppId()));
  if (app_shim_observer_) {
    app_shim_observer_->OnShimProcessConnected(bootstrap->GetAppShimPid());
  }

  auto notification_action_handler = bootstrap->TakeNotificationActionHandler();
  std::optional<mojo::ReceiverId> notification_action_receiver_id;
  if (base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution) &&
      notification_action_handler) {
    notification_action_receiver_id =
        notification_action_handler_receivers_.Add(
            this, std::move(notification_action_handler),
            bootstrap->GetAppId());
  }

  switch (bootstrap->GetLaunchType()) {
    case chrome::mojom::AppShimLaunchType::kNormal: {
      const base::FilePath profile_path = bootstrap->GetProfilePath();
      LoadAndLaunchAppParams params;
      params.app_id = bootstrap->GetAppId();
      params.files = bootstrap->GetLaunchFiles();
      params.urls = bootstrap->GetLaunchUrls();
      params.login_item_restore_state = bootstrap->GetLoginItemRestoreState();
      LoadAndLaunchAppCallback launch_callback = base::BindOnce(
          &AppShimManager::OnShimProcessConnectedAndAllLaunchesDone,
          weak_factory_.GetWeakPtr(), std::move(bootstrap));
      LoadAndLaunchApp(profile_path, params, std::move(launch_callback));
      break;
    }
    case chrome::mojom::AppShimLaunchType::kNotificationAction:
      if (base::FeatureList::IsEnabled(
              features::kAppShimNotificationAttribution) &&
          notification_action_receiver_id.has_value()) {
        // Wait for the notification action to be handled before finishing up
        // the connection process to ensure Chrome and the App Shim stay alive
        // long enough.
        bootstraps_pending_notification_actions_.emplace(
            *notification_action_receiver_id, std::move(bootstrap));
        break;
      }
      [[fallthrough]];
    case chrome::mojom::AppShimLaunchType::kRegisterOnly:
      OnShimProcessConnectedForRegisterOnly(std::move(bootstrap));
      break;
  }
}

void AppShimManager::OnShimProcessConnectedForRegisterOnly(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  const webapps::AppId& app_id = bootstrap->GetAppId();
  DCHECK(bootstrap->GetLaunchType() ==
             chrome::mojom::AppShimLaunchType::kRegisterOnly ||
         bootstrap->GetLaunchType() ==
             chrome::mojom::AppShimLaunchType::kNotificationAction)
      << bootstrap->GetLaunchType();

  // Create a ProfileState the specified profile (if there is one). We should
  // not do this (if there exists no ProfileState, then the shim should just
  // exit), but many tests assume this behavior, and need to be updated.
  Profile* profile = ProfileForPath(bootstrap->GetProfilePath());
  bool app_installed = delegate_->AppIsInstalled(profile, app_id);
  if (profile && app_installed && delegate_->AppCanCreateHost(profile, app_id))
    GetOrCreateProfileState(profile, app_id);

  // Because this was a register-only launch, it must have been launched by
  // Chrome, and so there should probably still exist the ProfileState through
  // which the launch was originally done.
  ProfileState* profile_state = nullptr;
  auto found_app = apps_.find(app_id);
  if (found_app != apps_.end()) {
    AppState* app_state = found_app->second.get();
    if (app_state->IsMultiProfile()) {
      // While generally `profiles` should never be empty, sometimes we keep
      // alive app shims even when no profiles have windows open for the app
      // (for example when we have a pending notification permission request).
      if (!app_state->profiles.empty()) {
        profile_state = app_state->profiles.begin()->second.get();
      }
    } else {
      auto found_profile = app_state->profiles.find(profile);
      if (found_profile != app_state->profiles.end()) {
        profile_state = found_profile->second.get();
      }
    }
  }

  OnShimProcessConnectedAndAllLaunchesDone(
      std::move(bootstrap), profile_state,
      profile_state
          ? chrome::mojom::AppShimLaunchResult::kSuccess
          : chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect);
}

void AppShimManager::LoadAndLaunchAppForTesting(const webapps::AppId& app_id) {
  LoadAndLaunchAppParams params;
  params.app_id = app_id;
  LoadAndLaunchApp(/*profile_path=*/base::FilePath(), params,
                   base::DoNothing());
}

void AppShimManager::LoadAndLaunchApp(
    const base::FilePath& profile_path,
    const LoadAndLaunchAppParams& params,
    LoadAndLaunchAppCallback launch_callback) {
  // Before anything else, if this launch includes files or urls we need to
  // determine which profiles are capable of handling those files or urls.
  std::map<base::FilePath, int> profiles_with_handlers =
      GetProfilesWithMatchingHandlers(params);

  // Check to see if the app is already running for a profile compatible with
  // |profile_path|. If so, early-out.
  if (LoadAndLaunchApp_TryExistingProfileStates(
          profile_path, params, profiles_with_handlers, &launch_callback)) {
    // If we used an existing profile, |launch_callback| should have been run.
    DCHECK(!launch_callback);
    DCHECK(!GetShimStartupDoneCallback());
    return;
  }

  // Retrieve the list of last-active profiles. If there are no last-active
  // profiles (which is rare -- e.g, when the last-active profiles were
  // removed), then use all profiles for which the app is installed.
  std::set<base::FilePath> last_active_profile_paths =
      AppShimRegistry::Get()->GetLastActiveProfilesForApp(params.app_id);
  if (last_active_profile_paths.empty()) {
    last_active_profile_paths =
        AppShimRegistry::Get()->GetInstalledProfilesForApp(params.app_id);
  }

  // If a non-empty `profile_path` was specified, use that as first preferred
  // profile. If this is a file or protocol handler launch subsequently use the
  // best match from `profiles_with_handlers`. Otherwise append all profiles
  // from `last_active_profile_paths` to the list of profiles to launch.
  std::vector<base::FilePath> profile_paths_to_launch;
  if (!profile_path.empty())
    profile_paths_to_launch.push_back(profile_path);
  if (!profiles_with_handlers.empty()) {
    int best_score = 0;
    base::FilePath best_path;
    for (const auto& [profile, score] : profiles_with_handlers) {
      if (score > best_score) {
        best_score = score;
        best_path = profile;
      }
    }
    DCHECK(!best_path.empty());
    profile_paths_to_launch.push_back(best_path);
  } else {
    profile_paths_to_launch.insert(profile_paths_to_launch.end(),
                                   last_active_profile_paths.begin(),
                                   last_active_profile_paths.end());
  }

  // Attempt load all of the profiles in |profile_paths_to_launch|, and once
  // they're loaded (or have failed to load), call
  // OnShimProcessConnectedAndProfilesToLaunchLoaded.
  base::OnceClosure callback =
      base::BindOnce(&AppShimManager::LoadAndLaunchApp_OnProfilesAndAppReady,
                     weak_factory_.GetWeakPtr(), profile_paths_to_launch,
                     /*first_profile_is_from_bootstrap=*/!profile_path.empty(),
                     params, std::move(launch_callback));
  {
    // This will update |callback| to be a chain of callbacks that load the
    // profiles in |profile_paths_to_load|, one by one, using
    // LoadProfileAndApp, and then finally call the initial |callback|. This
    // may end up being async (if some profiles aren't loaded), or may be
    // synchronous (if all profiles happen to already be loaded).
    for (const auto& profile_path_to_launch : profile_paths_to_launch) {
      if (profile_path_to_launch.empty())
        continue;
      LoadProfileAndAppCallback callback_wrapped =
          base::BindOnce([](base::OnceClosure callback_to_wrap,
                            Profile*) { std::move(callback_to_wrap).Run(); },
                         std::move(callback));
      callback = base::BindOnce(
          &AppShimManager::LoadProfileAndApp, weak_factory_.GetWeakPtr(),
          profile_path_to_launch, params.app_id, std::move(callback_wrapped));
    }
  }
  std::move(callback).Run();
}

bool AppShimManager::LoadAndLaunchApp_TryExistingProfileStates(
    const base::FilePath& profile_path,
    const LoadAndLaunchAppParams& params,
    const std::map<base::FilePath, int>& profiles_with_handlers,
    LoadAndLaunchAppCallback* launch_callback) {
  auto found_app = apps_.find(params.app_id);
  if (found_app == apps_.end())
    return false;
  AppState* app_state = found_app->second.get();

  // Search for an existing ProfileState for this app.
  Profile* profile = nullptr;
  ProfileState* profile_state = nullptr;
  if (!profile_path.empty()) {
    // If |profile_path| is populated, then only retrieve that specified
    // profile's ProfileState.
    profile = ProfileForPath(profile_path);
    auto found_profile = app_state->profiles.find(profile);
    if (found_profile == app_state->profiles.end())
      return false;
    profile_state = found_profile->second.get();
  } else {
    // If no profile was specified, select the best option from the open
    // profiles in `profiles_with_handlers`. If there are profiles with handlers
    // yet none of them are currently open don't use an existing profile.
    if (!profiles_with_handlers.empty()) {
      int best_score = 0;
      for (const auto& [it_profile, it_profile_state] : app_state->profiles) {
        auto it = profiles_with_handlers.find(it_profile->GetPath());
        if (it != profiles_with_handlers.end()) {
          int score = it->second;
          if (score > best_score) {
            best_score = score;
            profile = it_profile;
            profile_state = it_profile_state.get();
          }
        }
      }
    } else {
      // If `profiles_with_handlers` is empty, either because `params` does not
      // contains files or urls, or because there are no profiles that can
      // handle the files or urls, select the first open profile encountered.
      // TODO(crbug.com/40570436): This should select the
      // most-recently-used profile, not the first profile encountered.
      auto it = app_state->profiles.begin();
      if (it != app_state->profiles.end()) {
        profile = it->first;
        profile_state = it->second.get();
      }
    }
  }
  if (!profile_state)
    return false;
  DCHECK(profile);

  // Launch the app, if appropriate.
  LoadAndLaunchApp_LaunchIfAppropriate(
      profile, profile_state, params, TakeShimStartupDoneCallbackOrDoNothing());

  std::move(*launch_callback)
      .Run(profile_state, chrome::mojom::AppShimLaunchResult::kSuccess);
  return true;
}

void AppShimManager::LoadAndLaunchApp_OnProfilesAndAppReady(
    const std::vector<base::FilePath>& profile_paths_to_launch,
    bool first_profile_is_from_bootstrap,
    const LoadAndLaunchAppParams& params,
    LoadAndLaunchAppCallback launch_callback) {
  // Launch all of the profiles in |profile_paths_to_launch|. Record the most
  // profile successfully launched in |launched_profile_state|, and the most
  // recent reason for a failure (if any) in |launch_result|.
  ProfileState* launched_profile_state = nullptr;
  auto launch_result = chrome::mojom::AppShimLaunchResult::kProfileNotFound;
  auto barrier = base::BarrierClosure(profile_paths_to_launch.size(),
                                      TakeShimStartupDoneCallbackOrDoNothing());

  for (size_t iter = 0; iter < profile_paths_to_launch.size(); ++iter) {
    base::ScopedClosureRunner launch_finished(barrier);
    const base::FilePath& profile_path = profile_paths_to_launch[iter];
    if (profile_path.empty()) {
      continue;
    }
    if (IsProfileLockedForPath(profile_path)) {
      launch_result = chrome::mojom::AppShimLaunchResult::kProfileLocked;
      continue;
    }
    Profile* profile = ProfileForPath(profile_path);
    if (!profile) {
      launch_result = chrome::mojom::AppShimLaunchResult::kProfileNotFound;
      continue;
    }
    if (!delegate_->AppIsInstalled(profile, params.app_id)) {
      launch_result = chrome::mojom::AppShimLaunchResult::kAppNotFound;
      continue;
    }

    // Create a ProfileState for this app, if appropriate (e.g, not for
    // open-in-a-tab bookmark apps).
    ProfileState* profile_state = nullptr;
    if (delegate_->AppCanCreateHost(profile, params.app_id)) {
      profile_state = GetOrCreateProfileState(profile, params.app_id);
    }

    // Launch the app, if appropriate.
    LoadAndLaunchApp_LaunchIfAppropriate(profile, profile_state, params,
                                         launch_finished.Release());

    // If we successfully created a profile state, save it for |bootstrap| to
    // connect to once all launches are done.
    if (profile_state) {
      launched_profile_state = profile_state;
    } else {
      launch_result = chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect;
    }

    // If files or urls were specified, only open one new window.
    // If this was the profile specified in the bootstrap, also stop here.
    if (params.HasFilesOrURLs() ||
        (first_profile_is_from_bootstrap && iter == 0)) {
      // Trigger barrier for remaining profiles we didn't launch in.
      for (size_t i = iter + 1; i < profile_paths_to_launch.size(); ++i) {
        barrier.Run();
      }

      break;
    }
  }

  // If we launched any profile, report success.
  if (launched_profile_state)
    launch_result = chrome::mojom::AppShimLaunchResult::kSuccess;

  std::move(launch_callback).Run(launched_profile_state, launch_result);
}

void AppShimManager::OnShimProcessConnectedAndAllLaunchesDone(
    std::unique_ptr<AppShimHostBootstrap> bootstrap,
    ProfileState* profile_state,
    chrome::mojom::AppShimLaunchResult result) {
  if (app_shim_observer_) {
    app_shim_observer_->OnShimProcessConnectedAndAllLaunchesDone(
        bootstrap->GetAppShimPid(), result);
  }

  // If the browser process was launched by the App Shim in hidden mode, the
  // browser process should not stay alive indefinitely after all Browser
  // instances have been closed. Calling ResetKeepAliveWhileHidden() lets
  // the browser process terminate itself when no more Browsers or other
  // ScopedKeepAlives exist.
  //
  // At this point, if chrome was launched by an App Shim we would have finished
  // creating any browser windows or other ScopedKeepAlive instances that
  // resulted from the app shim launch, so now is a good time to stop the
  // browser process from keeping itself alive indefinitely.
  app_controller_mac::ResetKeepAliveWhileHidden();

  // If we failed because the profile was locked, launch the profile manager.
  if (result == chrome::mojom::AppShimLaunchResult::kProfileLocked) {
    LaunchProfilePicker();
  } else {
    // If the app specified a URL, but we tried and failed to launch it, then
    // open that URL in a new browser window.
    if (result != chrome::mojom::AppShimLaunchResult::kSuccess &&
        result != chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect &&
        bootstrap->GetLaunchType() ==
            chrome::mojom::AppShimLaunchType::kNormal) {
      const GURL& url = bootstrap->GetAppURL();
      if (url.is_valid()) {
        OpenAppURLInBrowserWindow(bootstrap->GetProfilePath(), url);
      }
    }
  }

  // If we failed to find a AppShimHost (in a ProfileState) for |bootstrap|
  // to connect to, then quit the shim. This may not represent an actual
  // failure (e.g, open-in-a-tab bookmarks return kSuccessAndDisconnect).
  if (result != chrome::mojom::AppShimLaunchResult::kSuccess) {
    DCHECK(!profile_state);
    bootstrap->OnFailedToConnectToHost(result);
    return;
  }
  DCHECK(profile_state);
  AppShimHost* host = profile_state->GetHost();
  DCHECK(host);

  // If we already have a host attached (e.g, due to multiple launches racing),
  // close down the app shim that didn't win the race.
  if (host->HasBootstrapConnected()) {
    bootstrap->OnFailedToConnectToHost(
        chrome::mojom::AppShimLaunchResult::kDuplicateHost);
    return;
  }

  // If the connecting shim process doesn't have an acceptable code
  // signature, reject the connection and re-launch the shim. The internal
  // re-launch will likely fail, whereupon the shim will be recreated.
  if (!IsAcceptablyCodeSigned(bootstrap->GetAppShimAuditToken())) {
    LOG(ERROR) << "The attaching app shim's code signature is invalid.";
    bootstrap->OnFailedToConnectToHost(
        chrome::mojom::AppShimLaunchResult::kFailedValidation);
    host->LaunchShim();
    return;
  }

  host->OnBootstrapConnected(std::move(bootstrap));
}

void AppShimManager::LoadAndLaunchApp_LaunchIfAppropriate(
    Profile* profile,
    ProfileState* profile_state,
    const LoadAndLaunchAppParams& params,
    base::OnceClosure launch_finished_callback) {
  // If `params.files`, `params.urls` or `params.override_url` is non-empty,
  // then always do a launch to open the files or URL(s).
  bool do_launch = params.HasFilesOrURLs();

  // Otherwise, only launch if there are no open windows.
  // TODO(https://crbug.com/331931430): This code should ignore browsers that
  // are closing (where IsBrowserClosing() returns true), but doing so is
  // tricky.
  if (!do_launch) {
    bool had_windows = delegate_->ShowAppWindows(profile, params.app_id);
    if (!had_windows && profile_state && !profile_state->browsers.empty()) {
      // Try to activate the most recently used open window.
      BrowserList* browsers = BrowserList::GetInstance();
      Browser* browser = nullptr;
      for (auto it = browsers->begin_browsers_ordered_by_activation();
           it != browsers->end_browsers_ordered_by_activation(); ++it) {
        if ((*it)->profile() != profile) {
          continue;
        }
        if (!web_app::AppBrowserController::IsForWebApp(*it, params.app_id)) {
          continue;
        }
        browser = *it;
        break;
      }

      // If iterating the browsers by activation order didn't find any matching
      // windows fall back to showing an arbitrary one from our ProfileState
      // instead.
      if (!browser) {
        browser = *(profile_state->browsers.begin());
      }

      browser->window()->Show();
      had_windows = true;
    }

    if (!had_windows) {
      do_launch = true;
    }
  }

  if (do_launch) {
    delegate_->LaunchApp(profile, params.app_id, params.files, params.urls,
                         params.override_url, params.login_item_restore_state,
                         std::move(launch_finished_callback));
  } else {
    std::move(launch_finished_callback).Run();
  }
}

// static
AppShimManager* AppShimManager::Get() {
  // This will only return nullptr in certain unit tests that do not initialize
  // the app shim host manager.
  return g_browser_process->platform_part()->app_shim_manager();
}

void AppShimManager::LoadProfileAndApp(const base::FilePath& profile_path,
                                       const webapps::AppId& app_id,
                                       LoadProfileAndAppCallback callback) {
  // Run |profile_loaded_callback| when the profile is loaded (be that now, or
  // after having to asynchronously load the profile).
  auto profile_loaded_callback = base::BindOnce(
      &AppShimManager::LoadProfileAndApp_OnProfileLoaded,
      weak_factory_.GetWeakPtr(), profile_path, app_id, std::move(callback));
  if (auto* profile = ProfileForPath(profile_path))
    std::move(profile_loaded_callback).Run(profile);
  else
    LoadProfileAsync(profile_path, std::move(profile_loaded_callback));
}

void AppShimManager::LoadProfileAndApp_OnProfileLoaded(
    const base::FilePath& profile_path,
    const webapps::AppId& app_id,
    LoadProfileAndAppCallback callback,
    Profile* profile) {
  // It may be that the profile fails to load.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile) {
    LOG(ERROR) << "Failed to load profile from " << profile_path.value() << ".";
    std::move(callback).Run(nullptr);
    return;
  }
  // Run |registry_ready_callback| when the WebAppProvider is ready (be that
  // now, or after a callback). Failing to do so will result in apps not
  // launching.
  // https://crbug.com/1094419.
  auto registry_ready_callback = base::BindOnce(
      &AppShimManager::LoadProfileAndApp_OnProfileAppRegistryReady,
      weak_factory_.GetWeakPtr(), profile_path, app_id, std::move(callback));
  WaitForAppRegistryReadyAsync(profile, std::move(registry_ready_callback));
}

void AppShimManager::LoadProfileAndApp_OnProfileAppRegistryReady(
    const base::FilePath& profile_path,
    const webapps::AppId& app_id,
    LoadProfileAndAppCallback callback) {
  // It may be that the profile was destroyed while waiting for the callback to
  // be issued.
  Profile* profile = ProfileForPath(profile_path);
  if (!profile) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Run |app_enabled_callback| once the app is enabled (now or async). Note
  // that this is only relevant for extension-based apps.
  auto app_enabled_callback = base::BindOnce(
      &AppShimManager::LoadProfileAndApp_OnAppEnabled,
      weak_factory_.GetWeakPtr(), profile_path, app_id, std::move(callback));
  if (delegate_->AppIsInstalled(profile, app_id)) {
    std::move(app_enabled_callback).Run();
  } else {
    delegate_->EnableExtension(profile, app_id,
                               std::move(app_enabled_callback));
  }
}

void AppShimManager::LoadProfileAndApp_OnAppEnabled(
    const base::FilePath& profile_path,
    const webapps::AppId& app_id,
    LoadProfileAndAppCallback callback) {
  std::move(callback).Run(ProfileForPath(profile_path));
}

// UMA metric name for result of validating app shim signature.
constexpr const char* kAppShimSignatureValidationResult =
    "Apps.AppShimSignatureValidationResult";

// Result of validating app shim signature.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SignatureValidationResult {
  kInvalidSignature = 0,
  kSuccessAdHoc = 1,
  kSuccessLegacy = 2,
  kExpectedAdHocGotLegacy = 3,
  kMaxValue = kExpectedAdHocGotLegacy,
};

// Records the result of validating the app shim code signature to UMA.
void RecordSignatureValidationResult(SignatureValidationResult result) {
  base::UmaHistogramEnumeration(kAppShimSignatureValidationResult, result);
}

bool AppShimManager::IsAcceptablyCodeSigned(audit_token_t audit_token) const {
  static const bool requires_adhoc_signature =
      web_app::UseAdHocSigningForWebAppShims();

  if (requires_adhoc_signature && IsAcceptablyAdHocCodeSigned(audit_token)) {
    RecordSignatureValidationResult(SignatureValidationResult::kSuccessAdHoc);
    return true;
  }

  if (IsAcceptablyCodeSignedLegacy(audit_token)) {
    if (requires_adhoc_signature) {
      RecordSignatureValidationResult(
          SignatureValidationResult::kExpectedAdHocGotLegacy);

      // Returning false to indicate that the signature is invalid will trigger
      // the recreation of the app shim app bundle. This will result in it
      // being re-signed with an ad-hoc signature as expected.
      return false;
    }

    RecordSignatureValidationResult(SignatureValidationResult::kSuccessLegacy);
    return true;
  }

  RecordSignatureValidationResult(SignatureValidationResult::kInvalidSignature);
  return false;
}

Profile* AppShimManager::ProfileForPath(const base::FilePath& full_path) {
  if (!profile_manager_)
    return nullptr;
  Profile* profile = profile_manager_->GetProfileByPath(full_path);

  // Use IsValidProfile to check if the profile has been created.
  return profile && profile_manager_->IsValidProfile(profile) ? profile
                                                              : nullptr;
}

void AppShimManager::LoadProfileAsync(
    const base::FilePath& full_path,
    base::OnceCallback<void(Profile*)> callback) {
  profile_manager_->LoadProfileByPath(full_path, false, std::move(callback));
}

void AppShimManager::WaitForAppRegistryReadyAsync(
    Profile* profile,
    base::OnceCallback<void()> callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);
  if (provider->on_registry_ready().is_signaled())
    std::move(callback).Run();
  else
    provider->on_registry_ready().Post(FROM_HERE, std::move(callback));
}

bool AppShimManager::IsProfileLockedForPath(const base::FilePath& full_path) {
  return profiles::IsProfileLocked(full_path);
}

std::unique_ptr<AppShimHost> AppShimManager::CreateHost(
    AppShimHost::Client* client,
    const base::FilePath& profile_path,
    const webapps::AppId& app_id,
    bool use_remote_cocoa) {
  return std::make_unique<AppShimHost>(client, app_id, profile_path,
                                       use_remote_cocoa);
}

void AppShimManager::OpenAppURLInBrowserWindow(
    const base::FilePath& profile_path,
    const GURL& url) {
  Profile* profile =
      profile_path.empty() ? nullptr : ProfileForPath(profile_path);
  if (!profile)
    profile = profile_manager_->GetLastUsedProfile();
  if (!profile || Browser::GetCreationStatusForProfile(profile) !=
                      Browser::CreationStatus::kOk) {
    return;
  }
  Browser* browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
  browser->window()->Show();
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.tabstrip_add_types = AddTabTypes::ADD_ACTIVE;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void AppShimManager::LaunchProfilePicker() {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
}

void AppShimManager::MaybeTerminate() {
  apps::AppShimTerminationManager::Get()->MaybeTerminate();
}

void AppShimManager::OnShimProcessDisconnected(AppShimHost* host) {
  const std::string app_id = host->GetAppId();

  auto found_app = apps_.find(app_id);
  CHECK(found_app != apps_.end(), base::NotFatalUntil::M130);
  AppState* app_state = found_app->second.get();
  DCHECK(app_state);

  app_state->MaybeSaveLastActiveProfiles();

  // For multi-profile apps, just delete the AppState, which will take down
  // |host| and all profiles' state.
  if (app_state->IsMultiProfile()) {
    DCHECK_EQ(host, app_state->multi_profile_host.get());
    apps_.erase(found_app);
    if (apps_.empty())
      MaybeTerminate();
    return;
  }

  // For non-RemoteCocoa apps, close all of the windows only if the the shim
  // process has successfully connected (if it never connected, then let the
  // app run as normal).
  bool close_windows =
      !host->UsesRemoteViews() && host->HasBootstrapConnected();

  // Erase the ProfileState, which will delete |host|.
  Profile* profile = ProfileForPath(host->GetProfilePath());
  auto found_profile = app_state->profiles.find(profile);
  CHECK(found_profile != app_state->profiles.end(), base::NotFatalUntil::M130);
  ProfileState* profile_state = found_profile->second.get();
  DCHECK_EQ(host, profile_state->single_profile_host.get());
  app_state->profiles.erase(found_profile);
  host = nullptr;

  // Erase |app_state| if this was the last profile.
  if (app_state->profiles.empty())
    apps_.erase(found_app);

  // Close app windows if we decided to do so above.
  if (close_windows)
    delegate_->CloseAppWindows(profile, app_id);
}

void AppShimManager::OnShimFocus(AppShimHost* host) {
  // This path is only for legacy apps (which are perforce single-profile).
  if (host->UsesRemoteViews())
    return;

  // Legacy apps don't own their own windows, so when we focus the app,
  // what we really want to do is focus the Chrome windows.
  Profile* profile = ProfileForPath(host->GetProfilePath());
  delegate_->ShowAppWindows(profile, host->GetAppId());
}

void AppShimManager::OnShimReopen(AppShimHost* host) {
  if (app_shim_observer_) {
    app_shim_observer_->OnShimReopen(host->GetAppShimPid());
  }
  auto found_app = apps_.find(host->GetAppId());
  CHECK(found_app != apps_.end(), base::NotFatalUntil::M130);
  AppState* app_state = found_app->second.get();
  LoadAndLaunchAppParams params;
  params.app_id = host->GetAppId();
  LoadAndLaunchApp(
      app_state->IsMultiProfile() ? base::FilePath() : host->GetProfilePath(),
      params, base::DoNothing());
}

void AppShimManager::OnShimOpenedFiles(
    AppShimHost* host,
    const std::vector<base::FilePath>& files) {
  auto found_app = apps_.find(host->GetAppId());
  CHECK(found_app != apps_.end(), base::NotFatalUntil::M130);
  AppState* app_state = found_app->second.get();
  LoadAndLaunchAppParams params;
  params.app_id = host->GetAppId();
  params.files = files;
  LoadAndLaunchApp(
      app_state->IsMultiProfile() ? base::FilePath() : host->GetProfilePath(),
      params, base::DoNothing());
  if (app_shim_observer_) {
    app_shim_observer_->OnShimOpenedURLs(host->GetAppShimPid());
  }
}

void AppShimManager::OnShimSelectedProfile(AppShimHost* host,
                                           const base::FilePath& profile_path) {
  LaunchAppInProfile(host->GetAppId(), profile_path);
}

void AppShimManager::LaunchAppInProfile(const webapps::AppId& app_id,
                                        const base::FilePath& profile_path) {
  LoadAndLaunchAppParams params;
  params.app_id = app_id;
  LoadAndLaunchApp(profile_path, params, base::DoNothing());
}

void AppShimManager::OnShimOpenedAppSettings(AppShimHost* host) {
  // Retrieve the list of last-active profiles. If there are no last-active
  // profiles (which is rare -- e.g, when the last-active profiles were
  // removed), then use all profiles for which the app is installed.
  std::set<base::FilePath> last_active_profile_paths =
      AppShimRegistry::Get()->GetLastActiveProfilesForApp(host->GetAppId());
  if (last_active_profile_paths.empty()) {
    last_active_profile_paths =
        AppShimRegistry::Get()->GetInstalledProfilesForApp(host->GetAppId());
  }
  if (last_active_profile_paths.empty()) {
    return;
  }
  // Open settings in the first of these profiles.
  LoadProfileAsync(
      *last_active_profile_paths.begin(),
      base::BindOnce(
          [](const webapps::AppId& app_id, Profile* profile) {
            if (profile) {
              chrome::ShowWebAppSettings(
                  profile, app_id,
                  web_app::AppSettingsPageEntryPoint::kBrowserCommand);
            }
          },
          host->GetAppId()));
}

void AppShimManager::OnShimOpenedUrls(AppShimHost* host,
                                      const std::vector<GURL>& urls) {
  auto found_app = apps_.find(host->GetAppId());
  CHECK(found_app != apps_.end(), base::NotFatalUntil::M130);
  AppState* app_state = found_app->second.get();
  LoadAndLaunchAppParams params;
  params.app_id = host->GetAppId();
  params.urls = urls;
  LoadAndLaunchApp(
      app_state->IsMultiProfile() ? base::FilePath() : host->GetProfilePath(),
      params, base::DoNothing());
  if (app_shim_observer_) {
    app_shim_observer_->OnShimOpenedURLs(host->GetAppShimPid());
  }
}

void AppShimManager::OnShimOpenAppWithOverrideUrl(AppShimHost* host,
                                                  const GURL& override_url) {
  auto found_app = apps_.find(host->GetAppId());
  CHECK(found_app != apps_.end(), base::NotFatalUntil::M130);
  AppState* app_state = found_app->second.get();
  LoadAndLaunchAppParams params;
  params.app_id = host->GetAppId();
  params.override_url = override_url;
  LoadAndLaunchApp(
      app_state->IsMultiProfile() ? base::FilePath() : host->GetProfilePath(),
      params, base::DoNothing());
}

void AppShimManager::OnShimWillTerminate(AppShimHost* host) {
  auto found_app = apps_.find(host->GetAppId());
  CHECK(found_app != apps_.end(), base::NotFatalUntil::M130);
  AppState* app_state = found_app->second.get();
  DCHECK(app_state);

  auto* notification_bridge = static_cast<NotificationPlatformBridgeMac*>(
      g_browser_process->notification_platform_bridge());
  notification_bridge->AppShimWillTerminate(host->GetAppId());

  DCHECK(!app_state->did_save_last_active_profiles_on_terminate);
  app_state->MaybeSaveLastActiveProfiles();
  app_state->did_save_last_active_profiles_on_terminate = true;
}

void AppShimManager::OnNotificationPermissionStatusChanged(
    AppShimHost* host,
    mac_notifications::mojom::PermissionStatus status) {
  AppShimRegistry::Get()->SaveNotificationPermissionStatusForApp(
      host->GetAppId(), status);
}

void AppShimManager::OnProfileAdded(Profile* profile) {
  if (profile->IsOffTheRecord())
    return;

  // The app lifetime monitor service might not be available for some irregular
  // profiles, like the System Profile.
  if (AppLifetimeMonitor* app_lifetime_monitor =
          AppLifetimeMonitorFactory::GetForBrowserContext(profile)) {
    app_lifetime_monitor->AddObserver(this);
  }
}

void AppShimManager::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  if (profile->IsOffTheRecord())
    return;

  // The app lifetime monitor service might not be available for some irregular
  // profiles, like the System Profile.
  if (AppLifetimeMonitor* app_lifetime_monitor =
          AppLifetimeMonitorFactory::GetForBrowserContext(profile)) {
    app_lifetime_monitor->RemoveObserver(this);
  }

  // Close app shims that were kept alive only for this profile.
  for (auto iter_app = apps_.begin(); iter_app != apps_.end();) {
    AppState* app_state = iter_app->second.get();
    if (app_state->ShouldDeleteAppState()) {
      iter_app = apps_.erase(iter_app);
    } else {
      ++iter_app;
    }
  }
}

void AppShimManager::OnAppStart(content::BrowserContext* context,
                                const std::string& app_id) {}

void AppShimManager::OnAppActivated(content::BrowserContext* context,
                                    const std::string& app_id) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!delegate_->AppIsInstalled(profile, app_id))
    return;
  if (auto* profile_state = GetOrCreateProfileState(profile, app_id))
    profile_state->GetHost()->LaunchShim();
}

void AppShimManager::OnAppDeactivated(content::BrowserContext* context,
                                      const std::string& app_id) {
  Profile* profile = static_cast<Profile*>(context);
  auto found_app = apps_.find(app_id);
  if (found_app != apps_.end()) {
    AppState* app_state = found_app->second.get();
    auto found_profile = app_state->profiles.find(profile);
    if (found_profile != app_state->profiles.end()) {
      if (app_state->profiles.size() == 1) {
        app_state->MaybeSaveLastActiveProfiles();
      }
      app_state->profiles.erase(found_profile);
      if (app_state->ShouldDeleteAppState()) {
        apps_.erase(found_app);
      }
    }
  }

  if (apps_.empty())
    MaybeTerminate();

  // Check the integrity of AppState::profiles across all apps. Include the app
  // ID in the dump, to help pin down the cause.
  //
  // TODO(crbug.com/40217091): Remove this once we're confident this never
  // happens.
  std::string inconsistent_app_ids;
  for (const auto& [id, state] : apps_) {
    if (state->ShouldDeleteAppState()) {
      inconsistent_app_ids += id + " ";
    }
  }
  if (!inconsistent_app_ids.empty())
    DumpError(inconsistent_app_ids);
}

void AppShimManager::OnAppStop(content::BrowserContext* context,
                               const std::string& app_id) {}

void AppShimManager::OnBrowserAdded(Browser* browser) {
  Profile* profile = browser->profile();
  const std::string app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  if (!delegate_->AppUsesRemoteCocoa(profile, app_id))
    return;
  if (auto* profile_state = GetOrCreateProfileState(profile, app_id)) {
    profile_state->browsers.insert(browser);
    if (profile_state->browsers.size() == 1)
      OnAppActivated(browser->profile(), app_id);
  }
}

void AppShimManager::OnBrowserRemoved(Browser* browser) {
  // We can't call OnAppDeactivated() while iterating on |apps_|. It would
  // invalidate the iterator.
  std::vector<std::string> apps_to_deactivate;

  for (const auto& [app_id, app_state] : apps_) {
    for (const auto& [profile, profile_state] : app_state->profiles) {
      auto found = profile_state->browsers.find(browser);
      if (found != profile_state->browsers.end()) {
        // If we have no browser windows open after erasing this window, then
        // close the ProfileState (and potentially the shim as well).
        profile_state->browsers.erase(found);
        if (profile_state->browsers.empty())
          apps_to_deactivate.push_back(app_id);
        break;  // Break to outer loop.
      }
    }
  }

  for (const std::string& app_id : apps_to_deactivate)
    OnAppDeactivated(browser->profile(), app_id);
}

void AppShimManager::OnBrowserSetLastActive(Browser* browser) {
  // Rebuild the profile menu items (to ensure that the checkmark in the menu
  // is next to the new-active item).
  if (avatar_menu_)
    avatar_menu_->ActiveBrowserChanged(browser);
  UpdateAllProfileMenus();

  // Update the application dock menu for the current profile.
  const std::string app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  if (!delegate_->AppUsesRemoteCocoa(browser->profile(), app_id))
    return;
  auto* profile_state = GetOrCreateProfileState(browser->profile(), app_id);
  if (profile_state)
    UpdateApplicationDockMenu(browser->profile(), profile_state);
}

void AppShimManager::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.RemoveObservation(profile);

  // Clean up dangling Profile pointers. This can happen in rare cases, if a
  // Browser is never created for a particular Profile. In those cases,
  // OnBrowserRemoved() never runs, and it doesn't clean up AppState::profiles.
  //
  // Use the same pattern as in OnBrowserRemoved() to avoid invalidating the
  // iterator.
  std::vector<std::string> apps_to_deactivate;

  for (const auto& [app_id, app_state] : apps_) {
    auto found = app_state->profiles.find(profile);
    if (found != app_state->profiles.end()) {
      CHECK(found->second->browsers.empty());
      apps_to_deactivate.push_back(app_id);
    }
  }

  for (const std::string& app_id : apps_to_deactivate)
    OnAppDeactivated(profile, app_id);
}

void AppShimManager::OnAppLaunchCancelled(content::BrowserContext* context,
                                          const std::string& app_id) {
  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end())
    return;

  Profile* profile = static_cast<Profile*>(context);
  AppState* app_state = found_app->second.get();
  auto found_profile = app_state->profiles.find(profile);
  if (found_profile == app_state->profiles.end())
    return;

  // If there are no browser windows open, then close the ProfileState
  // (and potentially the shim as well).
  ProfileState* profile_state = found_profile->second.get();
  if (profile_state->browsers.empty())
    OnAppDeactivated(context, app_id);
}

void AppShimManager::UpdateAllProfileMenus() {
  RebuildProfileMenuItemsFromAvatarMenu();
  for (auto& iter_app : apps_) {
    AppState* app_state = iter_app.second.get();
    if (app_state->IsMultiProfile())
      UpdateAppProfileMenu(app_state);
  }
}

void AppShimManager::RebuildProfileMenuItemsFromAvatarMenu() {
  if (!avatar_menu_) {
    avatar_menu_ = std::make_unique<AvatarMenu>(
        &profile_manager_->GetProfileAttributesStorage(), this, nullptr);
  }
  avatar_menu_->RebuildMenu();
  profile_menu_items_.clear();
  for (size_t i = 0; i < avatar_menu_->GetNumberOfItems(); ++i) {
    auto mojo_item = chrome::mojom::ProfileMenuItem::New();
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(i);
    mojo_item->name = item.name;
    mojo_item->menu_index = item.menu_index;
    mojo_item->active = item.active;
    mojo_item->profile_path = item.profile_path;
    if (base::FeatureList::IsEnabled(kAppShimProfileMenuIcons)) {
      mojo_item->icon =
          profiles::GetAvatarIconForNSMenu(item.profile_path).ToImageSkia()[0];
    }
    profile_menu_items_.push_back(std::move(mojo_item));
  }
}

void AppShimManager::OnAvatarMenuChanged(AvatarMenu* menu) {
  // Rebuild the profile menu to reflect changes (e.g, added or removed
  // profiles).
  DCHECK_EQ(avatar_menu_.get(), menu);
  UpdateAllProfileMenus();
}

void AppShimManager::UpdateAppProfileMenu(AppState* app_state) {
  DCHECK(app_state->IsMultiProfile());
  // Include in |items| the profiles from |profile_menu_items_| for which this
  // app is installed, sorted by |menu_index|.
  std::vector<chrome::mojom::ProfileMenuItemPtr> items;
  auto installed_profiles =
      AppShimRegistry::Get()->GetInstalledProfilesForApp(app_state->app_id);
  for (const auto& item : profile_menu_items_) {
    if (installed_profiles.count(item->profile_path))
      items.push_back(item->Clone());
  }
  std::sort(items.begin(), items.end(), ProfileMenuItemComparator);

  // Do not show a profile menu unless it has at least 2 entries (that is, the
  // app is available for at least 2 profiles).
  if (items.size() < 2)
    items.clear();

  // Send the profile menu to the app shim process.
  app_state->multi_profile_host->GetAppShim()->UpdateProfileMenu(
      std::move(items));
}

void AppShimManager::UpdateApplicationDockMenu(Profile* profile,
                                               ProfileState* profile_state) {
  AppState* app_state = profile_state->app_state;
  // Send the application dock menu to the app shim process.
  profile_state->GetHost()->GetAppShim()->UpdateApplicationDockMenu(
      delegate_->GetAppShortcutsMenuItemInfos(profile, app_state->app_id));
}

AppShimManager::ProfileState* AppShimManager::GetOrCreateProfileState(
    Profile* profile,
    const webapps::AppId& app_id) {
  if (web_app::AppShimCreationAndLaunchDisabledForTest()) {
    return nullptr;
  }

  const bool is_multi_profile = delegate_->AppIsMultiProfile(profile, app_id);
  const base::FilePath profile_path =
      is_multi_profile ? base::FilePath() : profile->GetPath();
  const bool use_remote_cocoa = delegate_->AppUsesRemoteCocoa(profile, app_id);

  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end()) {
    std::unique_ptr<AppShimHost> multi_profile_host;
    if (is_multi_profile) {
      multi_profile_host =
          CreateHost(this, profile_path, app_id, use_remote_cocoa);
    }
    auto new_app_state =
        std::make_unique<AppState>(app_id, std::move(multi_profile_host));
    found_app =
        apps_.insert(std::make_pair(app_id, std::move(new_app_state))).first;
  }
  AppState* app_state = found_app->second.get();

  // Initialize the profile menu.
  if (is_multi_profile)
    UpdateAppProfileMenu(app_state);

  auto found_profile = app_state->profiles.find(profile);
  if (found_profile == app_state->profiles.end()) {
    std::unique_ptr<AppShimHost> single_profile_host;
    if (!is_multi_profile) {
      single_profile_host =
          CreateHost(this, profile_path, app_id, use_remote_cocoa);
    }
    auto new_profile_state = std::make_unique<ProfileState>(
        app_state, std::move(single_profile_host));
    found_profile =
        app_state->profiles
            .insert(std::make_pair(profile, std::move(new_profile_state)))
            .first;
  }

  // Listen for OnProfileWillBeDestroyed(), but not more than once per Profile.
  // O(n), where n is the number of loaded Profiles (AKA a very small number).
  if (!profile_observation_.IsObservingSource(profile))
    profile_observation_.AddObservation(profile);

  return found_profile->second.get();
}

std::map<base::FilePath, int> AppShimManager::GetProfilesWithMatchingHandlers(
    const LoadAndLaunchAppParams& params) {
  if (!params.HasFilesOrURLs())
    return {};
  std::map<base::FilePath, int> result;

  // Files can be passed both as files or as file:// URLs, so gather all
  // the files from both.
  std::vector<base::FilePath> files = params.files;
  GURL protocol_handler_url;
  for (const GURL& url : params.urls) {
    // Ignore invalid URLs.
    if (!url.is_valid() || !url.has_scheme())
      continue;

    if (url.SchemeIsFile()) {
      base::FilePath file_path;
      if (net::FileURLToFilePath(url, &file_path))
        files.push_back(file_path);
      continue;
    }

    protocol_handler_url = url;
  }

  // For each profile with available handlers, count how many paths and/or
  // URLs those profiles can handle.
  std::map<base::FilePath, AppShimRegistry::HandlerInfo> handlers =
      AppShimRegistry::Get()->GetHandlersForApp(params.app_id);
  for (const auto& [profile, handler_info] : handlers) {
    int count = base::ranges::count_if(
        files, [&handler_info](const base::FilePath& file_path) {
          std::string file_extension =
              base::FilePath(file_path.Extension()).AsUTF8Unsafe();
          return file_extension.length() > 1 &&
                 base::Contains(handler_info.file_handler_extensions,
                                file_extension);
        });

    if (protocol_handler_url.is_valid() &&
        base::Contains(handler_info.protocol_handlers,
                       protocol_handler_url.scheme())) {
      count++;
    }

    if (count > 0)
      result[profile] = count;
  }
  return result;
}

AppShimManager::LoadAndLaunchAppParams::LoadAndLaunchAppParams() = default;

AppShimManager::LoadAndLaunchAppParams::~LoadAndLaunchAppParams() = default;

AppShimManager::LoadAndLaunchAppParams::LoadAndLaunchAppParams(
    const LoadAndLaunchAppParams&) = default;

AppShimManager::LoadAndLaunchAppParams&
AppShimManager::LoadAndLaunchAppParams::operator=(
    const LoadAndLaunchAppParams&) = default;

bool AppShimManager::LoadAndLaunchAppParams::HasFilesOrURLs() const {
  return !files.empty() || !urls.empty() || !override_url.is_empty();
}

base::apple::ScopedCFTypeRef<CFStringRef>
AppShimManager::BuildAppShimRequirementStringFromFrameworkRequirementString(
    CFStringRef framwork_requirement) {
  // Make sure the framework bundle requirement is in the expected format.
  // It should start with 'identifier "' and have at least 2 quotes. This allows
  // us to easily find the end of the "identifier" portion of the requirement so
  // we can swap in the desired app shim identifier leaving rest of the
  // requirement unmodified.
  CFIndex len = CFStringGetLength(framwork_requirement);
  base::apple::ScopedCFTypeRef<CFArrayRef> quote_ranges(
      CFStringCreateArrayWithFindResults(nullptr, framwork_requirement,
                                         CFSTR("\""), CFRangeMake(0, len), 0));
  if (!CFStringHasPrefix(framwork_requirement, CFSTR("identifier \"")) ||
      !quote_ranges || CFArrayGetCount(quote_ranges.get()) < 2) {
    DumpError("Framework bundle requirement is malformed.");
    return base::apple::ScopedCFTypeRef<CFStringRef>(nullptr);
  }

  // Get the index of the second quote.
  CFIndex second_quote_index =
      static_cast<const CFRange*>(CFArrayGetValueAtIndex(quote_ranges.get(), 1))
          ->location;

  // Make sure there is something to read after the second quote.
  if (second_quote_index + 1 >= len) {
    DumpError("Framework bundle requirement is too short");
    return base::apple::ScopedCFTypeRef<CFStringRef>(nullptr);
  }

  // Build the app shim requirement. Keep the data from the framework bundle
  // requirement starting after second quote.
  base::apple::ScopedCFTypeRef<CFStringRef> right_of_second_quote(
      CFStringCreateWithSubstring(
          nullptr, framwork_requirement,
          CFRangeMake(second_quote_index + 1, len - second_quote_index - 1)));
  base::apple::ScopedCFTypeRef<CFMutableStringRef> shim_requirement_string(
      CFStringCreateMutableCopy(nullptr, 0,
                                CFSTR("identifier \"app_mode_loader\"")));
  CFStringAppend(shim_requirement_string.get(), right_of_second_quote.get());
  return shim_requirement_string;
}

}  // namespace apps
