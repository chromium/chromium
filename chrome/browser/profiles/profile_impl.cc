// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
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
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_service.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_service_factory.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/in_process_service_factory_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/prefs_internals_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
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
#include "components/domain_reliability/monitor.h"
#include "components/domain_reliability/service.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/metrics/metrics_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/identity/identity_service.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/features.h"
#include "services/preferences/public/cpp/in_process_service_factory.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/authpolicy/auth_policy_credentials_manager.h"
#include "chrome/browser/chromeos/cryptauth/gcm_device_info_provider_impl.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/locale_change_guard.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/multidevice_setup/android_sms_app_helper_delegate_impl.h"
#include "chrome/browser/chromeos/multidevice_setup/android_sms_pairing_state_tracker_impl.h"
#include "chrome/browser/chromeos/multidevice_setup/auth_token_validator_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/auth_token_validator_impl.h"
#include "chrome/browser/chromeos/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chromeos/account_manager/account_manager.h"
#include "chromeos/account_manager/account_manager_factory.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/services/device_sync/device_sync_service.h"
#include "chromeos/services/device_sync/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/service.h"
#include "content/public/browser/network_service_instance.h"
#endif

#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_manager_service.h"
#else
#include "chrome/browser/apps/foundation/app_service/app_service.h"
#include "chrome/browser/apps/foundation/app_service/public/mojom/constants.mojom.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/common/page_zoom.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_system.h"
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
using content::DownloadManagerDelegate;

namespace {

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
// Delay, in milliseconds, before we explicitly create the SessionService.
const int kCreateSessionServiceDelayMS = 500;
#endif

// Text content of README file created in each profile directory. Both %s
// placeholders must contain the product name. This is not localizable and hence
// not in resources.
const char kReadmeText[] =
    "%s settings and storage represent user-selected preferences and "
    "information and MUST not be extracted, overwritten or modified except "
    "through %s defined APIs.";

// Value written to prefs for EXIT_CRASHED and EXIT_SESSION_ENDED.
const char kPrefExitTypeCrashed[] = "Crashed";
const char kPrefExitTypeSessionEnded[] = "SessionEnded";

void CreateProfileReadme(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  base::FilePath readme_path = profile_path.Append(chrome::kReadmeFilename);
  std::string product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string readme_text = base::StringPrintf(
      kReadmeText, product_name.c_str(), product_name.c_str());
  if (base::WriteFile(readme_path, readme_text.data(), readme_text.size()) ==
      -1) {
    LOG(ERROR) << "Could not create README file.";
  }
}

// Creates the profile directory synchronously if it doesn't exist. If
// |create_readme| is true, the profile README will be created asynchronously in
// the profile directory.
void CreateProfileDirectory(base::SequencedTaskRunner* io_task_runner,
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
    return;

  DVLOG(1) << "Creating directory " << path.value();
  if (base::CreateDirectory(path) && create_readme) {
    base::PostTaskWithTraits(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                             base::Bind(&CreateProfileReadme, path));
  }
}

base::FilePath GetMediaCachePath(const base::FilePath& base) {
  return base.Append(chrome::kMediaCacheDirname);
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

#if !defined(OS_ANDROID)
std::unique_ptr<service_manager::Service> CreateAppService(Profile* profile) {
  // TODO(crbug.com/826982): use |profile| to fetch existing registries.
  return std::make_unique<apps::AppService>();
}
#endif

}  // namespace

// static
Profile* Profile::CreateProfile(const base::FilePath& path,
                                Delegate* delegate,
                                CreateMode create_mode) {
  TRACE_EVENT1("browser,startup", "Profile::CreateProfile", "profile_path",
               path.AsUTF8Unsafe());

  // Get sequenced task runner for making sure that file operations of
  // this profile are executed in expected order (what was previously assured by
  // the FILE thread).
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  if (create_mode == CREATE_MODE_ASYNCHRONOUS) {
    DCHECK(delegate);
    CreateProfileDirectory(io_task_runner.get(), path, true);
  } else if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    if (!base::PathExists(path)) {
      // TODO(rogerta): http://crbug/160553 - Bad things happen if we can't
      // write to the profile directory.  We should eventually be able to run in
      // this situation.
      if (!base::CreateDirectory(path))
        return NULL;

      CreateProfileReadme(path);
    }
  } else {
    NOTREACHED();
  }

