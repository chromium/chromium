// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/background_sync/background_sync_controller_factory.h"
#include "chrome/browser/background_sync/background_sync_controller_impl.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/client_hints/client_hints_factory.h"
#include "chrome/browser/content_index/content_index_provider_factory.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_builder.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/pref_service_builder_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/startup_data.h"
#include "chrome/browser/storage/storage_notification_service_factory.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/prefs_internals_source.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/metrics/metrics_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/permissions/permission_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/app_mode/app_launch_utils.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/locale_change_guard.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_builder_chromeos.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_builder.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/profile_key_startup_accessor.h"
#else
#include "components/zoom/zoom_event_manager.h"
#include "content/public/common/page_zoom.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/content_settings/content_settings_supervised_provider.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using base::TimeDelta;
using bookmarks::BookmarkModel;
using content::BrowserThread;
using content::CorsOriginPatternSetter;
using content::DownloadManagerDelegate;

namespace {

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
// Delay before we explicitly create the SessionService.
static constexpr TimeDelta kCreateSessionServiceDelay =
    TimeDelta::FromMilliseconds(500);
#endif

// Value written to prefs for EXIT_CRASHED and EXIT_SESSION_ENDED.
const char kPrefExitTypeCrashed[] = "Crashed";
const char kPrefExitTypeSessionEnded[] = "SessionEnded";

// Gets the creation time for |path|, returning base::Time::Now() on failure.
base::Time GetCreationTimeForPath(const base::FilePath& path) {
  base::File::Info info;
  if (base::GetFileInfo(path, &info))
    return info.creation_time;
  return base::Time::Now();
}

// Creates the profile directory synchronously if it doesn't exist. If
// |create_readme| is true, the profile README will be created asynchronously in
// the profile directory. Returns the creation time/date of the profile
// directory.
base::Time CreateProfileDirectory(base::SequencedTaskRunner* io_task_runner,
                                  const base::FilePath& path,
                                  bool create_readme) {
  // Create the profile directory synchronously otherwise we would need to
  // sequence every otherwise independent I/O operation inside the profile
  // directory with this operation. base::PathExists() and
  // base::CreateDirectory() should be lightweight I/O operations and avoiding
  // the headache of sequencing all otherwise unrelated I/O after these
  // justifies running them on the main thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io_to_create_directory;

  // If the readme exists, the profile directory must also already exist.
  if (base::PathExists(path.Append(chrome::kReadmeFilename)))
    return GetCreationTimeForPath(path);

  DVLOG(1) << "Creating directory " << path.value();
  if (base::CreateDirectory(path)) {
    if (create_readme) {
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&CreateProfileReadme, path));
    }
    return GetCreationTimeForPath(path);
  }
  return base::Time::Now();
}

// Converts the kSessionExitedCleanly pref to the corresponding EXIT_TYPE.
Profile::ExitType SessionTypePrefValueToExitType(const std::string& value) {
  if (value == kPrefExitTypeSessionEnded)
    return Profile::EXIT_SESSION_ENDED;
  if (value == kPrefExitTypeCrashed)
    return Profile::EXIT_CRASHED;
  return Profile::EXIT_NORMAL;
}

// Converts an ExitType into a string that is written to prefs.
std::string ExitTypeToSessionTypePrefValue(Profile::ExitType type) {
  switch (type) {
    case Profile::EXIT_NORMAL:
      return ProfileImpl::kPrefExitTypeNormal;
    case Profile::EXIT_SESSION_ENDED:
      return kPrefExitTypeSessionEnded;
    case Profile::EXIT_CRASHED:
      return kPrefExitTypeCrashed;
  }
  NOTREACHED();
  return std::string();
}

#if defined(OS_CHROMEOS)
// Checks if |new_locale| is the same as |pref_locale| or |pref_locale| is used
// to show UI translation for |new_locale|. (e.g. "it" is used for "it-CH")
bool LocaleNotChanged(const std::string& pref_locale,
                      const std::string& new_locale) {
  std::string new_locale_converted = new_locale;
  language::ConvertToActualUILocale(&new_locale_converted);
  return pref_locale == new_locale_converted;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

// static
std::unique_ptr<Profile> Profile::CreateProfile(const base::FilePath& path,
                                                Delegate* delegate,
                                                CreateMode create_mode) {
  TRACE_EVENT1("browser,startup", "Profile::CreateProfile", "profile_path",
               path.AsUTF8Unsafe());

  // Get sequenced task runner for making sure that file operations of
  // this profile are executed in expected order (what was previously assured by
  // the FILE thread).
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  base::Time creation_time = base::Time::Now();
  if (create_mode == CREATE_MODE_ASYNCHRONOUS) {
    DCHECK(delegate);
    creation_time = CreateProfileDirectory(io_task_runner.get(), path, true);
  } else if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    if (base::PathExists(path)) {
      creation_time = GetCreationTimeForPath(path);
    } else {
      // TODO(rogerta): http://crbug/160553 - Bad things happen if we can't
      // write to the profile directory.  We should eventually be able to run in
      // this situation.
      if (!base::CreateDirectory(path))
        return nullptr;

      CreateProfileReadme(path);
    }
  } else {
    NOTREACHED();
  }

  std::unique_ptr<Profile> profile = base::WrapUnique(new ProfileImpl(
      path, delegate, create_mode, creation_time, io_task_runner));
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && !defined(OS_ANDROID) && \
    !defined(OS_CHROMEOS)
  if (create_mode == CREATE_MODE_SYNCHRONOUS && profile->IsLegacySupervised())
    return nullptr;
#endif
  return profile;
}

// static
const char ProfileImpl::kPrefExitTypeNormal[] = "Normal";

