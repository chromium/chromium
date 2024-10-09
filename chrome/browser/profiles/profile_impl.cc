// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/background_sync/background_sync_controller_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/client_hints/client_hints_factory.h"
#include "chrome/browser/content_index/content_index_provider_factory.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
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
#include "chrome/browser/privacy/privacy_metrics_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/pref_service_builder_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/startup_data.h"
#include "chrome/browser/storage/storage_notification_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_header_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tpcd/support/origin_trial_service_factory.h"
#include "chrome/browser/tpcd/support/top_level_trial_service_factory.h"
#include "chrome/browser/tpcd/support/tpcd_support_service_factory.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/prefs_internals_source.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service_factory.h"
#include "chrome/browser/webid/federated_identity_api_permission_context.h"
#include "chrome/browser/webid/federated_identity_api_permission_context_factory.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/sync_to_signin_migration.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/metrics/metrics_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/payments/core/payment_request_metrics.h"
#include "components/permissions/permission_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "components/version_info/channel.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/locale/locale_change_guard.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_factory_ash.h"
#include "chrome/browser/ash/preferences/preferences.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "components/password_manager/core/common/password_manager_features.h"
#else
#include "chrome/browser/accessibility/ax_main_node_annotator_controller_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/guest_profile_creation_logger.h"
#include "content/public/common/page_zoom.h"
#include "ui/accessibility/accessibility_features.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_PDF)

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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "chrome/browser/spellchecker/spellcheck_service.h"
#endif

using bookmarks::BookmarkModel;
using content::BrowserThread;
using content::DownloadManagerDelegate;

class ScopedAllowBlockingForProfile : public base::ScopedAllowBlocking {};

namespace {

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
// Delay before we explicitly create the SessionService.
static constexpr base::TimeDelta kCreateSessionServiceDelay =
    base::Milliseconds(500);
#endif

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
  ScopedAllowBlockingForProfile allow_io_to_create_directory;

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

#if BUILDFLAG(IS_CHROMEOS)
// Checks if |new_locale| is the same as |pref_locale| or |pref_locale| is used
// to show UI translation for |new_locale|. (e.g. "it" is used for "it-CH")
bool LocaleNotChanged(const std::string& pref_locale,
                      const std::string& new_locale) {
  std::string new_locale_converted = new_locale;
  language::ConvertToActualUILocale(&new_locale_converted);
  return pref_locale == new_locale_converted;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  switch (create_mode) {
    case CreateMode::kAsynchronous:
      DCHECK(delegate);
      creation_time = CreateProfileDirectory(io_task_runner.get(), path, true);
      break;
    case CreateMode::kSynchronous:
      if (base::PathExists(path)) {
        creation_time = GetCreationTimeForPath(path);
      } else {
        // TODO(rogerta): http://crbug/160553 - Bad things happen if we can't
        // write to the profile directory.  We should eventually be able to run
        // in this situation.
        if (!base::CreateDirectory(path)) {
          return nullptr;
        }

        CreateProfileReadme(path);
      }
      break;
  }

  std::unique_ptr<Profile> profile = base::WrapUnique(new ProfileImpl(
      path, delegate, create_mode, creation_time, io_task_runner));
  return profile;
}

// static
void ProfileImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled, false);
  registry->RegisterBooleanPref(prefs::kAllowDeletingBrowserHistory, true);
  registry->RegisterBooleanPref(policy::policy_prefs::kForceGoogleSafeSearch,
                                false);
  registry->RegisterIntegerPref(policy::policy_prefs::kForceYouTubeRestrict,
                                safe_search_api::YOUTUBE_RESTRICT_OFF);
  registry->RegisterStringPref(prefs::kAllowedDomainsForApps, std::string());

  registry->RegisterIntegerPref(prefs::kProfileAvatarIndex, -1);
  // Whether a profile is using an avatar without having explicitely chosen it
  // (i.e. was assigned by default by legacy profile creation).
  registry->RegisterBooleanPref(prefs::kProfileUsingDefaultAvatar, true);
  registry->RegisterBooleanPref(prefs::kProfileUsingGAIAAvatar, false);
  // Whether a profile is using a default avatar name (eg. Pickles or Person 1).
  registry->RegisterBooleanPref(prefs::kProfileUsingDefaultName, true);
  registry->RegisterStringPref(prefs::kProfileName, std::string());
#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  registry->RegisterBooleanPref(prefs::kOopPrintDriversAllowedByPolicy, true);
#endif
  registry->RegisterBooleanPref(prefs::kPrintPreviewDisabled, false);
  registry->RegisterStringPref(
      prefs::kPrintPreviewDefaultDestinationSelectionRules, std::string());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(prefs::kPrintPdfAsImageAvailability, false);