  auto profile = base::WrapUnique(
      new ProfileImpl(path, delegate, create_mode, io_task_runner));
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && !defined(OS_ANDROID) && \
    !defined(OS_CHROMEOS)
  if (create_mode == CREATE_MODE_SYNCHRONOUS && profile->IsLegacySupervised())
    return nullptr;
#endif
  return profile.release();
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
  registry->RegisterBooleanPref(prefs::kForceSessionSync, false);
  registry->RegisterStringPref(prefs::kAllowedDomainsForApps, std::string());

#if defined(OS_ANDROID)
  // The following prefs don't need to be sync'd to mobile. This file isn't
  // compiled on iOS so we only need to exclude them syncing from the Android
  // build.
  registry->RegisterIntegerPref(prefs::kProfileAvatarIndex, -1);
  // Whether a profile is using an avatar without having explicitely chosen it
  // (i.e. was assigned by default by legacy profile creation).
  registry->RegisterBooleanPref(prefs::kProfileUsingDefaultAvatar, true);
  registry->RegisterBooleanPref(prefs::kProfileUsingGAIAAvatar, false);
  // Whether a profile is using a default avatar name (eg. Pickles or Person 1).
  registry->RegisterBooleanPref(prefs::kProfileUsingDefaultName, true);
  registry->RegisterStringPref(prefs::kProfileName, std::string());
#else
  registry->RegisterIntegerPref(
      prefs::kProfileAvatarIndex, -1,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // Whether a profile is using an avatar without having explicitely chosen it
  // (i.e. was assigned by default by legacy profile creation).
  registry->RegisterBooleanPref(
      prefs::kProfileUsingDefaultAvatar, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kProfileUsingGAIAAvatar, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // Whether a profile is using a default avatar name (eg. Pickles or Person 1).
  registry->RegisterBooleanPref(
      prefs::kProfileUsingDefaultName, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kProfileName, std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
  registry->RegisterIntegerPref(prefs::kProfileLocalAvatarIndex, -1);

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
#if defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(prefs::kPrintingAllowedColorModes, 0);
  registry->RegisterIntegerPref(prefs::kPrintingAllowedDuplexModes, 0);
  registry->RegisterListPref(prefs::kPrintingAllowedPageSizes);
  registry->RegisterIntegerPref(prefs::kPrintingColorDefault, 0);
  registry->RegisterIntegerPref(prefs::kPrintingDuplexDefault, 0);
  registry->RegisterDictionaryPref(prefs::kPrintingSizeDefault);
  registry->RegisterBooleanPref(
      prefs::kOobeMarketingOptInScreenFinished, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // defined(OS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_PRINTING)
  registry->RegisterBooleanPref(prefs::kPrintPreviewDisabled, false);
  registry->RegisterStringPref(
      prefs::kPrintPreviewDefaultDestinationSelectionRules, std::string());
  registry->RegisterBooleanPref(prefs::kForceEphemeralProfiles, false);
  registry->RegisterBooleanPref(prefs::kEnableMediaRouter, true);
#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kShowCastIconInToolbar, false);
#endif  // !defined(OS_ANDROID)
  // Initialize the cache prefs.
  registry->RegisterFilePathPref(prefs::kDiskCacheDir, base::FilePath());
  registry->RegisterIntegerPref(prefs::kDiskCacheSize, 0);
  registry->RegisterIntegerPref(prefs::kMediaCacheSize, 0);
}

ProfileImpl::ProfileImpl(
    const base::FilePath& path,
    Delegate* delegate,
    CreateMode create_mode,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : path_(path),
      io_task_runner_(std::move(io_task_runner)),
      pref_registry_(new user_prefs::PrefRegistrySyncable),
      io_data_(this),
      last_session_exit_type_(EXIT_NORMAL),
      start_time_(base::Time::Now()),
      delegate_(delegate),
      reporting_permissions_checker_factory_(this) {
  TRACE_EVENT0("browser,startup", "ProfileImpl::ctor")
  DCHECK(!path.empty()) << "Using an empty path will attempt to write "
                        << "profile files to the root directory!";

#if defined(OS_CHROMEOS)
  if (!chromeos::ProfileHelper::IsSigninProfile(this) &&
      !chromeos::ProfileHelper::IsLockScreenAppProfile(this)) {
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
    chromeos::AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    chromeos::AccountManager* account_manager =
        factory->GetAccountManager(path.value());
    account_manager->Initialize(
        path,
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory(),
        base::BindRepeating(&chromeos::DelayNetworkCall,
                            base::TimeDelta::FromMilliseconds(
                                chromeos::kDefaultNetworkRetryDelayMS)));
  }
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  create_session_service_timer_.Start(
      FROM_HERE, TimeDelta::FromMilliseconds(kCreateSessionServiceDelayMS),
      this, &ProfileImpl::EnsureSessionServiceCreated);
#endif

  set_is_guest_profile(path == ProfileManager::GetGuestProfilePath());
  set_is_system_profile(path == ProfileManager::GetSystemProfilePath());

  // If we are creating the profile synchronously, then we should load the
  // policy data immediately.
  bool force_immediate_policy_load = (create_mode == CREATE_MODE_SYNCHRONOUS);
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  schema_registry_service_ =
      policy::SchemaRegistryServiceFactory::CreateForContext(
          this, connector->GetChromeSchema(), connector->GetSchemaRegistry());
#if defined(OS_CHROMEOS)
  if (force_immediate_policy_load)
    chromeos::DeviceSettingsService::Get()->LoadImmediately();
  configuration_policy_provider_ =
      policy::UserPolicyManagerFactoryChromeOS::CreateForProfile(
          this, force_immediate_policy_load, io_task_runner_);
#else
  configuration_policy_provider_ =
      policy::UserCloudPolicyManagerFactory::CreateForOriginalBrowserContext(
          this, force_immediate_policy_load, io_task_runner_);
#endif
  profile_policy_connector_ =
      policy::ProfilePolicyConnectorFactory::CreateForBrowserContext(
          this, force_immediate_policy_load);

  DCHECK(create_mode == CREATE_MODE_ASYNCHRONOUS ||
         create_mode == CREATE_MODE_SYNCHRONOUS);
  bool async_prefs = create_mode == CREATE_MODE_ASYNCHRONOUS;

#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this))
    RegisterLoginProfilePrefs(pref_registry_.get());
  else
#endif
    RegisterUserProfilePrefs(pref_registry_.get());

  BrowserContextDependencyManager::GetInstance()
      ->RegisterProfilePrefsForServices(this, pref_registry_.get());

  SupervisedUserSettingsService* supervised_user_settings = nullptr;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(this);
  supervised_user_settings->Init(path_, io_task_runner_.get(),
                                 create_mode == CREATE_MODE_SYNCHRONOUS);
#endif

  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());
  prefs::mojom::TrackedPreferenceValidationDelegatePtr pref_validation_delegate;
  if (safe_browsing_service.get()) {
    auto pref_validation_delegate_impl =
        safe_browsing_service->CreatePreferenceValidationDelegate(this);
    if (pref_validation_delegate_impl) {
      mojo::MakeStrongBinding(std::move(pref_validation_delegate_impl),
                              mojo::MakeRequest(&pref_validation_delegate));
    }
  }

  content::BrowserContext::Initialize(this, path_);

  {
    auto delegate =
        InProcessPrefServiceFactoryFactory::GetInstanceForContext(this)
            ->CreateDelegate();
    delegate->InitPrefRegistry(pref_registry_.get());
    prefs_ = chrome_prefs::CreateProfilePrefs(
        path_, std::move(pref_validation_delegate),
        profile_policy_connector_->policy_service(), supervised_user_settings,
        CreateExtensionPrefStore(this, false), pref_registry_, async_prefs,
        GetIOTaskRunner(), std::move(delegate));
    // Register on BrowserContext.
    user_prefs::UserPrefs::Set(this, prefs_.get());
  }

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

void ProfileImpl::DoFinalInit() {
  TRACE_EVENT0("browser", "ProfileImpl::DoFinalInit")
  SCOPED_UMA_HISTOGRAM_TIMER("Profile.ProfileImplDoFinalInit");

  PrefService* prefs = GetPrefs();
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

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the profile directory, which isn't available
  // to PathService.
  chrome::GetUserCacheDirectory(path_, &base_cache_path_);
  // Always create the cache directory asynchronously.
  CreateProfileDirectory(io_task_runner_.get(), base_cache_path_, false);

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

  base::FilePath media_cache_path = base_cache_path_;
  int media_cache_max_size;
  GetMediaCacheParameters(&media_cache_path, &media_cache_max_size);
  media_cache_path = GetMediaCachePath(media_cache_path);

  base::FilePath extensions_cookie_path = GetPath();
  extensions_cookie_path =
      extensions_cookie_path.Append(chrome::kExtensionsCookieFilename);

  // Make sure we initialize the ProfileIOData after everything else has been
  // initialized that we might be reading from the IO thread.

  PrefService* local_state = g_browser_process->local_state();
  io_data_.Init(media_cache_path, media_cache_max_size, extensions_cookie_path,
                GetPath(), GetSpecialStoragePolicy(),
                reporting_permissions_checker_factory_.CreateChecker(),
                CreateDomainReliabilityMonitor(local_state));

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      this, io_data_.GetResourceContextNoInit());
#endif

  TRACE_EVENT0("browser", "ProfileImpl::SetSaveSessionStorageOnDisk");
  content::BrowserContext::GetDefaultStoragePartition(this)
      ->GetDOMStorageContext()
      ->SetSaveSessionStorageOnDisk();

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  dom_distiller::RegisterViewerSource(this);

#if defined(OS_CHROMEOS)
  // Finished profile initialization - let the UserManager know so it can
  // mark the session as initialized. Need to do this before we restart below
  // so we don't get in a weird state where we restart before the session is
  // marked as initialized and so try to initialize it again.
  if (!chromeos::ProfileHelper::IsSigninProfile(this) &&
      !chromeos::ProfileHelper::IsLockScreenAppProfile(this)) {
    chromeos::ProfileHelper* profile_helper = chromeos::ProfileHelper::Get();
    user_manager::UserManager::Get()->OnProfileInitialized(
        profile_helper->GetUserByProfile(this));
  }

  MigrateSigninScopedDeviceId(this);

  if (chromeos::UserSessionManager::GetInstance()
          ->RestartToApplyPerSessionFlagsIfNeed(this, true)) {
    return;
  }
#endif

  if (delegate_) {
    TRACE_EVENT0("browser", "ProfileImpl::DoFileInit:DelegateOnProfileCreated")
    delegate_->OnProfileCreated(this, true, IsNewProfile());
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("Profile.NotifyProfileCreatedTime");
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_CREATED, content::Source<Profile>(this),
        content::NotificationService::NoDetails());
  }
#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(this);
  model->AddObserver(new BookmarkModelLoadedObserver(this));
#endif

  PageLoadCappingServiceFactory::GetForBrowserContext(this)->Initialize(
      GetPath());

  PushMessagingServiceImpl::InitializeForProfile(this);

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  signin_ui_util::InitializePrefsForProfile(this);
#endif

  content::URLDataSource::Add(this,
                              std::make_unique<PrefsInternalsSource>(this));
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
  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      io_data_.GetResourceContextNoInit());
#endif

  // Destroy OTR profile and its profile services first.
  if (off_the_record_profile_) {
    ProfileDestroyer::DestroyOffTheRecordProfileNow(
        off_the_record_profile_.get());
  } else {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    ExtensionPrefValueMapFactory::GetForBrowserContext(this)
        ->ClearAllIncognitoSessionOnlyPreferences();
#endif
  }

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

  // This causes the Preferences file to be written to disk.
  if (prefs_loaded)
    SetExitType(EXIT_NORMAL);

  // This must be called before ProfileIOData::ShutdownOnUIThread but after
  // other profile-related destroy notifications are dispatched.
  ShutdownStoragePartitions();
}

std::string ProfileImpl::GetProfileUserName() const {
  const identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(this);
  if (identity_manager)
    return identity_manager->GetPrimaryAccountInfo().email;

  return std::string();
}

Profile::ProfileType ProfileImpl::GetProfileType() const {
  return REGULAR_PROFILE;
}

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate>
ProfileImpl::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelPrefs>(
      GetPrefs(), GetPath(), partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}
#endif  // !defined(OS_ANDROID)

base::FilePath ProfileImpl::GetPath() const {
  return path_;
}

base::FilePath ProfileImpl::GetCachePath() const {
  return base_cache_path_;
}

scoped_refptr<base::SequencedTaskRunner> ProfileImpl::GetIOTaskRunner() {
  return io_task_runner_;
}

bool ProfileImpl::IsOffTheRecord() const {
  return false;
}

Profile* ProfileImpl::GetOffTheRecordProfile() {
  if (!off_the_record_profile_) {
    std::unique_ptr<Profile> p(CreateOffTheRecordProfile());
    off_the_record_profile_.swap(p);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::Source<Profile>(off_the_record_profile_.get()),
        content::NotificationService::NoDetails());
  }
  return off_the_record_profile_.get();
}

void ProfileImpl::DestroyOffTheRecordProfile() {
  off_the_record_profile_.reset();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionPrefValueMapFactory::GetForBrowserContext(this)
      ->ClearAllIncognitoSessionOnlyPreferences();
#endif
}

bool ProfileImpl::HasOffTheRecordProfile() {
  return off_the_record_profile_.get() != NULL;
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
  SCOPED_UMA_HISTOGRAM_TIMER("Profile.OnLocaleReadyTime");
  // Migrate obsolete prefs.
  if (g_browser_process->local_state())
    MigrateObsoleteBrowserPrefs(this, g_browser_process->local_state());
  MigrateObsoleteProfilePrefs(this);

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

  {
    SCOPED_UMA_HISTOGRAM_TIMER("Profile.CreateBrowserContextServicesTime");
    BrowserContextDependencyManager::GetInstance()
        ->CreateBrowserContextServices(this);
  }

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

Profile::ExitType ProfileImpl::GetLastSessionExitType() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exit_type_;
}

bool ProfileImpl::ShouldRestoreOldSessionCookies() {
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

bool ProfileImpl::ShouldPersistSessionCookies() {
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
  if (HasOffTheRecordProfile()) {
    return GetOffTheRecordProfile()->GetPrefs();
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
        prefs_.get(), CreateExtensionPrefStore(this, true), nullptr);
  }
  return dummy_otr_prefs_.get();
}

content::ResourceContext* ProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

base::OnceCallback<net::CookieStore*()>
ProfileImpl::GetExtensionsCookieStoreGetter() {
  return base::BindOnce(
      [](content::ResourceContext* context) {
        auto* io_data = ProfileIOData::FromResourceContext(context);
        return io_data->GetExtensionsCookieStore();
      },
      GetResourceContext());
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

content::SSLHostStateDelegate* ProfileImpl::GetSSLHostStateDelegate() {
  return ChromeSSLHostStateDelegateFactory::GetForProfile(this);
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

content::BackgroundFetchDelegate* ProfileImpl::GetBackgroundFetchDelegate() {
  return BackgroundFetchDelegateFactory::GetForProfile(this);
}

content::BackgroundSyncController* ProfileImpl::GetBackgroundSyncController() {
  return BackgroundSyncControllerFactory::GetForProfile(this);
}

net::URLRequestContextGetter* ProfileImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_
      .CreateMainRequestContextGetter(protocol_handlers,
                                      std::move(request_interceptors),
                                      g_browser_process->io_thread())
      .get();
}

net::URLRequestContextGetter*
ProfileImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_
      .CreateIsolatedAppRequestContextGetter(partition_path, in_memory,
                                             protocol_handlers,
                                             std::move(request_interceptors))
      .get();
}

net::URLRequestContextGetter* ProfileImpl::CreateMediaRequestContext() {
  return io_data_.GetMediaRequestContextGetter().get();
}

net::URLRequestContextGetter*
ProfileImpl::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return io_data_
      .GetIsolatedMediaRequestContextGetter(partition_path, in_memory)
      .get();
}