// static
void ProfileImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled, false);
  registry->RegisterBooleanPref(prefs::kAllowDeletingBrowserHistory, true);
  registry->RegisterBooleanPref(prefs::kForceGoogleSafeSearch, false);
  registry->RegisterIntegerPref(prefs::kForceYouTubeRestrict,
                                safe_search_util::YOUTUBE_RESTRICT_OFF);
  registry->RegisterStringPref(prefs::kAllowedDomainsForApps, std::string());

  registry->RegisterIntegerPref(prefs::kProfileAvatarIndex, -1);
  // Whether a profile is using an avatar without having explicitely chosen it
  // (i.e. was assigned by default by legacy profile creation).
  registry->RegisterBooleanPref(prefs::kProfileUsingDefaultAvatar, true);
  registry->RegisterBooleanPref(prefs::kProfileUsingGAIAAvatar, false);
  // Whether a profile is using a default avatar name (eg. Pickles or Person 1).
  registry->RegisterBooleanPref(prefs::kProfileUsingDefaultName, true);
  registry->RegisterStringPref(prefs::kProfileName, std::string());

  registry->RegisterStringPref(prefs::kSupervisedUserId, std::string());
#if defined(OS_ANDROID)
  uint32_t home_page_flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  uint32_t home_page_flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
  registry->RegisterStringPref(prefs::kHomePage, std::string(),
                               home_page_flags);
  registry->RegisterStringPref(prefs::kNewTabPageLocationOverride,
                               std::string());

#if BUILDFLAG(ENABLE_PRINTING)
  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);
#endif  // BUILDFLAG(ENABLE_PRINTING)
  registry->RegisterBooleanPref(prefs::kPrintPreviewDisabled, false);
  registry->RegisterStringPref(
      prefs::kPrintPreviewDefaultDestinationSelectionRules, std::string());
#if defined(OS_WIN) && BUILDFLAG(ENABLE_PRINTING)
  registry->RegisterIntegerPref(prefs::kPrintRasterizationMode, 0);
#endif

  registry->RegisterBooleanPref(prefs::kForceEphemeralProfiles, false);
  registry->RegisterBooleanPref(prefs::kEnableMediaRouter, true);
#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(
      prefs::kOobeMarketingOptInScreenFinished, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kShowCastIconInToolbar, false);
#endif  // !defined(OS_ANDROID)
  registry->RegisterTimePref(prefs::kProfileCreationTime, base::Time());
}

ProfileImpl::ProfileImpl(
    const base::FilePath& path,
    Delegate* delegate,
    CreateMode create_mode,
    base::Time path_creation_time,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : path_(path),
      path_creation_time_(path_creation_time),
      io_task_runner_(std::move(io_task_runner)),
      io_data_(this),
      last_session_exit_type_(EXIT_NORMAL),
      start_time_(base::Time::Now()),
      delegate_(delegate),
      shared_cors_origin_access_list_(
          content::SharedCorsOriginAccessList::Create()) {
  TRACE_EVENT0("browser,startup", "ProfileImpl::ctor")
  DCHECK(!path.empty()) << "Using an empty path will attempt to write "
                        << "profile files to the root directory!";

#if defined(OS_CHROMEOS)
  const bool is_regular_profile =
      chromeos::ProfileHelper::IsRegularProfile(this);

  if (is_regular_profile) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(this);
    // A |User| instance should always exist for a profile which is not the
    // initial, the sign-in or the lock screen app profile.
    CHECK(user);
    LOG_IF(FATAL,
           !session_manager::SessionManager::Get()->HasSessionForAccountId(
               user->GetAccountId()))
        << "Attempting to construct the profile before starting the user "
           "session";
  }
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  create_session_service_timer_.Start(
      FROM_HERE, kCreateSessionServiceDelay, this,
      &ProfileImpl::EnsureSessionServiceCreated);
#endif

  set_is_guest_profile(path == ProfileManager::GetGuestProfilePath());
  set_is_system_profile(path == ProfileManager::GetSystemProfilePath());

  // The ProfileImpl can be created both synchronously and asynchronously.
  bool async_prefs = create_mode == CREATE_MODE_ASYNCHRONOUS;

#if defined(OS_ANDROID)
  auto* startup_data = g_browser_process->startup_data();
  DCHECK(startup_data && startup_data->GetProfileKey());
  TakePrefsFromStartupData();
  async_prefs = false;
#else
  LoadPrefsForNormalStartup(async_prefs);
#endif

  // Register on BrowserContext.
  user_prefs::UserPrefs::Set(this, prefs_.get());

#if defined(OS_ANDROID)
  // On Android StartupData creates proto database provider for the profile
  // before profile is created, so move ownership to storage partition.
  GetDefaultStoragePartition(this)->SetProtoDatabaseProvider(
      startup_data->TakeProtoDatabaseProvider());
#endif

  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

#if defined(OS_CHROMEOS)
  if (is_regular_profile) {
    // |chromeos::InitializeAccountManager| is called during a User's session
    // initialization but some tests do not properly login to a User Session.
    // This invocation of |chromeos::InitializeAccountManager| is used only
    // during tests.
    // Note: |chromeos::InitializeAccountManager| is idempotent and safe to call
    // multiple times.
    // TODO(https://crbug.com/982233): Remove this call.
    chromeos::InitializeAccountManager(
        path_, base::DoNothing() /* initialization_callback */);

    chromeos::AccountManager* account_manager =
        g_browser_process->platform_part()
            ->GetAccountManagerFactory()
            ->GetAccountManager(path_.value());
    account_manager->SetPrefService(GetPrefs());
  }
#endif

  if (async_prefs) {
    // Wait for the notification that prefs has been loaded
    // (successfully or not).  Note that we can use base::Unretained
    // because the PrefService is owned by this class and lives on
    // the same thread.
    prefs_->AddPrefInitObserver(base::BindOnce(
        &ProfileImpl::OnPrefsLoaded, base::Unretained(this), create_mode));
  } else {
    // Prefs were loaded synchronously so we can continue directly.
    OnPrefsLoaded(create_mode, true);
  }
}