#endif
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_PRINTING)
  registry->RegisterIntegerPref(prefs::kPrintPostScriptMode, 0);
  registry->RegisterIntegerPref(prefs::kPrintRasterizationMode, 0);
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  registry->RegisterIntegerPref(prefs::kPrintRasterizePdfDpi, 0);
  registry->RegisterBooleanPref(prefs::kPrintPdfAsImageDefault, false);
#endif

  registry->RegisterBooleanPref(prefs::kForceEphemeralProfiles, false);
  registry->RegisterBooleanPref(prefs::kEnableMediaRouter, true);
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kShowCastIconInToolbar, false);
#endif  // !BUILDFLAG(IS_ANDROID)
  registry->RegisterTimePref(prefs::kProfileCreationTime, base::Time());

#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kPdfAnnotationsEnabled, true);
#endif
  registry->RegisterIntegerPref(prefs::kEnterpriseBadgingTemporarySetting, 0);
}

ProfileImpl::ProfileImpl(
    const base::FilePath& path,
    Delegate* delegate,
    CreateMode create_mode,
    base::Time path_creation_time,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : Profile(nullptr),
      path_(path),
      path_creation_time_(path_creation_time),
      io_task_runner_(std::move(io_task_runner)),
      start_time_(base::Time::Now()),
      delegate_(delegate) {
  TRACE_EVENT0("browser,startup", "ProfileImpl::ctor");
  DCHECK(!path.empty()) << "Using an empty path will attempt to write "
                        << "profile files to the root directory!";

  if (path == ProfileManager::GetGuestProfilePath()) {
    profile_metrics::SetBrowserProfileType(
        this, profile_metrics::BrowserProfileType::kGuest);
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  } else if (path == ProfileManager::GetSystemProfilePath()) {
    profile_metrics::SetBrowserProfileType(
        this, profile_metrics::BrowserProfileType::kSystem);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  } else {
    profile_metrics::SetBrowserProfileType(
        this, profile_metrics::BrowserProfileType::kRegular);
  }

  if (delegate_) {
    delegate_->OnProfileCreationStarted(this, create_mode);
  }

  // The ProfileImpl can be created both synchronously and asynchronously.
  bool async_prefs = create_mode == CreateMode::kAsynchronous;

#if BUILDFLAG(IS_ANDROID)
  auto* startup_data = g_browser_process->startup_data();
  DCHECK(startup_data && startup_data->GetProfileKey());
  TakePrefsFromStartupData();
  async_prefs = false;
#else
  LoadPrefsForNormalStartup(async_prefs);
#endif

  // Register on BrowserContext.
  user_prefs::UserPrefs::Set(this, prefs_.get());

  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40225390): Move this into
  // ProfileUserManagerController::OnProfileCreationStarted().
  if (ash::ProfileHelper::IsUserProfile(this)) {
    // |ash::InitializeAccountManager| is called during a User's session
    // initialization but some tests do not properly login to a User Session.
    // This invocation of |ash::InitializeAccountManager| is used only during
    // tests.
    // Note: |ash::InitializeAccountManager| is idempotent and safe to call
    // multiple times.
    // TODO(crbug.com/40635309): Remove this call.
    ash::InitializeAccountManager(
        path_, base::DoNothing() /* initialization_callback */);

    auto* account_manager = g_browser_process->platform_part()
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
#if !BUILDFLAG(IS_ANDROID)
  if (IsGuestSession()) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    base::UmaHistogramBoolean(
        "Profile.Guest.ForcedByPolicy",
        local_state->GetBoolean(prefs::kBrowserGuestModeEnforced));
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
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

  policy::CloudPolicyManager* cloud_policy_manager;
  policy::ConfigurationPolicyProvider* policy_provider;
#if BUILDFLAG(IS_CHROMEOS)
  if (force_immediate_policy_load)
    ash::DeviceSettingsService::Get()->LoadImmediately();
  else
    ash::DeviceSettingsService::Get()->LoadIfNotPresent();

  user_cloud_policy_manager_ash_ = policy::CreateUserCloudPolicyManagerAsh(
      this, force_immediate_policy_load, io_task_runner_);

  cloud_policy_manager = nullptr;
  policy_provider = GetUserCloudPolicyManagerAsh();
#else  // !BUILDFLAG(IS_CHROMEOS)
  {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ProfileAttributesEntry* entry =
        profile_manager->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(GetPath());

    if (entry && (!entry->GetProfileManagementEnrollmentToken().empty() ||
                  entry->IsDasherlessManagement())) {
      profile_cloud_policy_manager_ = policy::ProfileCloudPolicyManager::Create(
          GetPath(), GetPolicySchemaRegistryService()->registry(),
          force_immediate_policy_load, io_task_runner_,
          base::BindRepeating(&content::GetNetworkConnectionTracker),
          entry->IsDasherlessManagement());
      cloud_policy_manager = profile_cloud_policy_manager_.get();
    } else {
#else
    {
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      user_cloud_policy_manager_ = policy::UserCloudPolicyManager::Create(
          GetPath(), GetPolicySchemaRegistryService()->registry(),
          force_immediate_policy_load, io_task_runner_,
          base::BindRepeating(&content::GetNetworkConnectionTracker));
      cloud_policy_manager = user_cloud_policy_manager_.get();
    }
    policy_provider = cloud_policy_manager;
  }
#endif
  profile_policy_connector_ =
      policy::CreateProfilePolicyConnectorForBrowserContext(
          schema_registry_service_->registry(), cloud_policy_manager,
          policy_provider, g_browser_process->browser_policy_connector(),
          force_immediate_policy_load, this);

  bool is_signin_profile = false;
#if BUILDFLAG(IS_CHROMEOS)
  is_signin_profile = ash::ProfileHelper::IsSigninProfile(this);
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

  prefs_ = CreateProfilePrefService(
      pref_registry_, CreateExtensionPrefStore(this, false),
      profile_policy_connector_->policy_service(),
      g_browser_process->browser_policy_connector(),
      std::move(pref_validation_delegate), GetIOTaskRunner(), key_.get(), path_,
      async_prefs);
  key_->SetPrefs(prefs_.get());
#if BUILDFLAG(IS_CHROMEOS)
  // When Chrome crash or gets restarted for other reasons, it loads the policy
  // immediately. We need to cache the LacrosLaunchSwitch now, as the value is
  // needed later, while the profile is not fully initialized.
  if (force_immediate_policy_load &&
      ash::ProfileHelper::IsPrimaryProfile(this)) {
    auto& map = profile_policy_connector_->policy_service()->GetPolicies(
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
    ash::standalone_browser::BrowserSupport::InitializeForPrimaryUser(
        map, IsNewProfile(), IsRegularProfile());
    crosapi::browser_util::CacheLacrosAvailability(map);
    crosapi::browser_util::CacheLacrosDataBackwardMigrationMode(map);
    ash::standalone_browser::CacheLacrosSelection(map);
  }
#endif
}

void ProfileImpl::DoFinalInit(CreateMode create_mode) {
  TRACE_EVENT0("browser", "ProfileImpl::DoFinalInit");

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
      base::BindRepeating(&ProfileImpl::UpdateSupervisedUserIdInStorage,
                          base::Unretained(this)));

  // Changes in the profile avatar.
  pref_change_registrar_.Add(
      prefs::kProfileAvatarIndex,
      base::BindRepeating(&ProfileImpl::UpdateAvatarInStorage,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileUsingDefaultAvatar,
      base::BindRepeating(&ProfileImpl::UpdateAvatarInStorage,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileUsingGAIAAvatar,
      base::BindRepeating(&ProfileImpl::UpdateAvatarInStorage,
                          base::Unretained(this)));

  // Changes in the profile name.
  pref_change_registrar_.Add(
      prefs::kProfileUsingDefaultName,
      base::BindRepeating(&ProfileImpl::UpdateNameInStorage,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileName,
      base::BindRepeating(&ProfileImpl::UpdateNameInStorage,
                          base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kForceEphemeralProfiles,
      base::BindRepeating(&ProfileImpl::UpdateIsEphemeralInStorage,
                          base::Unretained(this)));

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

  // Background mode and plugins are not used with all profiles. These use
  // KeyedServices that might not be available such as the ExtensionSystem for
  // some profiles, e.g. the System profile.
  if (!AreKeyedServicesDisabledForProfileByDefault(this)) {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
    // Initialize the BackgroundModeManager - this has to be done here before
    // InitExtensions() is called because it relies on receiving notifications
    // when extensions are loaded. BackgroundModeManager is not needed under
    // ChromeOS because Chrome is always running, no need for special keep-alive
    // or launch-on-startup support unless kKeepAliveForTest is set.
    bool init_background_mode_manager = true;
#if BUILDFLAG(IS_CHROMEOS)
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kKeepAliveForTest)) {
      init_background_mode_manager = false;
    }
#endif
    if (init_background_mode_manager &&
        g_browser_process->background_mode_manager()) {
      g_browser_process->background_mode_manager()->RegisterProfile(this);
    }
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

#if BUILDFLAG(ENABLE_PLUGINS)
    ChromePluginServiceFilter::GetInstance()->RegisterProfile(this);
#endif
  }

  auto* db_provider = GetDefaultStoragePartition()->GetProtoDatabaseProvider();
  key_->SetProtoDatabaseProvider(db_provider);

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  dom_distiller::RegisterViewerSource(this);

#if BUILDFLAG(IS_CHROMEOS)
  MigrateSigninScopedDeviceId(this);

  if (ash::UserSessionManager::GetInstance()
          ->RestartToApplyPerSessionFlagsIfNeed(this, true)) {
    return;
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  if (IsGuestSession()) {
    // Note: We need to record the creation of the guest parent before the
    // `delegate_`'s `OnProfileCreationFinished()` callback executes, as it
    // might trigger the creation of a child OTR profile.
    profile::RecordGuestParentCreation(this);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // Not necessary for profiles that don't have a BookmarkModel.
  // On CrOS sync service will be initialized after sign in.
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(this);
  if (model) {
    // `BookmarkModelLoadedObserver` destroys itself eventually, when loading
    // completes.
    new BookmarkModelLoadedObserver(this, model);
  }
#endif

  // The ad service might not be available for some irregular profiles, like the
  // System Profile.
  if (heavy_ad_intervention::HeavyAdService* heavy_ad_service =
          HeavyAdServiceFactory::GetForBrowserContext(this)) {
    heavy_ad_service->Initialize(GetPath());
  }

  PushMessagingServiceImpl::InitializeForProfile(this);

  site_isolation::SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(this);

  content::URLDataSource::Add(this,
                              std::make_unique<PrefsInternalsSource>(this));

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (IsNewProfile()) {
    // The installed Windows language packs aren't determined until
    // the spellcheck service is initialized. Make sure the primary
    // preferred language is enabled for spellchecking until the user
    // opts out later. If there is no dictionary support for the language
    // then it will later be automatically disabled.
    SpellcheckService::EnableFirstUserLanguageForSpellcheck(prefs_.get());
  }
#endif

  if (delegate_) {
    TRACE_EVENT0("browser",
                 "ProfileImpl::DoFinalInit:DelegateOnProfileCreationFinished");
    // Fails if the browser is shutting down. This is done to avoid
    // launching new UI, finalising profile creation, etc. which
    // would trigger a crash down the line. See ...
    const bool shutting_down = g_browser_process->IsShuttingDown();
    delegate_->OnProfileCreationFinished(this, create_mode, !shutting_down,
                                         IsNewProfile());
    // The current Profile may be immediately deleted as part of
    // the call to OnProfileCreationFinished(...) if the initialisation is
    // reported as a failure, thus no code should be executed past
    // that point.
    if (shutting_down)
      return;
  }

  NotifyProfileInitializationComplete();

  RecordPrefValuesAfterProfileInitialization();

  SharingServiceFactory::GetForBrowserContext(this);

  HttpsFirstModeServiceFactory::GetForProfile(this);

  // The Privacy Metrics service should start alongside each profile session.
  PrivacyMetricsServiceFactory::GetForProfile(this);

  // The Privacy Sandbox service must be created with the profile to ensure that
  // preference reconciliation occurs.
  PrivacySandboxServiceFactory::GetForProfile(this);

#if BUILDFLAG(IS_ANDROID)
  // The password settings service needs to start listening to settings
  // changes from Google Mobile Services, as early as possible.
  PasswordManagerSettingsServiceFactory::GetForProfile(this);
#else

#if BUILDFLAG(ENABLE_PDF)
  if (features::IsPdfOcrEnabled()) {
    bool pcf_ocr_may_be_needed = true;
#if BUILDFLAG(IS_CHROMEOS)
    // `PdfOcrControllerFactory` is not needed in the not-signed-in profile of
    // ChromeOS as no user navigation to PDFs is possible there.
    pcf_ocr_may_be_needed = IsSignedIn();
#endif
    // Create the PDF OCR controller so that it can self-activate as needed.
    if (pcf_ocr_may_be_needed) {
      screen_ai::PdfOcrControllerFactory::GetForProfile(this);
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  if (features::IsMainNodeAnnotationsEnabled()) {
    screen_ai::AXMainNodeAnnotatorControllerFactory::GetForProfile(this);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // The announcement notification  service might not be available for some
  // irregular profiles, like the System Profile.
  if (AnnouncementNotificationService* announcement_notification =
          AnnouncementNotificationServiceFactory::GetForProfile(this)) {
    announcement_notification->MaybeShowNotification();
  }

  // Request an OriginTrialsControllerDelegate to ensure it is initialized.
  // OriginTrialsControllerDelegate needs to be explicitly created here instead
  // of using the common pattern for initializing with the profile (override
  // OriginTrialsFactory::ServiceIsCreatedWithBrowserContext() to return true)
  // as it depends on the default StoragePartition being initialized.
  GetOriginTrialsControllerDelegate();

  // The TpcdTrialService, TopLevelTrialService, OriginTrialService, and
  // StorageAccessHeaderService for third-party cookie deprecation must be
  // created with the profile, but after the initialization of the
  // OriginTrialsControllerDelegate, as it depends on it.
  tpcd::trial::TpcdTrialServiceFactory::GetForProfile(this);
  tpcd::trial::TopLevelTrialServiceFactory::GetForProfile(this);
  tpcd::trial::OriginTrialServiceFactory::GetForProfile(this);
  storage_access_api::trial::StorageAccessHeaderServiceFactory::GetForProfile(
      this);
}

base::FilePath ProfileImpl::last_selected_directory() {
  return GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
}

void ProfileImpl::set_last_selected_directory(const base::FilePath& path) {
  GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory, path);
}

ProfileImpl::~ProfileImpl() {
  MaybeSendDestroyedNotification();

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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool primary_otr_available = false;
#endif

  // Get a list of existing OTR profiles since |off_the_record_profile_| might
  // be modified after the call to |DestroyProfileNow|.
  for (auto& otr_profile : otr_profiles_) {
    raw_otr_profiles.push_back(otr_profile.second.get());
#if BUILDFLAG(ENABLE_EXTENSIONS)
    primary_otr_available |= (otr_profile.first == OTRProfileID::PrimaryID());
#endif
  }

  for (Profile* otr_profile : raw_otr_profiles)
    ProfileDestroyer::DestroyOTRProfileImmediately(otr_profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!primary_otr_available &&
      !extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(this)) {
    ExtensionPrefValueMap* pref_value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(this);
    DCHECK(pref_value_map);
    pref_value_map->ClearAllIncognitoSessionOnlyPreferences();
  }
#endif

  FullBrowserTransitionManager::Get()->OnProfileDestroyed(this);

  // Records the number of active KeyedServices for SystemProfile right before
  // shutting the Services.
  if (IsSystemProfile())
    ProfileMetrics::LogSystemProfileKeyedServicesCount(this);

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

  // This must be called before ProfileIOData::ShutdownOnUIThread but after
  // other profile-related destroy notifications are dispatched.
  ShutdownStoragePartitions();
}

std::string ProfileImpl::GetProfileUserName() const {
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(this);
  if (identity_manager) {
    return identity_manager
        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
        .email;
  }

  return std::string();
}

std::unique_ptr<content::ZoomLevelDelegate>
ProfileImpl::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelPrefs>(
      GetPrefs(), GetPath(), partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}

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

Profile* ProfileImpl::GetOffTheRecordProfile(const OTRProfileID& otr_profile_id,
                                             bool create_if_needed) {
  if (HasOffTheRecordProfile(otr_profile_id))
    return otr_profiles_[otr_profile_id].get();

  if (!create_if_needed)
    return nullptr;

  // Create a new OffTheRecordProfile
  std::unique_ptr<Profile> otr_profile =
      Profile::CreateOffTheRecordProfile(this, otr_profile_id);
  Profile* raw_otr_profile = otr_profile.get();

  otr_profiles_[otr_profile_id] = std::move(otr_profile);

  NotifyOffTheRecordProfileCreated(raw_otr_profile);

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
  if (profile_id == OTRProfileID::PrimaryID() &&
      !extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(this)) {
    ExtensionPrefValueMap* pref_value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(this);
    DCHECK(pref_value_map);
    pref_value_map->ClearAllIncognitoSessionOnlyPreferences();
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

bool ProfileImpl::IsChild() const {
  return GetPrefs()->GetString(prefs::kSupervisedUserId) ==
         supervised_user::kChildAccountSUID;
}

bool ProfileImpl::AllowsBrowserWindows() const {
#if BUILDFLAG(IS_CHROMEOS)
  if (ash::ProfileHelper::IsSigninProfile(this) ||
      ash::ProfileHelper::IsLockScreenAppProfile(this)) {
    return false;
  }
#endif
  return !IsSystemProfile();
}

ExtensionSpecialStoragePolicy* ProfileImpl::GetExtensionSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get()) {
    TRACE_EVENT0("browser", "ProfileImpl::GetExtensionSpecialStoragePolicy");
    extension_special_storage_policy_ =
        base::MakeRefCounted<ExtensionSpecialStoragePolicy>(
            CookieSettingsFactory::GetForProfile(this).get());
  }
  return extension_special_storage_policy_.get();
#else
  return NULL;
#endif
}

void ProfileImpl::OnLocaleReady(CreateMode create_mode) {
  TRACE_EVENT0("browser", "ProfileImpl::OnLocaleReady");

  // Migrate obsolete prefs.
  MigrateObsoleteProfilePrefs(GetPrefs(), GetPath());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note: Extension preferences can be keyed off the extension ID, so need to
  // be handled specially (rather than directly as part of
  // MigrateObsoleteProfilePrefs()).
  if (!extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(this)) {
    extensions::ExtensionPrefs* extension_prefs =
        extensions::ExtensionPrefs::Get(this);
    DCHECK(extension_prefs);
    extension_prefs->MigrateObsoleteExtensionPrefs();
  }
#endif

  // Run the sync->signin-migration now that PrefService is ready but none of
  // the services affected by the migration are.
  // TODO(crbug.com/369297671): Remove one year after launching
  // kForceMigrateSyncingUserToSignedIn on all //chrome platforms.
  CHECK(GetPrefs(), base::NotFatalUntil::M133);
  CHECK(!IdentityManagerFactory::GetForProfileIfExists(this),
        base::NotFatalUntil::M133);
  CHECK(!SyncServiceFactory::HasSyncService(this), base::NotFatalUntil::M133);
  CHECK(!BookmarkModelFactory::GetForBrowserContextIfExists(this),
        base::NotFatalUntil::M133);
  CHECK(!ProfilePasswordStoreFactory::HasStore(this),
        base::NotFatalUntil::M133);
  CHECK(!AccountPasswordStoreFactory::HasStore(this),
        base::NotFatalUntil::M133);
  CHECK(!ReadingListModelFactory::HasModel(this), base::NotFatalUntil::M133);
  browser_sync::MaybeMigrateSyncingUserToSignedIn(GetPath(), GetPrefs());

#if BUILDFLAG(IS_ANDROID)
  // On Android StartupData creates proto database provider for the profile
  // before profile is created, so move ownership to storage partition.
  GetDefaultStoragePartition()->SetProtoDatabaseProvider(
      g_browser_process->startup_data()->TakeProtoDatabaseProvider());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // If this is a kiosk profile, reset some of its prefs which should not
  // persist between sessions.
  if (IsRunningInForcedAppMode()) {
    ash::ResetEphemeralKioskPreferences(prefs_.get());
  }
#endif

  g_browser_process->profile_manager()->InitProfileUserPrefs(this);

#if BUILDFLAG(IS_CHROMEOS)
  arc::ArcServiceLauncher::Get()->MaybeSetProfile(this);
#endif

  FullBrowserTransitionManager::Get()->OnProfileCreated(this);

  SimpleDependencyManager::GetInstance()->CreateServices(GetProfileKey());
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  ChromeVersionService::OnProfileLoaded(prefs_.get(), IsNewProfile());
  DoFinalInit(create_mode);
}

void ProfileImpl::OnPrefsLoaded(CreateMode create_mode, bool success) {
  TRACE_EVENT0("browser", "ProfileImpl::OnPrefsLoaded");
  if (!success) {
    if (delegate_)
      delegate_->OnProfileCreationFinished(this, create_mode, false, false);
    return;
  }

  // Fail fast if the browser is shutting down. We want to avoid launching new
  // UI, finalising profile creation, etc. which would trigger a crash down the
  // the line. See crbug.com/625646
  if (g_browser_process->IsShuttingDown()) {
    if (delegate_)
      delegate_->OnProfileCreationFinished(this, create_mode, false, false);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  switch (create_mode) {
    case CreateMode::kSynchronous:
      // Synchronous create mode implies that either it is restart after crash,
      // or we are in tests. In both cases the first loaded locale is correct.
      OnLocaleReady(create_mode);
      break;
    case CreateMode::kAsynchronous:
      if (ash::ProfileHelper::IsPrimaryProfile(this)) {
        auto& map = profile_policy_connector_->policy_service()->GetPolicies(
            policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                    std::string()));
        ash::standalone_browser::BrowserSupport::InitializeForPrimaryUser(
            map, IsNewProfile(), IsRegularProfile());
        crosapi::browser_util::CacheLacrosAvailability(map);
        crosapi::browser_util::CacheLacrosDataBackwardMigrationMode(map);
        ash::standalone_browser::CacheLacrosSelection(map);
      }

      ash::UserSessionManager::GetInstance()->RespectLocalePreferenceWrapper(
          this, base::BindOnce(&ProfileImpl::OnLocaleReady,
                               base::Unretained(this), create_mode));
      break;
  }
#else
  OnLocaleReady(create_mode);
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // SessionService depends on Profile::GetPrefs() and therefore shouldn't be
  // forced initialized before prefs are loaded. Starting this Timer before
  // that point resulted in a racy use-before-initialized in the past.
  create_session_service_timer_.Start(
      FROM_HERE, kCreateSessionServiceDelay, this,
      &ProfileImpl::EnsureSessionServiceCreated);
#endif
}

bool ProfileImpl::WasCreatedByVersionOrLater(const std::string& version) {
  base::Version profile_version(ChromeVersionService::GetVersion(prefs_.get()));
  base::Version arg_version(version);
  return (profile_version.CompareTo(arg_version) >= 0);
}

bool ProfileImpl::ShouldRestoreOldSessionCookies() {
#if BUILDFLAG(IS_ANDROID)
  SessionStartupPref startup_pref(SessionStartupPref::GetDefaultStartupType());
  return startup_pref.ShouldRestoreLastSession();
#else
  SessionStartupPref startup_pref =
      StartupBrowserCreator::GetSessionStartupPref(
          *base::CommandLine::ForCurrentProcess(), this);
  return ExitTypeService::GetLastSessionExitType(this) == ExitType::kCrashed ||
         startup_pref.ShouldRestoreLastSession();
#endif
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

ChromeZoomLevelPrefs* ProfileImpl::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetDefaultStoragePartition()->GetZoomLevelDelegate());
}

// TODO(crbug.com/40526371): Remove this function.
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

#if BUILDFLAG(IS_CHROMEOS)
policy::UserCloudPolicyManagerAsh* ProfileImpl::GetUserCloudPolicyManagerAsh() {
  return user_cloud_policy_manager_ash_.get();
}
#else
policy::UserCloudPolicyManager* ProfileImpl::GetUserCloudPolicyManager() {
  return user_cloud_policy_manager_.get();
}

policy::ProfileCloudPolicyManager* ProfileImpl::GetProfileCloudPolicyManager() {
  return profile_cloud_policy_manager_.get();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

policy::CloudPolicyManager* ProfileImpl::GetCloudPolicyManager() {
#if BUILDFLAG(IS_CHROMEOS)
  return GetUserCloudPolicyManagerAsh();
#else
  if (user_cloud_policy_manager_) {
    return GetUserCloudPolicyManager();
  }
  if (profile_cloud_policy_manager_) {
    return GetProfileCloudPolicyManager();
  }
  return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

policy::ConfigurationPolicyProvider*
ProfileImpl::configuration_policy_provider() {
#if BUILDFLAG(IS_CHROMEOS)
  if (user_cloud_policy_manager_ash_)
    return user_cloud_policy_manager_ash_.get();
  return nullptr;
#else  // !BUILDFLAG(IS_CHROMEOS)
  if (user_cloud_policy_manager_.get()) {
    return user_cloud_policy_manager_.get();
  } else {
    return profile_cloud_policy_manager_.get();
  }
#endif
}

policy::ProfilePolicyConnector* ProfileImpl::GetProfilePolicyConnector() {
  return profile_policy_connector_.get();
}

const policy::ProfilePolicyConnector* ProfileImpl::GetProfilePolicyConnector()
    const {
  return profile_policy_connector_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
ProfileImpl::GetURLLoaderFactory() {
  return GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess();
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

content::PlatformNotificationService*
ProfileImpl::GetPlatformNotificationService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return PlatformNotificationServiceFactory::GetForProfile(this);
}

content::PushMessagingService* ProfileImpl::GetPushMessagingService() {
  return PushMessagingServiceFactory::GetForProfile(this);
}

content::StorageNotificationService*
ProfileImpl::GetStorageNotificationService() {
#if BUILDFLAG(IS_ANDROID)
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

content::FederatedIdentityApiPermissionContextDelegate*
ProfileImpl::GetFederatedIdentityApiPermissionContext() {
  return FederatedIdentityApiPermissionContextFactory::GetForProfile(this);
}

content::FederatedIdentityAutoReauthnPermissionContextDelegate*
ProfileImpl::GetFederatedIdentityAutoReauthnPermissionContext() {
  return FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
      this);
}

content::FederatedIdentityPermissionContextDelegate*
ProfileImpl::GetFederatedIdentityPermissionContext() {
  return FederatedIdentityPermissionContextFactory::GetForProfile(this);
}

content::KAnonymityServiceDelegate*
ProfileImpl::GetKAnonymityServiceDelegate() {
  return KAnonymityServiceFactory::GetForProfile(this);
}

content::ReduceAcceptLanguageControllerDelegate*
ProfileImpl::GetReduceAcceptLanguageControllerDelegate() {
  return ReduceAcceptLanguageFactory::GetForProfile(this);
}

content::OriginTrialsControllerDelegate*
ProfileImpl::GetOriginTrialsControllerDelegate() {
  return OriginTrialsFactory::GetForBrowserContext(this);
}

std::unique_ptr<download::InProgressDownloadManager>
ProfileImpl::RetrieveInProgressDownloadManager() {
  return DownloadManagerUtils::RetrieveInProgressDownloadManager(this);
}

content::FileSystemAccessPermissionContext*
ProfileImpl::GetFileSystemAccessPermissionContext() {
  return FileSystemAccessPermissionContextFactory::GetForProfile(this);
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

#if BUILDFLAG(IS_CHROMEOS)
void ProfileImpl::ChangeAppLocale(const std::string& new_locale,
                                  AppLocaleChangedVia via) {
  if (new_locale.empty()) {
    NOTREACHED_IN_MIGRATION();
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
              std::make_unique<ash::LocaleChangeGuard>(this, local_state);
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
    case APP_LOCALE_CHANGED_VIA_DEMO_SESSION_REVERT:
    case APP_LOCALE_CHANGED_VIA_SYSTEM_TRAY: {
      // no-op
      break;
    }
    case APP_LOCALE_CHANGED_VIA_UNKNOWN: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }
  if (do_update_pref)
    GetPrefs()->SetString(language::prefs::kApplicationLocale, new_locale);
  if (via != APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN)
    local_state->SetString(language::prefs::kApplicationLocale, new_locale);

  if (user_manager::UserManager::Get()->GetOwnerAccountId() ==
      ash::ProfileHelper::Get()->GetUserByProfile(this)->GetAccountId())
    local_state->SetString(prefs::kOwnerLocale, new_locale);
}

void ProfileImpl::OnLogin() {
  if (!locale_change_guard_) {
    locale_change_guard_ = std::make_unique<ash::LocaleChangeGuard>(
        this, g_browser_process->local_state());
  }
  locale_change_guard_->OnLogin();
}

void ProfileImpl::InitChromeOSPreferences() {
  chromeos_preferences_ = std::make_unique<ash::Preferences>();
  chromeos_preferences_->Init(
      this, ash::ProfileHelper::Get()->GetUserByProfile(this));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

bool ProfileImpl::IsNewProfile() const {
#if !BUILDFLAG(IS_ANDROID)
  // The profile is new if the preference files has just been created, except on
  // first run, because the installer may create a preference file. See
  // https://crbug.com/728402
  if (first_run::IsChromeFirstRun())
    return true;
#endif

  return GetPrefs()->GetInitializationStatus() ==
         PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}

void ProfileImpl::SetCreationTimeForTesting(base::Time creation_time) {
  prefs_->SetTime(prefs::kProfileCreationTime, creation_time);
}

bool ProfileImpl::IsSignedIn() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(this);
  // TODO(crbug.com/348368545): Switch to ConsentLevel::kSignin on ChromeOS.
  signin::ConsentLevel consent_level =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  return identity_manager && identity_manager->HasPrimaryAccount(consent_level);
}

GURL ProfileImpl::GetHomePage() {
  // --homepage overrides any preferences.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHomePage)) {
    // TODO(evanm): clean up usage of DIR_CURRENT.
    //   http://code.google.com/p/chromium/issues/detail?id=60630
    // For now, allow this code to call getcwd().
    base::ScopedAllowBlocking allow_blocking;

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
  ProfileAttributesEntry* entry = profile_manager->GetProfileAttributesStorage()
                                      .GetProfileAttributesWithPath(GetPath());
  if (entry)
    entry->SetSupervisedUserId(GetPrefs()->GetString(prefs::kSupervisedUserId));
}

void ProfileImpl::UpdateNameInStorage() {
  ProfileAttributesEntry* entry = g_browser_process->profile_manager()
                                      ->GetProfileAttributesStorage()
                                      .GetProfileAttributesWithPath(GetPath());
  if (entry) {
    entry->SetLocalProfileName(
        base::UTF8ToUTF16(GetPrefs()->GetString(prefs::kProfileName)),
        GetPrefs()->GetBoolean(prefs::kProfileUsingDefaultName));
  }
}

void ProfileImpl::UpdateAvatarInStorage() {
  ProfileAttributesEntry* entry = g_browser_process->profile_manager()
                                      ->GetProfileAttributesStorage()
                                      .GetProfileAttributesWithPath(GetPath());
  if (entry) {
    entry->SetAvatarIconIndex(
        GetPrefs()->GetInteger(prefs::kProfileAvatarIndex));
    entry->SetIsUsingDefaultAvatar(
        GetPrefs()->GetBoolean(prefs::kProfileUsingDefaultAvatar));
    entry->SetIsUsingGAIAPicture(
        GetPrefs()->GetBoolean(prefs::kProfileUsingGAIAAvatar));
  }
}

void ProfileImpl::UpdateIsEphemeralInStorage() {
  ProfileAttributesEntry* entry = g_browser_process->profile_manager()
                                      ->GetProfileAttributesStorage()
                                      .GetProfileAttributesWithPath(GetPath());
  // If a profile is omitted, it has to be ephemeral and thus setting the value
  // based on the pref does not make any sense. Whenever the profile is set as
  // non-omitted, it should also update IsEphemeral to respect this pref.
  if (entry && !entry->IsOmitted()) {
    entry->SetIsEphemeral(
        GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles));
  }
}

void ProfileImpl::RecordPrefValuesAfterProfileInitialization() {
  // Measure whether users have the "Allow sites to check if you have payment
  // methods saved" toggle enabled or disabled in chrome://settings/payments
  //
  // This is only relevant for regular profiles, as guest and incognito profiles
  // do not have access to this settings page nor will any changes to the pref
  // in those profiles affect future browsing sessions.
  if (IsRegularProfile()) {
    payments::RecordCanMakePaymentPrefMetrics(*GetPrefs(), "Startup");
  }
}