void ProfileImpl::RegisterInProcessServices(StaticServiceMap* services) {
  {
    service_manager::EmbeddedServiceInfo info;
    info.factory =
        InProcessPrefServiceFactoryFactory::GetInstanceForContext(this)
            ->CreatePrefServiceFactory();
    info.task_runner = base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::UI});
    services->insert(std::make_pair(prefs::mojom::kServiceName, info));
  }

#if defined(OS_CHROMEOS)
#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
  {
    service_manager::EmbeddedServiceInfo info;
    info.factory = base::BindRepeating([] {
      network::NetworkConnectionTracker* network_connection_tracker =
          content::GetNetworkConnectionTracker();
      return std::unique_ptr<service_manager::Service>(
          std::make_unique<chromeos::assistant::Service>(
              network_connection_tracker));
    });
    info.task_runner = base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::UI});
    services->insert(
        std::make_pair(chromeos::assistant::mojom::kServiceName, info));
  }
#endif

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    service_manager::EmbeddedServiceInfo info;
    info.task_runner = base::ThreadTaskRunnerHandle::Get();
    info.factory = base::BindRepeating(&ProfileImpl::CreateDeviceSyncService,
                                       base::Unretained(this));
    services->emplace(chromeos::device_sync::mojom::kServiceName, info);
  }

  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup) &&
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    service_manager::EmbeddedServiceInfo info;
    info.task_runner = base::ThreadTaskRunnerHandle::Get();
    info.factory = base::BindRepeating(
        &ProfileImpl::CreateMultiDeviceSetupService, base::Unretained(this));
    services->emplace(chromeos::multidevice_setup::mojom::kServiceName, info);
  }