#if defined(OS_ANDROID)
void ProfileImpl::TakePrefsFromStartupData() {
  auto* startup_data = g_browser_process->startup_data();

  // On Android, it is possible that the ProfileKey has been build before the
  // ProfileImpl is created. The ownership of all these pre-created objects
  // will be taken by ProfileImpl.
  key_ = startup_data->TakeProfileKey();
  prefs_ = startup_data->TakeProfilePrefService();
  schema_registry_service_ = startup_data->TakeSchemaRegistryService();
  user_cloud_policy_manager_ = startup_data->TakeUserCloudPolicyManager();
  profile_policy_connector_ = startup_data->TakeProfilePolicyConnector();
  pref_registry_ = startup_data->TakePrefRegistrySyncable();

  ProfileKeyStartupAccessor::GetInstance()->Reset();
}
#endif

void ProfileImpl::LoadPrefsForNormalStartup(bool async_prefs) {
  key_ = std::make_unique<ProfileKey>(GetPath());
  pref_registry_ = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

  policy::ChromeBrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  schema_registry_service_ = BuildSchemaRegistryServiceForProfile(
      this, connector->GetChromeSchema(), connector->GetSchemaRegistry());

  // If we are creating the profile synchronously, then we should load the
  // policy data immediately.
  bool force_immediate_policy_load = !async_prefs;

  policy::UserCloudPolicyManager* user_cloud_policy_manager;
#if defined(OS_CHROMEOS)
  if (force_immediate_policy_load)
    chromeos::DeviceSettingsService::Get()->LoadImmediately();

  policy::CreateConfigurationPolicyProvider(
      this, force_immediate_policy_load, io_task_runner_,
      &user_cloud_policy_manager_chromeos_, &active_directory_policy_manager_);

  user_cloud_policy_manager = nullptr;
#else
  user_cloud_policy_manager_ = CreateUserCloudPolicyManager(
      GetPath(), GetPolicySchemaRegistryService()->registry(),
      force_immediate_policy_load, io_task_runner_);
  user_cloud_policy_manager = user_cloud_policy_manager_.get();
#endif
  profile_policy_connector_ =
      policy::CreateProfilePolicyConnectorForBrowserContext(
          schema_registry_service_->registry(), user_cloud_policy_manager,
          g_browser_process->browser_policy_connector(),
          force_immediate_policy_load, this);

  bool is_signin_profile = false;
#if defined(OS_CHROMEOS)
  is_signin_profile = chromeos::ProfileHelper::IsSigninProfile(this);
#endif
  ::RegisterProfilePrefs(is_signin_profile,
                         g_browser_process->GetApplicationLocale(),
                         pref_registry_.get());

  mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
      pref_validation_delegate;
  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());
  if (safe_browsing_service.get()) {
    auto pref_validation_delegate_impl =
        safe_browsing_service->CreatePreferenceValidationDelegate(this);
    if (pref_validation_delegate_impl) {
      mojo::MakeSelfOwnedReceiver(
          std::move(pref_validation_delegate_impl),
          pref_validation_delegate.InitWithNewPipeAndPassReceiver());
    }
  }

  prefs_ =
      CreatePrefService(pref_registry_, CreateExtensionPrefStore(this, false),
                        profile_policy_connector_->policy_service(),
                        g_browser_process->browser_policy_connector(),
                        std::move(pref_validation_delegate), GetIOTaskRunner(),
                        key_.get(), path_, async_prefs);
  key_->SetPrefs(prefs_.get());
}

