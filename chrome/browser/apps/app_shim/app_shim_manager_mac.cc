// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <algorithm>
#include <set>
#include <utility>

#include "apps/app_lifetime_monitor_factory.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/components/app_shim_registry_mac.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {

// Create a SHA1 hex digest of a certificate, for use specifically in building
// a code signing requirement string in IsAcceptablyCodeSigned(), below.
std::string CertificateSHA1Digest(SecCertificateRef certificate) {
  base::ScopedCFTypeRef<CFDataRef> certificate_data(
      SecCertificateCopyData(certificate));
  char hash[base::kSHA1Length];
  base::SHA1HashBytes(CFDataGetBytePtr(certificate_data),
                      CFDataGetLength(certificate_data),
                      reinterpret_cast<unsigned char*>(hash));
  return base::HexEncode(hash, base::kSHA1Length);
}

// Returns whether |pid|'s code signature is trusted:
// - True if the caller is unsigned (there's nothing to verify).
// - True if |pid| satisfies the caller's designated requirement.
// - False otherwise (|pid| does not satisfy caller's designated requirement).
bool IsAcceptablyCodeSignedInternal(pid_t pid) {
  base::ScopedCFTypeRef<SecCodeRef> own_code;
  base::ScopedCFTypeRef<CFDictionaryRef> own_signing_info;

  // Fetch the calling process's designated requirement. The shim can only be
  // validated if the caller has one (i.e. if the caller is code signed).
  //
  // Note: Don't validate |own_code|: updates modify the browser's bundle and
  // invalidate its code signature while an update is pending. This can be
  // revisited after https://crbug.com/496298 is resolved.
  if (SecCodeCopySelf(kSecCSDefaultFlags, own_code.InitializeInto()) !=
          errSecSuccess ||
      SecCodeCopySigningInformation(own_code.get(), kSecCSSigningInformation,
                                    own_signing_info.InitializeInto()) !=
          errSecSuccess) {
    LOG(ERROR) << "Failed to get own code signing information.";
    return false;
  }

  auto* own_certificates = base::mac::GetValueFromDictionary<CFArrayRef>(
      own_signing_info, kSecCodeInfoCertificates);
  if (!own_certificates || CFArrayGetCount(own_certificates) < 1) {
    return true;
  }

  auto* own_certificate = base::mac::CFCast<SecCertificateRef>(
      CFArrayGetValueAtIndex(own_certificates, 0));
  auto own_certificate_hash = CertificateSHA1Digest(own_certificate);

  base::ScopedCFTypeRef<CFStringRef> shim_requirement_string(
      CFStringCreateWithFormat(
          kCFAllocatorDefault, nullptr,
          CFSTR(
              "identifier \"app_mode_loader\" and certificate leaf = H\"%s\""),
          own_certificate_hash.c_str()));

  base::ScopedCFTypeRef<SecRequirementRef> shim_requirement;
  if (SecRequirementCreateWithString(
          shim_requirement_string, kSecCSDefaultFlags,
          shim_requirement.InitializeInto()) != errSecSuccess) {
    LOG(ERROR)
        << "Failed to create a SecRequirementRef from the requirement string \""
        << shim_requirement_string << "\"";
    return false;
  }

  base::ScopedCFTypeRef<SecCodeRef> guest_code;

  base::ScopedCFTypeRef<CFNumberRef> pid_cf(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid));
  const void* guest_attribute_keys[] = {kSecGuestAttributePid};
  const void* guest_attribute_values[] = {pid_cf};
  base::ScopedCFTypeRef<CFDictionaryRef> guest_attributes(CFDictionaryCreate(
      nullptr, guest_attribute_keys, guest_attribute_values,
      base::size(guest_attribute_keys), &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  if (SecCodeCopyGuestWithAttributes(nullptr, guest_attributes,
                                     kSecCSDefaultFlags,
                                     guest_code.InitializeInto())) {
    LOG(ERROR) << "Failed to create a SecCodeRef from the app shim's pid.";
    return false;
  }

  return SecCodeCheckValidity(guest_code, kSecCSDefaultFlags,
                              shim_requirement) == errSecSuccess;
}

bool ProfileMenuItemComparator(const chrome::mojom::ProfileMenuItemPtr& a,
                               const chrome::mojom::ProfileMenuItemPtr& b) {
  return a->menu_index < b->menu_index;
}

}  // namespace

namespace apps {

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
  AppShimManager::AppState* const app_state;