#endif

#if !defined(OS_ANDROID)
  {
    // Binding the App Service here means that its preferences will be stored in
    // the primary Preferences file for this profile.
    service_manager::EmbeddedServiceInfo info;
    info.task_runner = base::ThreadTaskRunnerHandle::Get();
    info.factory =
        base::BindRepeating(&CreateAppService, base::Unretained(this));
    services->emplace(apps::mojom::kServiceName, info);
  }
#endif

  service_manager::EmbeddedServiceInfo identity_service_info;

  // The Identity Service must run on the UI thread.
  identity_service_info.task_runner = base::ThreadTaskRunnerHandle::Get();

  // NOTE: The dependencies of the Identity Service have not yet been created,
  // so it is not possible to bind them here. Instead, bind them at the time
  // of the actual request to create the Identity Service.
  identity_service_info.factory =
      base::Bind(&ProfileImpl::CreateIdentityService, base::Unretained(this));
  services->insert(
      std::make_pair(identity::mojom::kServiceName, identity_service_info));
}

std::string ProfileImpl::GetMediaDeviceIDSalt() {
  return media_device_id_salt_->GetSalt();
}

download::InProgressDownloadManager*
ProfileImpl::RetriveInProgressDownloadManager() {
#if defined(OS_ANDROID)
  return DownloadManagerService::GetInstance()
      ->RetriveInProgressDownloadManager(this);
#else
  return nullptr;
#endif
}