void ProfileImpl::DoFinalInit() {
  TRACE_EVENT0("browser", "ProfileImpl::DoFinalInit")

  PrefService* prefs = GetPrefs();

  // Do not override the existing pref in case a profile directory is copied, or
  // if the file system does not support creation time and the property (i.e.
  // st_ctim in posix which is actually the last status change time when the
  // inode was last updated) use to mimic it changes because of some other
  // modification.
  if (!prefs->HasPrefPath(prefs::kProfileCreationTime))
    prefs->SetTime(prefs::kProfileCreationTime, path_creation_time_);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::Bind(&ProfileImpl::UpdateSupervisedUserIdInStorage,
                 base::Unretained(this)));

  // Changes in the profile avatar.
  pref_change_registrar_.Add(
      prefs::kProfileAvatarIndex,
      base::Bind(&ProfileImpl::UpdateAvatarInStorage, base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileUsingDefaultAvatar,
      base::Bind(&ProfileImpl::UpdateAvatarInStorage, base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileUsingGAIAAvatar,
      base::Bind(&ProfileImpl::UpdateAvatarInStorage, base::Unretained(this)));

  // Changes in the profile name.
  pref_change_registrar_.Add(
      prefs::kProfileUsingDefaultName,
      base::Bind(&ProfileImpl::UpdateNameInStorage, base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileName,
      base::Bind(&ProfileImpl::UpdateNameInStorage, base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kForceEphemeralProfiles,
      base::Bind(&ProfileImpl::UpdateIsEphemeralInStorage,
                 base::Unretained(this)));

  media_device_id_salt_ = new MediaDeviceIDSalt(prefs_.get());

  base::FilePath base_cache_path;
  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the profile directory, which isn't available
  // to PathService.
  chrome::GetUserCacheDirectory(path_, &base_cache_path);
  // Always create the cache directory asynchronously.
  CreateProfileDirectory(io_task_runner_.get(), base_cache_path, false);

  // Initialize components that depend on the current value.
  UpdateSupervisedUserIdInStorage();
  UpdateIsEphemeralInStorage();
  GAIAInfoUpdateServiceFactory::GetForProfile(this);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Initialize the BackgroundModeManager - this has to be done here before
  // InitExtensions() is called because it relies on receiving notifications
  // when extensions are loaded. BackgroundModeManager is not needed under
  // ChromeOS because Chrome is always running, no need for special keep-alive
  // or launch-on-startup support unless kKeepAliveForTest is set.
  bool init_background_mode_manager = true;
#if defined(OS_CHROMEOS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKeepAliveForTest))
    init_background_mode_manager = false;
#endif
  if (init_background_mode_manager) {
    if (g_browser_process->background_mode_manager())
      g_browser_process->background_mode_manager()->RegisterProfile(this);
  }
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterProfile(this);
#endif

  auto* db_provider =
      GetDefaultStoragePartition(this)->GetProtoDatabaseProvider();
  key_->SetProtoDatabaseProvider(db_provider);

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  dom_distiller::RegisterViewerSource(this);

#if defined(OS_CHROMEOS)
  MigrateSigninScopedDeviceId(this);

  if (chromeos::UserSessionManager::GetInstance()
          ->RestartToApplyPerSessionFlagsIfNeed(this, true)) {
    return;
  }
#endif

#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(this);
  model->AddObserver(new BookmarkModelLoadedObserver(this));
#endif

  HeavyAdServiceFactory::GetForBrowserContext(this)->Initialize(GetPath());

  PushMessagingServiceImpl::InitializeForProfile(this);

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  signin_ui_util::InitializePrefsForProfile(this);
#endif

  site_isolation::SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(this);

  InitializeDataReductionProxy();

  content::URLDataSource::Add(this,
                              std::make_unique<PrefsInternalsSource>(this));

  if (delegate_) {
    TRACE_EVENT0("browser", "ProfileImpl::DoFileInit:DelegateOnProfileCreated");
    // Fails if the browser is shutting down. This is done to avoid
    // launching new UI, finalising profile creation, etc. which
    // would trigger a crash down the line. See ...
    const bool shutting_down = g_browser_process->IsShuttingDown();
    delegate_->OnProfileCreated(this, !shutting_down, IsNewProfile());
    // The current Profile may be immediately deleted as part of
    // the call to OnProfileCreated(...) if the initialisation is
    // reported as a failure, thus no code should be executed past
    // that point.
    if (shutting_down)
      return;
  }
  // Ensure that the SharingService is initialized now that io_data_ is
  // initialized. https://crbug.com/171406
  SharingServiceFactory::GetForBrowserContext(this);

  // The creation of FlocIdProvider should align with the start of a browser
  // profile session, so initialize it here.
  federated_learning::FlocIdProviderFactory::GetForProfile(this);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED, content::Source<Profile>(this),
      content::NotificationService::NoDetails());

  AnnouncementNotificationServiceFactory::GetForProfile(this)
      ->MaybeShowNotification();
}

base::FilePath ProfileImpl::last_selected_directory() {
  return GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
}

void ProfileImpl::set_last_selected_directory(const base::FilePath& path) {
  GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory, path);
}

ProfileImpl::~ProfileImpl() {
  MaybeSendDestroyedNotification();

  bool prefs_loaded = prefs_->GetInitializationStatus() !=
                      PrefService::INITIALIZATION_STATUS_WAITING;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  StopCreateSessionServiceTimer();
#endif

  // Remove pref observers
  pref_change_registrar_.RemoveAll();

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterProfile(this);
#endif

  // Destroy all OTR profiles and their profile services first.
  std::vector<Profile*> raw_otr_profiles;
  bool primary_otr_available = false;

  // Get a list of existing OTR profiles since |off_the_record_profile_| might
  // be modified after the call to |DestroyProfileNow|.
  for (auto& otr_profile : otr_profiles_) {
    raw_otr_profiles.push_back(otr_profile.second.get());
    primary_otr_available |= (otr_profile.first == OTRProfileID::PrimaryID());
  }

  for (Profile* otr_profile : raw_otr_profiles)
    ProfileDestroyer::DestroyOffTheRecordProfileNow(otr_profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!primary_otr_available) {
    ExtensionPrefValueMapFactory::GetForBrowserContext(this)
        ->ClearAllIncognitoSessionOnlyPreferences();
  }
#endif

  FullBrowserTransitionManager::Get()->OnProfileDestroyed(this);

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  SimpleKeyMap::GetInstance()->Dissociate(this);

  profile_policy_connector_->Shutdown();
  if (configuration_policy_provider())
    configuration_policy_provider()->Shutdown();

  // This causes the Preferences file to be written to disk.
  if (prefs_loaded)
    SetExitType(EXIT_NORMAL);

  // This must be called before ProfileIOData::ShutdownOnUIThread but after
  // other profile-related destroy notifications are dispatched.
  ShutdownStoragePartitions();
}

std::string ProfileImpl::GetProfileUserName() const {
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(this);
  if (identity_manager)
    return identity_manager->GetPrimaryAccountInfo().email;

  return std::string();
}

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate>
ProfileImpl::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelPrefs>(
      GetPrefs(), GetPath(), partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}
#endif  // !defined(OS_ANDROID)

base::FilePath ProfileImpl::GetPath() {
  return path_;
}

base::FilePath ProfileImpl::GetPath() const {
  return path_;
}

base::Time ProfileImpl::GetCreationTime() const {
  return prefs_->GetTime(prefs::kProfileCreationTime);
}

scoped_refptr<base::SequencedTaskRunner> ProfileImpl::GetIOTaskRunner() {
  return io_task_runner_;
}

bool ProfileImpl::IsOffTheRecord() {
  return false;
}

bool ProfileImpl::IsOffTheRecord() const {
  return false;
}

const Profile::OTRProfileID& ProfileImpl::GetOTRProfileID() const {
  NOTREACHED();
  static base::NoDestructor<OTRProfileID> otr_profile_id(
      "ProfileImp::NoOTRProfileID");
  return *otr_profile_id;
}

Profile* ProfileImpl::GetOffTheRecordProfile(
    const OTRProfileID& otr_profile_id) {
  if (HasOffTheRecordProfile(otr_profile_id))
    return otr_profiles_[otr_profile_id].get();

  // Create a new OffTheRecordProfile
  std::unique_ptr<Profile> otr_profile =
      Profile::CreateOffTheRecordProfile(this, otr_profile_id);
  Profile* raw_otr_profile = otr_profile.get();

  otr_profiles_[otr_profile_id] = std::move(otr_profile);

  NotifyOffTheRecordProfileCreated(raw_otr_profile);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(raw_otr_profile),
      content::NotificationService::NoDetails());

  return raw_otr_profile;
}