  // The AppShimHost for apps that are not multi-profile.
  const std::unique_ptr<AppShimHost> single_profile_host;

  // All browser instances for this (app, Profile) pair.
  std::set<Browser*> browsers;
};

// The state for an individual app. This includes the state for all
// profiles that are using the app.
struct AppShimManager::AppState {
  AppState(const web_app::AppId& app_id,
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
  // when the app is started next.
  void SaveLastActiveProfiles() const;

  const std::string app_id;

  // Multi-profile apps share the same shim process across multiple profiles.
  const std::unique_ptr<AppShimHost> multi_profile_host;

  // The profile state for the profiles currently running this app.
  std::map<Profile*, std::unique_ptr<ProfileState>> profiles;
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
    return profiles.empty() &&
           AppShimRegistry::Get()->GetInstalledProfilesForApp(app_id).empty();
  }

  // The old behavior, and the behavior for single-profile apps, is to close
  // only when all profiles are closed.
  return profiles.empty();
}

void AppShimManager::AppState::SaveLastActiveProfiles() const {
  if (!IsMultiProfile())
    return;
  std::set<base::FilePath> last_active_profile_paths;
  for (auto iter_profile = profiles.begin(); iter_profile != profiles.end();
       ++iter_profile) {
    last_active_profile_paths.insert(iter_profile->first->GetPath());
  }
  AppShimRegistry::Get()->OnAppQuit(app_id, last_active_profile_paths);
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

void AppShimManager::OnBeginTearDown() {
  avatar_menu_.reset();
  if (profile_manager_)
    profile_manager_->RemoveObserver(this);
  profile_manager_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
}

AppShimHost* AppShimManager::FindHost(Profile* profile,
                                      const web_app::AppId& app_id) {
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

void AppShimManager::OnShimLaunchRequested(
    AppShimHost* host,
    bool recreate_shims,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  // A shim can only be launched through an active profile, so find a profile
  // through which to do the launch. For multi-profile apps, select one
  // arbitrarily. For non-multi-profile apps, select the specified profile.
  Profile* profile = nullptr;
  {
    auto found_app = apps_.find(host->GetAppId());
    DCHECK(found_app != apps_.end());
    AppState* app_state = found_app->second.get();
    if (app_state->IsMultiProfile()) {
      DCHECK(!app_state->profiles.empty());
      profile = app_state->profiles.begin()->first;
    } else {
      profile = ProfileForPath(host->GetProfilePath());
    }
  }
  delegate_->LaunchShim(profile, host->GetAppId(), recreate_shims,
                        std::move(launched_callback),
                        std::move(terminated_callback));
}

void AppShimManager::OnShimProcessConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  DCHECK(crx_file::id_util::IdIsValid(bootstrap->GetAppId()));
  switch (bootstrap->GetLaunchType()) {
    case chrome::mojom::AppShimLaunchType::kNormal: {
      const web_app::AppId app_id = bootstrap->GetAppId();
      const base::FilePath profile_path = bootstrap->GetProfilePath();
      const std::vector<base::FilePath> launch_files =
          bootstrap->GetLaunchFiles();
      const chrome::mojom::AppShimLoginItemRestoreState
          login_item_restore_state = bootstrap->GetLoginItemRestoreState();
      LoadAndLaunchAppCallback launch_callback = base::BindOnce(
          &AppShimManager::OnShimProcessConnectedAndAllLaunchesDone,
          weak_factory_.GetWeakPtr(), std::move(bootstrap));
      LoadAndLaunchApp(app_id, profile_path, launch_files,
                       login_item_restore_state, std::move(launch_callback));
      break;
    }
    case chrome::mojom::AppShimLaunchType::kRegisterOnly:
      OnShimProcessConnectedForRegisterOnly(std::move(bootstrap));
      break;
  }
}

void AppShimManager::OnShimProcessConnectedForRegisterOnly(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  const web_app::AppId& app_id = bootstrap->GetAppId();
  DCHECK_EQ(bootstrap->GetLaunchType(),
            chrome::mojom::AppShimLaunchType::kRegisterOnly);

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
      DCHECK(!app_state->profiles.empty());
      profile_state = app_state->profiles.begin()->second.get();
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

void AppShimManager::LoadAndLaunchApp(
    const web_app::AppId& app_id,
    const base::FilePath& profile_path,
    const std::vector<base::FilePath>& launch_files,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
    LoadAndLaunchAppCallback launch_callback) {
  // Check to see if the app is already running for a profile compatible with
  // |profile_path|. If so, early-out.
  if (LoadAndLaunchApp_TryExistingProfileStates(
          app_id, profile_path, launch_files, login_item_restore_state,
          &launch_callback)) {
    // If we used an existing profile, |launch_callback| should have been run.
    DCHECK(!launch_callback);
    return;
  }

  // Retrieve the list of last-active profiles. If there are no last-active
  // profiles (which is rare -- e.g, when the last-active profiles were
  // removed), then use all profiles for which the app is installed.
  std::set<base::FilePath> last_active_profile_paths =
      AppShimRegistry::Get()->GetLastActiveProfilesForApp(app_id);
  if (last_active_profile_paths.empty()) {
    last_active_profile_paths =
        AppShimRegistry::Get()->GetInstalledProfilesForApp(app_id);
  }

  // Construct |profile_paths_to_launch| to be the list of all profiles to
  // attempt to launch, starting with the profile specified in |bootstrap|,
  // at the front of the list.
  std::vector<base::FilePath> profile_paths_to_launch = {profile_path};
  for (const auto& profile_path : last_active_profile_paths)
    profile_paths_to_launch.push_back(profile_path);

  // Attempt load all of the profiles in |profile_paths_to_launch|, and once
  // they're loaded (or have failed to load), call
  // OnShimProcessConnectedAndProfilesToLaunchLoaded.
  base::OnceClosure callback =
      base::BindOnce(&AppShimManager::LoadAndLaunchApp_OnProfilesAndAppReady,
                     weak_factory_.GetWeakPtr(), app_id, launch_files,
                     login_item_restore_state, profile_paths_to_launch,
                     std::move(launch_callback));
  {
    // This will update |callback| to be a chain of callbacks that load the
    // profiles in |profile_paths_to_load|, one by one, using
    // LoadProfileAndApp, and then finally call the initial |callback|. This
    // may end up being async (if some profiles aren't loaded), or may be
    // synchronous (if all profiles happen to already be loaded).
    for (const auto& profile_path : profile_paths_to_launch) {
      if (profile_path.empty())
        continue;
      LoadProfileAndAppCallback callback_wrapped =
          base::BindOnce([](base::OnceClosure callback_to_wrap,
                            Profile*) { std::move(callback_to_wrap).Run(); },
                         std::move(callback));
      callback = base::BindOnce(&AppShimManager::LoadProfileAndApp,
                                weak_factory_.GetWeakPtr(), profile_path,
                                app_id, std::move(callback_wrapped));
    }
  }
  std::move(callback).Run();
}

bool AppShimManager::LoadAndLaunchApp_TryExistingProfileStates(
    const web_app::AppId& app_id,
    const base::FilePath& profile_path,
    const std::vector<base::FilePath>& launch_files,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
    LoadAndLaunchAppCallback* launch_callback) {
  auto found_app = apps_.find(app_id);
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
    // If no profile was specified, select the first open profile encountered.
    // TODO(https://crbug.com/829689): This should select the most-recently-used
    // profile, not the first profile encountered.
    auto it = app_state->profiles.begin();
    if (it != app_state->profiles.end()) {
      profile = it->first;
      profile_state = it->second.get();
    }
  }
  if (!profile_state)
    return false;
  DCHECK(profile);

  // Launch the app, if appropriate.
  LoadAndLaunchApp_LaunchIfAppropriate(profile, profile_state, app_id,
                                       launch_files, login_item_restore_state);

  std::move(*launch_callback)
      .Run(profile_state, chrome::mojom::AppShimLaunchResult::kSuccess);
  return true;
}

void AppShimManager::LoadAndLaunchApp_OnProfilesAndAppReady(
    const web_app::AppId& app_id,
    const std::vector<base::FilePath>& launch_files,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
    const std::vector<base::FilePath>& profile_paths_to_launch,
    LoadAndLaunchAppCallback launch_callback) {
  // Launch all of the profiles in |profile_paths_to_launch|. Record the most
  // profile successfully launched in |launched_profile_state|, and the most
  // recent reason for a failure (if any) in |launch_result|.
  ProfileState* launched_profile_state = nullptr;
  auto launch_result = chrome::mojom::AppShimLaunchResult::kProfileNotFound;
  for (size_t iter = 0; iter < profile_paths_to_launch.size(); ++iter) {
    const base::FilePath& profile_path = profile_paths_to_launch[iter];
    if (profile_path.empty())
      continue;
    if (IsProfileLockedForPath(profile_path)) {
      launch_result = chrome::mojom::AppShimLaunchResult::kProfileLocked;
      continue;
    }
    Profile* profile = ProfileForPath(profile_path);
    if (!profile) {
      launch_result = chrome::mojom::AppShimLaunchResult::kProfileNotFound;
      continue;
    }
    if (!delegate_->AppIsInstalled(profile, app_id)) {
      launch_result = chrome::mojom::AppShimLaunchResult::kAppNotFound;
      continue;
    }

    // Create a ProfileState for this app, if appropriate (e.g, not for
    // open-in-a-tab bookmark apps).
    ProfileState* profile_state = nullptr;
    if (delegate_->AppCanCreateHost(profile, app_id))
      profile_state = GetOrCreateProfileState(profile, app_id);

    // Launch the app, if appropriate.
    LoadAndLaunchApp_LaunchIfAppropriate(
        profile, profile_state, app_id, launch_files, login_item_restore_state);

    // If we successfully created a profile state, save it for |bootstrap| to
    // connect to once all launches are done.
    if (profile_state)
      launched_profile_state = profile_state;
    else
      launch_result = chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect;

    // If files were specified, only open one new window.
    if (!launch_files.empty())
      break;

    // If this was the first profile in |profile_paths_to_launch|, then this
    // was the profile specified in the bootstrap, so stop here.
    if (iter == 0)
      break;
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
  // If we failed because the profile was locked, launch the profile manager.
  if (result == chrome::mojom::AppShimLaunchResult::kProfileLocked)
    LaunchProfilePicker();

  // If the app specified a URL, but we tried and failed to launch it, then
  // open that URL in a new browser window.
  if (result != chrome::mojom::AppShimLaunchResult::kSuccess &&
      result != chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect &&
      bootstrap->GetLaunchType() == chrome::mojom::AppShimLaunchType::kNormal) {
    const GURL& url = bootstrap->GetAppURL();
    if (url.is_valid())
      OpenAppURLInBrowserWindow(bootstrap->GetProfilePath(), url);
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
  if (!IsAcceptablyCodeSigned(bootstrap->GetAppShimPid())) {
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
    const web_app::AppId& app_id,
    const std::vector<base::FilePath>& launch_files,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state) {
  // If |launch_files| is non-empty, then always do a launch to open the
  // files.
  bool do_launch = !launch_files.empty();

  // Otherwise, only launch if there are no open windows.
  if (!do_launch) {
    bool had_windows = delegate_->ShowAppWindows(profile, app_id);
    if (profile_state)
      had_windows |= !profile_state->browsers.empty();

    if (!had_windows)
      do_launch = true;
  }

  if (do_launch)
    delegate_->LaunchApp(profile, app_id, launch_files,
                         login_item_restore_state);
}

// static
AppShimManager* AppShimManager::Get() {
  // This will only return nullptr in certain unit tests that do not initialize
  // the app shim host manager.
  return g_browser_process->platform_part()->app_shim_manager();
}

void AppShimManager::LoadProfileAndApp(const base::FilePath& profile_path,
                                       const web_app::AppId& app_id,
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
    const web_app::AppId& app_id,
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
    const web_app::AppId& app_id,
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
    const web_app::AppId& app_id,
    LoadProfileAndAppCallback callback) {
  std::move(callback).Run(ProfileForPath(profile_path));
}

bool AppShimManager::IsAcceptablyCodeSigned(pid_t pid) const {
  return IsAcceptablyCodeSignedInternal(pid);
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
  auto* provider = web_app::WebAppProvider::Get(profile);
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
    const web_app::AppId& app_id,
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
  if (!profile)
    return;
  Browser* browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
  browser->window()->Show();
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.tabstrip_add_types = TabStripModel::ADD_ACTIVE;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void AppShimManager::LaunchProfilePicker() {
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileLocked);
}

void AppShimManager::MaybeTerminate() {
  apps::AppShimTerminationManager::Get()->MaybeTerminate();
}

void AppShimManager::OnShimProcessDisconnected(AppShimHost* host) {
  const std::string app_id = host->GetAppId();

  auto found_app = apps_.find(app_id);
  DCHECK(found_app != apps_.end());
  AppState* app_state = found_app->second.get();
  DCHECK(app_state);

  // For multi-profile apps, just delete the AppState, which will take down
  // |host| and all profiles' state.
  if (app_state->IsMultiProfile()) {
    app_state->SaveLastActiveProfiles();
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
  DCHECK(found_profile != app_state->profiles.end());
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
  auto found_app = apps_.find(host->GetAppId());
  DCHECK(found_app != apps_.end());
  AppState* app_state = found_app->second.get();
  LoadAndLaunchApp(
      host->GetAppId(),
      app_state->IsMultiProfile() ? base::FilePath() : host->GetProfilePath(),
      std::vector<base::FilePath>(),
      chrome::mojom::AppShimLoginItemRestoreState::kNone, base::DoNothing());
}

void AppShimManager::OnShimOpenedFiles(
    AppShimHost* host,
    const std::vector<base::FilePath>& files) {
  auto found_app = apps_.find(host->GetAppId());
  DCHECK(found_app != apps_.end());
  AppState* app_state = found_app->second.get();
  LoadAndLaunchApp(
      host->GetAppId(),
      app_state->IsMultiProfile() ? base::FilePath() : host->GetProfilePath(),
      files, chrome::mojom::AppShimLoginItemRestoreState::kNone,
      base::DoNothing());
}

void AppShimManager::OnShimSelectedProfile(AppShimHost* host,
                                           const base::FilePath& profile_path) {
  LoadAndLaunchApp(
      host->GetAppId(), profile_path, std::vector<base::FilePath>(),
      chrome::mojom::AppShimLoginItemRestoreState::kNone, base::DoNothing());
}

void AppShimManager::OnProfileAdded(Profile* profile) {
  if (profile->IsOffTheRecord())
    return;

  AppLifetimeMonitorFactory::GetForBrowserContext(profile)->AddObserver(this);
}

void AppShimManager::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  if (profile->IsOffTheRecord())
    return;

  AppLifetimeMonitorFactory::GetForBrowserContext(profile)->RemoveObserver(
      this);

  // Close app shims that were kept alive only for this profile. Note that this
  // must be done as a posted task because closing shims may result in closing
  // windows midway through BrowserList::TryToCloseBrowserList, which does not
  // expect that behavior, and may result in crashes.
  auto close_shims_lambda = [](base::WeakPtr<AppShimManager> manager) {
    if (!manager)
      return;
    for (auto iter_app = manager->apps_.begin();
         iter_app != manager->apps_.end();) {
      AppState* app_state = iter_app->second.get();
      if (app_state->ShouldDeleteAppState())
        iter_app = manager->apps_.erase(iter_app);
      else
        ++iter_app;
    }
  };
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(close_shims_lambda, weak_factory_.GetWeakPtr()));
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
      if (app_state->profiles.size() == 1)
        app_state->SaveLastActiveProfiles();
      app_state->profiles.erase(found_profile);
      if (app_state->ShouldDeleteAppState())
        apps_.erase(found_app);
    }
  }

  if (apps_.empty())
    MaybeTerminate();
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
  const std::string app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end())
    return;
  AppState* app_state = found_app->second.get();

  for (auto iter_profile = app_state->profiles.begin();
       iter_profile != app_state->profiles.end(); ++iter_profile) {
    ProfileState* profile_state = iter_profile->second.get();
    auto found = profile_state->browsers.find(browser);
    if (found != profile_state->browsers.end()) {
      // If we have no browser windows open after erasing this window, then
      // close the ProfileState (and potentially the shim as well).
      profile_state->browsers.erase(found);
      if (profile_state->browsers.empty())
        OnAppDeactivated(browser->profile(), app_id);
      return;
    }
  }
}

void AppShimManager::OnBrowserSetLastActive(Browser* browser) {
  // Rebuild the profile menu items (to ensure that the checkmark in the menu
  // is next to the new-active item).
  if (avatar_menu_)
    avatar_menu_->ActiveBrowserChanged(browser);
  UpdateAllProfileMenus();
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
    mojo_item->icon =
        profiles::GetAvatarIconForNSMenu(item.profile_path).ToImageSkia()[0];
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
      std::move(items),
      base::FeatureList::IsEnabled(features::kNewProfilePicker));
}

AppShimManager::ProfileState* AppShimManager::GetOrCreateProfileState(
    Profile* profile,
    const web_app::AppId& app_id) {
  if (web_app::AppShimLaunchDisabled())
    return nullptr;

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
  return found_profile->second.get();
}

}  // namespace apps