bool ProfileImpl::IsSameProfile(Profile* profile) {
  if (profile == static_cast<Profile*>(this))
    return true;
  Profile* otr_profile = off_the_record_profile_.get();
  return otr_profile && profile == otr_profile;
}

base::Time ProfileImpl::GetStartTime() const {
  return start_time_;
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
          if (locale_change_guard_ == NULL)
            locale_change_guard_.reset(new chromeos::LocaleChangeGuard(this));
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
  if (locale_change_guard_ == NULL)
    locale_change_guard_.reset(new chromeos::LocaleChangeGuard(this));
  locale_change_guard_->OnLogin();
}

void ProfileImpl::InitChromeOSPreferences() {
  chromeos_preferences_.reset(new chromeos::Preferences());
  chromeos_preferences_->Init(
      this, chromeos::ProfileHelper::Get()->GetUserByProfile(this));
}

#endif  // defined(OS_CHROMEOS)

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
  if (has_entry) {
    entry->SetSupervisedUserId(GetPrefs()->GetString(prefs::kSupervisedUserId));
    ProfileMetrics::UpdateReportedProfilesStatistics(profile_manager);
  }
}

void ProfileImpl::UpdateNameInStorage() {
  ProfileAttributesEntry* entry;
  bool has_entry = g_browser_process->profile_manager()
                       ->GetProfileAttributesStorage()
                       .GetProfileAttributesWithPath(GetPath(), &entry);
  if (has_entry) {
    entry->SetName(
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

// Gets the media cache parameters from the command line. |cache_path| will be
// set to the user provided path, or will not be touched if there is not an
// argument. |max_size| will be the user provided value or zero by default.
void ProfileImpl::GetMediaCacheParameters(base::FilePath* cache_path,
                                          int* max_size) {
  base::FilePath path(prefs_->GetFilePath(prefs::kDiskCacheDir));
  if (!path.empty())
    *cache_path = path.Append(cache_path->BaseName());

  *max_size = prefs_->GetInteger(prefs::kMediaCacheSize);
}

std::unique_ptr<domain_reliability::DomainReliabilityMonitor>
ProfileImpl::CreateDomainReliabilityMonitor(PrefService* local_state) {
  domain_reliability::DomainReliabilityService* service =
      domain_reliability::DomainReliabilityServiceFactory::GetInstance()
          ->GetForBrowserContext(this);
  if (!service)
    return std::unique_ptr<domain_reliability::DomainReliabilityMonitor>();

  return service->CreateMonitor(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
}

std::unique_ptr<service_manager::Service> ProfileImpl::CreateIdentityService() {
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(this);
  SigninManagerBase* signin_manager = SigninManagerFactory::GetForProfile(this);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(this);
  return std::make_unique<identity::IdentityService>(
      account_tracker, signin_manager, token_service);
}

#if defined(OS_CHROMEOS)
std::unique_ptr<service_manager::Service>
ProfileImpl::CreateDeviceSyncService() {
  return std::make_unique<chromeos::device_sync::DeviceSyncService>(
      IdentityManagerFactory::GetForProfile(this),
      gcm::GCMProfileServiceFactory::GetForProfile(this)->driver(),
      chromeos::GcmDeviceInfoProviderImpl::GetInstance(),
      GetURLLoaderFactory());
}

std::unique_ptr<service_manager::Service>
ProfileImpl::CreateMultiDeviceSetupService() {
  return std::make_unique<chromeos::multidevice_setup::MultiDeviceSetupService>(
      GetPrefs(),
      chromeos::device_sync::DeviceSyncClientFactory::GetForProfile(this),
      chromeos::multidevice_setup::AuthTokenValidatorFactory::GetForProfile(
          this),
      chromeos::multidevice_setup::OobeCompletionTrackerFactory::GetForProfile(
          this),
      std::make_unique<
          chromeos::multidevice_setup::AndroidSmsAppHelperDelegateImpl>(this),
      std::make_unique<
          chromeos::multidevice_setup::AndroidSmsPairingStateTrackerImpl>(this),
      chromeos::GcmDeviceInfoProviderImpl::GetInstance());
}

#endif  // defined(OS_CHROMEOS)