std::vector<Profile*> ProfileImpl::GetAllOffTheRecordProfiles() {
  std::vector<Profile*> raw_otr_profiles;
  for (auto& otr : otr_profiles_)
    raw_otr_profiles.push_back(otr.second.get());
  return raw_otr_profiles;
}

void ProfileImpl::DestroyOffTheRecordProfile(Profile* otr_profile) {
  CHECK(otr_profile);
  OTRProfileID profile_id = otr_profile->GetOTRProfileID();
  DCHECK(HasOffTheRecordProfile(profile_id));
  otr_profiles_.erase(profile_id);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions are only supported on primary OTR profile.
  if (profile_id == OTRProfileID::PrimaryID()) {
    ExtensionPrefValueMapFactory::GetForBrowserContext(this)
        ->ClearAllIncognitoSessionOnlyPreferences();
  }
#endif
}

bool ProfileImpl::HasOffTheRecordProfile(const OTRProfileID& otr_profile_id) {
  return base::Contains(otr_profiles_, otr_profile_id);
}

bool ProfileImpl::HasAnyOffTheRecordProfile() {
  return !otr_profiles_.empty();
}

Profile* ProfileImpl::GetOriginalProfile() {
  return this;
}

const Profile* ProfileImpl::GetOriginalProfile() const {
  return this;
}

bool ProfileImpl::IsSupervised() const {
  return !GetPrefs()->GetString(prefs::kSupervisedUserId).empty();
}

bool ProfileImpl::IsChild() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return GetPrefs()->GetString(prefs::kSupervisedUserId) ==
         supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool ProfileImpl::IsLegacySupervised() const {
  return IsSupervised() && !IsChild();
}

bool ProfileImpl::AllowsBrowserWindows() const {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(this)) {
    return false;
  }
#endif
  return !IsSystemProfile();
}

ExtensionSpecialStoragePolicy* ProfileImpl::GetExtensionSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get()) {
    TRACE_EVENT0("browser", "ProfileImpl::GetExtensionSpecialStoragePolicy")
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(
        CookieSettingsFactory::GetForProfile(this).get());
  }
  return extension_special_storage_policy_.get();
#else
  return NULL;
#endif
}

void ProfileImpl::OnLocaleReady() {
  TRACE_EVENT0("browser", "ProfileImpl::OnLocaleReady");

  // Migrate obsolete prefs.
  MigrateObsoleteProfilePrefs(this);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note: Extension preferences can be keyed off the extension ID, so need to
  // be handled specially (rather than directly as part of
  // MigrateObsoleteProfilePrefs()).
  extensions::ExtensionPrefs::Get(this)->MigrateObsoleteExtensionPrefs();
#endif

#if defined(OS_CHROMEOS)
  // If this is a kiosk profile, reset some of its prefs which should not
  // persist between sessions.
  if (chrome::IsRunningInForcedAppMode()) {
    chromeos::ResetEphemeralKioskPreferences(prefs_.get());
  }
#endif

  // |kSessionExitType| was added after |kSessionExitedCleanly|. If the pref
  // value is empty fallback to checking for |kSessionExitedCleanly|.
  const std::string exit_type_pref_value(
      prefs_->GetString(prefs::kSessionExitType));
  if (exit_type_pref_value.empty()) {
    last_session_exit_type_ = prefs_->GetBoolean(prefs::kSessionExitedCleanly)
                                  ? EXIT_NORMAL
                                  : EXIT_CRASHED;
  } else {
    last_session_exit_type_ =
        SessionTypePrefValueToExitType(exit_type_pref_value);
  }
  // Mark the session as open.
  prefs_->SetString(prefs::kSessionExitType, kPrefExitTypeCrashed);
  // Force this to true in case we fallback and use it.
  // TODO(sky): remove this in a couple of releases (m28ish).
  prefs_->SetBoolean(prefs::kSessionExitedCleanly, true);

  g_browser_process->profile_manager()->InitProfileUserPrefs(this);

#if defined(OS_CHROMEOS)
  arc::ArcServiceLauncher::Get()->MaybeSetProfile(this);
#endif

  FullBrowserTransitionManager::Get()->OnProfileCreated(this);

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  ChromeVersionService::OnProfileLoaded(prefs_.get(), IsNewProfile());
  DoFinalInit();
}

void ProfileImpl::OnPrefsLoaded(CreateMode create_mode, bool success) {
  TRACE_EVENT0("browser", "ProfileImpl::OnPrefsLoaded");
  if (!success) {
    if (delegate_)
      delegate_->OnProfileCreated(this, false, false);
    return;
  }

  // Fail fast if the browser is shutting down. We want to avoid launching new
  // UI, finalising profile creation, etc. which would trigger a crash down the
  // the line. See crbug.com/625646
  if (g_browser_process->IsShuttingDown()) {
    if (delegate_)
      delegate_->OnProfileCreated(this, false, false);
    return;
  }

#if defined(OS_CHROMEOS)
  if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    // Synchronous create mode implies that either it is restart after crash,
    // or we are in tests. In both cases the first loaded locale is correct.
    OnLocaleReady();
  } else {
    chromeos::UserSessionManager::GetInstance()->RespectLocalePreferenceWrapper(
        this, base::Bind(&ProfileImpl::OnLocaleReady, base::Unretained(this)));
  }
#else
  OnLocaleReady();
#endif
}

bool ProfileImpl::WasCreatedByVersionOrLater(const std::string& version) {
  base::Version profile_version(ChromeVersionService::GetVersion(prefs_.get()));
  base::Version arg_version(version);
  return (profile_version.CompareTo(arg_version) >= 0);
}

void ProfileImpl::SetExitType(ExitType exit_type) {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this))
    return;
#endif
  if (!prefs_)
    return;
  ExitType current_exit_type = SessionTypePrefValueToExitType(
      prefs_->GetString(prefs::kSessionExitType));
  // This may be invoked multiple times during shutdown. Only persist the value
  // first passed in (unless it's a reset to the crash state, which happens when
  // foregrounding the app on mobile).
  if (exit_type == EXIT_CRASHED || current_exit_type == EXIT_CRASHED) {
    prefs_->SetString(prefs::kSessionExitType,
                      ExitTypeToSessionTypePrefValue(exit_type));
  }
}

Profile::ExitType ProfileImpl::GetLastSessionExitType() const {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exit_type_;
}

bool ProfileImpl::ShouldRestoreOldSessionCookies() const {
#if defined(OS_ANDROID)
  SessionStartupPref::Type startup_pref_type =
      SessionStartupPref::GetDefaultStartupType();
#else
  SessionStartupPref::Type startup_pref_type =
      StartupBrowserCreator::GetSessionStartupPref(
          *base::CommandLine::ForCurrentProcess(), this)
          .type;
#endif
  return GetLastSessionExitType() == Profile::EXIT_CRASHED ||
         startup_pref_type == SessionStartupPref::LAST;
}

bool ProfileImpl::ShouldPersistSessionCookies() const {
  return true;
}

PrefService* ProfileImpl::GetPrefs() {
  return const_cast<PrefService*>(
      static_cast<const ProfileImpl*>(this)->GetPrefs());
}

const PrefService* ProfileImpl::GetPrefs() const {
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

#if !defined(OS_ANDROID)
ChromeZoomLevelPrefs* ProfileImpl::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetDefaultStoragePartition(this)->GetZoomLevelDelegate());
}
#endif  // !defined(OS_ANDROID)

PrefService* ProfileImpl::GetOffTheRecordPrefs() {
  if (HasPrimaryOTRProfile()) {
    return GetPrimaryOTRProfile()->GetPrefs();
  } else {
    // The extensions preference API and many tests call this method even when
    // there's no OTR profile, in order to figure out what a pref value would
    // have been returned if an OTR profile existed. To support that case we
    // return a dummy PrefService here.
    //
    // TODO(crbug.com/734484): Don't call this method when there's no OTR
    // profile (and return null for such calls).
    return GetReadOnlyOffTheRecordPrefs();
  }
}

PrefService* ProfileImpl::GetReadOnlyOffTheRecordPrefs() {
  if (!dummy_otr_prefs_) {
    dummy_otr_prefs_ = CreateIncognitoPrefServiceSyncable(
        prefs_.get(), CreateExtensionPrefStore(this, true));
  }
  return dummy_otr_prefs_.get();
}

policy::SchemaRegistryService* ProfileImpl::GetPolicySchemaRegistryService() {
  return schema_registry_service_.get();
}

#if defined(OS_CHROMEOS)
policy::UserCloudPolicyManagerChromeOS*
ProfileImpl::GetUserCloudPolicyManagerChromeOS() {
  return user_cloud_policy_manager_chromeos_.get();
}

policy::ActiveDirectoryPolicyManager*
ProfileImpl::GetActiveDirectoryPolicyManager() {
  return active_directory_policy_manager_.get();
}
#else
policy::UserCloudPolicyManager* ProfileImpl::GetUserCloudPolicyManager() {
  return user_cloud_policy_manager_.get();
}
#endif  // defined(OS_CHROMEOS)

policy::ConfigurationPolicyProvider*
ProfileImpl::configuration_policy_provider() {
#if defined(OS_CHROMEOS)
  if (user_cloud_policy_manager_chromeos_)
    return user_cloud_policy_manager_chromeos_.get();
  if (active_directory_policy_manager_)
    return active_directory_policy_manager_.get();
  return nullptr;
#else
  return user_cloud_policy_manager_.get();
#endif
}

policy::ProfilePolicyConnector* ProfileImpl::GetProfilePolicyConnector() {
  return profile_policy_connector_.get();
}

const policy::ProfilePolicyConnector* ProfileImpl::GetProfilePolicyConnector()
    const {
  return profile_policy_connector_.get();
}

content::ResourceContext* ProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
ProfileImpl::GetURLLoaderFactory() {
  return GetDefaultStoragePartition(this)
      ->GetURLLoaderFactoryForBrowserProcess();
}

content::BrowserPluginGuestManager* ProfileImpl::GetGuestManager() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

DownloadManagerDelegate* ProfileImpl::GetDownloadManagerDelegate() {
  return DownloadCoreServiceFactory::GetForBrowserContext(this)
      ->GetDownloadManagerDelegate();
}

storage::SpecialStoragePolicy* ProfileImpl::GetSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::PushMessagingService* ProfileImpl::GetPushMessagingService() {
  return PushMessagingServiceFactory::GetForProfile(this);
}

content::StorageNotificationService*
ProfileImpl::GetStorageNotificationService() {
#if defined(OS_ANDROID)
  return nullptr;
#else
  return StorageNotificationServiceFactory::GetForBrowserContext(this);
#endif
}

content::SSLHostStateDelegate* ProfileImpl::GetSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForProfile(this);
}

content::BrowsingDataRemoverDelegate*
ProfileImpl::GetBrowsingDataRemoverDelegate() {
  return ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(this);
}

// TODO(mlamouri): we should all these BrowserContext implementation to Profile
// instead of repeating them inside all Profile implementations.
content::PermissionControllerDelegate*
ProfileImpl::GetPermissionControllerDelegate() {
  return PermissionManagerFactory::GetForProfile(this);
}

content::ClientHintsControllerDelegate*
ProfileImpl::GetClientHintsControllerDelegate() {
  return ClientHintsFactory::GetForBrowserContext(this);
}

content::BackgroundFetchDelegate* ProfileImpl::GetBackgroundFetchDelegate() {
  return BackgroundFetchDelegateFactory::GetForProfile(this);
}

content::BackgroundSyncController* ProfileImpl::GetBackgroundSyncController() {
  return BackgroundSyncControllerFactory::GetForProfile(this);
}

content::ContentIndexProvider* ProfileImpl::GetContentIndexProvider() {
  return ContentIndexProviderFactory::GetForProfile(this);
}

void ProfileImpl::SetCorsOriginAccessListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  auto barrier_closure = BarrierClosure(3, std::move(closure));

  // Keep profile storage partitions' NetworkContexts synchronized.
  auto profile_setter = base::MakeRefCounted<CorsOriginPatternSetter>(
      source_origin, CorsOriginPatternSetter::ClonePatterns(allow_patterns),
      CorsOriginPatternSetter::ClonePatterns(block_patterns), barrier_closure);
  ForEachStoragePartition(
      this, base::BindRepeating(&CorsOriginPatternSetter::SetLists,
                                base::RetainedRef(profile_setter.get())));

  // Keep incognito storage partitions' NetworkContexts synchronized.
  if (HasPrimaryOTRProfile()) {
    auto off_the_record_setter = base::MakeRefCounted<CorsOriginPatternSetter>(
        source_origin, CorsOriginPatternSetter::ClonePatterns(allow_patterns),
        CorsOriginPatternSetter::ClonePatterns(block_patterns),
        barrier_closure);
    ForEachStoragePartition(
        GetPrimaryOTRProfile(),
        base::BindRepeating(&CorsOriginPatternSetter::SetLists,
                            base::RetainedRef(off_the_record_setter.get())));
  } else {
    // Release unused closure reference.
    barrier_closure.Run();
  }

  // Keep the per-profile access list up to date so that we can use this to
  // restore NetworkContext settings at anytime, e.g. on restarting the
  // network service.
  shared_cors_origin_access_list_->SetForOrigin(
      source_origin, std::move(allow_patterns), std::move(block_patterns),
      barrier_closure);
}

content::SharedCorsOriginAccessList*
ProfileImpl::GetSharedCorsOriginAccessList() {
  return shared_cors_origin_access_list_.get();
}

std::string ProfileImpl::GetMediaDeviceIDSalt() {
  return media_device_id_salt_->GetSalt();
}

download::InProgressDownloadManager*
ProfileImpl::RetriveInProgressDownloadManager() {
  return DownloadManagerUtils::RetrieveInProgressDownloadManager(this);
}

content::NativeFileSystemPermissionContext*
ProfileImpl::GetNativeFileSystemPermissionContext() {
  return NativeFileSystemPermissionContextFactory::GetForProfile(this);
}

bool ProfileImpl::IsSameOrParent(Profile* profile) {
  return profile && profile->GetOriginalProfile() == this;
}

base::Time ProfileImpl::GetStartTime() const {
  return start_time_;
}

ProfileKey* ProfileImpl::GetProfileKey() const {
  DCHECK(key_);
  return key_.get();
}

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
void ProfileImpl::StopCreateSessionServiceTimer() {
  create_session_service_timer_.Stop();
}

void ProfileImpl::EnsureSessionServiceCreated() {
  SessionServiceFactory::GetForProfile(this);
}
#endif

#if defined(OS_CHROMEOS)
void ProfileImpl::ChangeAppLocale(const std::string& new_locale,
                                  AppLocaleChangedVia via) {
  if (new_locale.empty()) {
    NOTREACHED();
    return;
  }
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  if (local_state->IsManagedPreference(language::prefs::kApplicationLocale))
    return;
  std::string pref_locale =
      GetPrefs()->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&pref_locale);
  bool do_update_pref = true;
  switch (via) {
    case APP_LOCALE_CHANGED_VIA_SETTINGS:
    case APP_LOCALE_CHANGED_VIA_REVERT: {
      // We keep kApplicationLocaleBackup value as a reference.  In case value
      // of kApplicationLocale preference would change due to sync from other
      // device then kApplicationLocaleBackup value will trigger and allow us to
      // show notification about automatic locale change in LocaleChangeGuard.
      GetPrefs()->SetString(prefs::kApplicationLocaleBackup, new_locale);
      GetPrefs()->ClearPref(prefs::kApplicationLocaleAccepted);
      // We maintain kApplicationLocale property in both a global storage
      // and user's profile.  Global property determines locale of login screen,
      // while user's profile determines their personal locale preference.
      break;
    }
    case APP_LOCALE_CHANGED_VIA_LOGIN:
    case APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN: {
      if (!pref_locale.empty()) {
        DCHECK(LocaleNotChanged(pref_locale, new_locale));

        if (!locale_change_guard_) {
          locale_change_guard_ =
              std::make_unique<chromeos::LocaleChangeGuard>(this);
        }
        locale_change_guard_->set_locale_changed_during_login(true);

        std::string accepted_locale =
            GetPrefs()->GetString(prefs::kApplicationLocaleAccepted);
        if (accepted_locale == new_locale) {
          // If locale is accepted then we do not want to show LocaleChange
          // notification.  This notification is triggered by different values
          // of kApplicationLocaleBackup and kApplicationLocale preferences,
          // so make them identical.
          GetPrefs()->SetString(prefs::kApplicationLocaleBackup, new_locale);
        } else {
          // Back up locale of login screen.
          std::string cur_locale = g_browser_process->GetApplicationLocale();
          GetPrefs()->SetString(prefs::kApplicationLocaleBackup, cur_locale);
          locale_change_guard_->PrepareChangingLocale(cur_locale, new_locale);
        }
      } else {
        std::string cur_locale = g_browser_process->GetApplicationLocale();
        std::string backup_locale =
            GetPrefs()->GetString(prefs::kApplicationLocaleBackup);
        // Profile synchronization takes time and is not completed at that
        // moment at first login.  So we initialize locale preference in steps:
        // (1) first save it to temporary backup;
        // (2) on next login we assume that synchronization is already completed
        //     and we may finalize initialization.
        GetPrefs()->SetString(prefs::kApplicationLocaleBackup, cur_locale);
        if (!new_locale.empty())
          GetPrefs()->SetString(language::prefs::kApplicationLocale,
                                new_locale);
        else if (!backup_locale.empty())
          GetPrefs()->SetString(language::prefs::kApplicationLocale,
                                backup_locale);
        do_update_pref = false;
      }
      break;
    }
    case APP_LOCALE_CHANGED_VIA_POLICY: {
      // If the locale change has been triggered by policy, the original locale
      // is not allowed and can't be switched back to.
      GetPrefs()->SetString(prefs::kApplicationLocaleBackup, new_locale);
      break;
    }
    case APP_LOCALE_CHANGED_VIA_UNKNOWN:
    default: {
      NOTREACHED();
      break;
    }
  }
  if (do_update_pref)
    GetPrefs()->SetString(language::prefs::kApplicationLocale, new_locale);
  if (via != APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN)
    local_state->SetString(language::prefs::kApplicationLocale, new_locale);

  if (user_manager::UserManager::Get()->GetOwnerAccountId() ==
      chromeos::ProfileHelper::Get()->GetUserByProfile(this)->GetAccountId())
    local_state->SetString(prefs::kOwnerLocale, new_locale);
}

void ProfileImpl::OnLogin() {
  if (!locale_change_guard_)
    locale_change_guard_ = std::make_unique<chromeos::LocaleChangeGuard>(this);
  locale_change_guard_->OnLogin();
}

void ProfileImpl::InitChromeOSPreferences() {
  chromeos_preferences_.reset(new chromeos::Preferences());
  chromeos_preferences_->Init(
      this, chromeos::ProfileHelper::Get()->GetUserByProfile(this));
}

#endif  // defined(OS_CHROMEOS)

void ProfileImpl::SetCreationTimeForTesting(base::Time creation_time) {
  prefs_->SetTime(prefs::kProfileCreationTime, creation_time);
}

GURL ProfileImpl::GetHomePage() {
  // --homepage overrides any preferences.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHomePage)) {
    // TODO(evanm): clean up usage of DIR_CURRENT.
    //   http://code.google.com/p/chromium/issues/detail?id=60630
    // For now, allow this code to call getcwd().
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    base::FilePath browser_directory;
    base::PathService::Get(base::DIR_CURRENT, &browser_directory);
    GURL home_page(url_formatter::FixupRelativeFile(
        browser_directory,
        command_line.GetSwitchValuePath(switches::kHomePage)));
    if (home_page.is_valid())
      return home_page;
  }

  if (GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    return GURL(chrome::kChromeUINewTabURL);
  GURL home_page(url_formatter::FixupURL(
      GetPrefs()->GetString(prefs::kHomePage), std::string()));
  if (!home_page.is_valid())
    return GURL(chrome::kChromeUINewTabURL);
  return home_page;
}

void ProfileImpl::UpdateSupervisedUserIdInStorage() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry;
  bool has_entry = profile_manager->GetProfileAttributesStorage()
                       .GetProfileAttributesWithPath(GetPath(), &entry);
  if (has_entry)
    entry->SetSupervisedUserId(GetPrefs()->GetString(prefs::kSupervisedUserId));
}

void ProfileImpl::UpdateNameInStorage() {
  ProfileAttributesEntry* entry;
  bool has_entry = g_browser_process->profile_manager()
                       ->GetProfileAttributesStorage()
                       .GetProfileAttributesWithPath(GetPath(), &entry);
  if (has_entry) {
    entry->SetLocalProfileName(
        base::UTF8ToUTF16(GetPrefs()->GetString(prefs::kProfileName)));
    entry->SetIsUsingDefaultName(
        GetPrefs()->GetBoolean(prefs::kProfileUsingDefaultName));
  }
}

void ProfileImpl::UpdateAvatarInStorage() {
  ProfileAttributesEntry* entry;
  bool has_entry = g_browser_process->profile_manager()
                       ->GetProfileAttributesStorage()
                       .GetProfileAttributesWithPath(GetPath(), &entry);
  if (has_entry) {
    entry->SetAvatarIconIndex(
        GetPrefs()->GetInteger(prefs::kProfileAvatarIndex));
    entry->SetIsUsingDefaultAvatar(
        GetPrefs()->GetBoolean(prefs::kProfileUsingDefaultAvatar));
    entry->SetIsUsingGAIAPicture(
        GetPrefs()->GetBoolean(prefs::kProfileUsingGAIAAvatar));
  }
}

void ProfileImpl::UpdateIsEphemeralInStorage() {
  ProfileAttributesEntry* entry;
  bool has_entry = g_browser_process->profile_manager()
                       ->GetProfileAttributesStorage()
                       .GetProfileAttributesWithPath(GetPath(), &entry);
  if (has_entry) {
    entry->SetIsEphemeral(
        GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles));
  }
}

void ProfileImpl::InitializeDataReductionProxy() {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  std::unique_ptr<data_reduction_proxy::DataStore> store(
      new data_reduction_proxy::DataStoreImpl(GetPath()));
  DataReductionProxyChromeSettingsFactory::GetForBrowserContext(this)
      ->InitDataReductionProxySettings(this, std::move(store), db_task_runner);
}
