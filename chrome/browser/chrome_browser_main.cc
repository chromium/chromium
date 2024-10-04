// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/platform_thread.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/active_use_util.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_browser_field_trials.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/component_updater/registration.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/webrtc_log_util.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/expired_histograms_array.h"
#include "chrome/browser/metrics/shutdown_watcher_helper.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/chrome_serialized_navigation_driver.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/startup_data.h"
#include "chrome/browser/tracing/trace_event_system_stats_monitor.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_app_modal_dialog_view_factory.h"
#include "chrome/browser/ui/profiles/profile_error_dialog.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs.h"
#include "chrome/browser/ui/webui/chrome_web_ui_configs.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/media/media_resource_provider.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/profiler/unwind_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/color/color_mixers.h"
#include "components/device_event_log/device_event_log.h"
#include "components/embedder_support/origin_trials/component_updater_utils.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/embedder_support/switches.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/google/core/common/google_util.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/core/browser/language_usage_metrics.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/common/language_util.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/expired_histogram_util.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_shutdown.h"
#include "components/metrics/persistent_histograms.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/buildflags.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"
#include "components/sampling_profiler/process_type.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_metrics_logger_impl.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/synthetic_trial_syncer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/profiling.h"
#include "extensions/buildflags/buildflags.h"
#include "media/audio/audio_manager.h"
#include "media/base/localized_strings.h"
#include "media/media_buildflags.h"
#include "net/base/data_url.h"
#include "net/base/net_module.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "third_party/blink/public/common/origin_trials/origin_trials_settings_provider.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider_manager.h"

#if BUILDFLAG(ENABLE_UPDATER)
#include "chrome/browser/updater/scheduler.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/share/share_history.h"
#include "chrome/browser/ui/page_info/chrome_page_info_client.h"
#include "ui/base/resource/resource_bundle_android.h"
#else
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resources_integrity.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/usb/web_usb_detector.h"
#include "components/soda/soda_installer.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/first_run/upgrade_util.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/process/process.h"
#include "base/task/task_traits.h"
#else
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/hardware_data_usage_controller.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/first_run/upgrade_util_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crashpad.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <Security/Security.h>

#if defined(ARCH_CPU_X86_64)
#include "base/mac/mac_util.h"
#include "base/threading/platform_thread.h"
#endif  // defined(ARCH_CPU_X86_64)

#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/ui_features.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "base/trace_event/trace_event_etw_export_win.h"
#include "base/win/win_util.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/win/browser_util.h"
#include "chrome/browser/win/chrome_select_file_dialog_factory.h"
#include "chrome/browser/win/parental_controls.h"
#include "chrome/install_static/install_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/metrics/desktop_session_duration/touch_mode_stats_tracker.h"
#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"
#include "ui/base/pointer/touch_ui_controller.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/headless/headless_mode_metrics.h"  // nogncheck
#include "chrome/browser/headless/headless_mode_util.h"     // nogncheck
#include "components/headless/select_file_dialog/headless_select_file_dialog.h"
#include "ui/gfx/switches.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/browser/process_singleton.h"
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_protocols.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/components/javascript_dialog_extensions_client/javascript_dialog_extension_client_impl.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_process_host.h"
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_info_handler.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/printing_init.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_prefs.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/common/printing/printing_init.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
#include "printing/printed_document.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "components/rlz/rlz_tracker.h"  // nogncheck crbug.com/1125897
#endif  // BUILDFLAG(ENABLE_RLZ)

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_features.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#include "sql/database.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

namespace {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
constexpr base::FilePath::CharType kMediaHistoryDatabaseName[] =
    FILE_PATH_LITERAL("Media History");

void DeleteMediaHistoryDatabase(const base::FilePath& profile_path) {
  auto db_path = profile_path.Append(kMediaHistoryDatabaseName);
  base::UmaHistogramBoolean("Media.MediaHistory.DatabaseExists",
                            base::PathExists(db_path));
  sql::Database::Delete(db_path);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Initialized in PreMainMessageLoopRun() and handed off to content:: in
// WillRunMainMessageLoop() (or in TakeRunLoopForTest() in tests)
std::unique_ptr<base::RunLoop>& GetMainRunLoopInstance() {
  static base::NoDestructor<std::unique_ptr<base::RunLoop>>
      main_run_loop_instance;
  return *main_run_loop_instance;
}
#endif

// This function provides some ways to test crash and assertion handling
// behavior of the program.
void HandleTestParameters(const base::CommandLine& command_line) {
  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kBrowserCrashTest)) {
    base::ImmediateCrash();
  }
}

// Initializes the initial profile, possibly doing some user prompting to pick
// a fallback profile. Returns either
// - kBrowserWindow mode with the newly created profile,
// - kProfilePicker mode indicating that the profile picker should be shown;
//   the profile is a guest profile in this case, or
// - kError mode with a nullptr profile if startup should not continue.
StartupProfileInfo CreateInitialProfile(
    const base::FilePath& user_data_dir,
    const base::CommandLine& parsed_command_line) {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::CreateProfile");
  base::Time start = base::Time::Now();

  bool last_used_profile_set = false;

// If the browser is launched due to activation on Windows native
// notification, the profile id encoded in the notification launch id should
// be chosen over all others.
#if BUILDFLAG(IS_WIN)
  base::FilePath profile_basename =
      NotificationLaunchId::GetNotificationLaunchProfileBaseName(
          parsed_command_line);
  if (!profile_basename.empty()) {
    profiles::SetLastUsedProfile(profile_basename);
    last_used_profile_set = true;
  }
#endif  // BUILDFLAG(IS_WIN)

  bool profile_dir_specified =
      profiles::IsMultipleProfilesEnabled() &&
      parsed_command_line.HasSwitch(switches::kProfileDirectory);
  base::FilePath last_used_profile;
  if (!last_used_profile_set && profile_dir_specified) {
    last_used_profile =
        parsed_command_line.GetSwitchValuePath(switches::kProfileDirectory);
  }
  if (parsed_command_line.HasSwitch(
          switches::kIgnoreProfileDirectoryIfNotExists) &&
      !base::DirectoryExists(user_data_dir.Append(last_used_profile))) {
    last_used_profile.clear();
  }
  if (!last_used_profile.empty()) {
    profiles::SetLastUsedProfile(last_used_profile);
    last_used_profile_set = true;
  }

  if (last_used_profile_set &&
      !parsed_command_line.HasSwitch(switches::kAppId)) {
    // Clear kProfilesLastActive since the user only wants to launch a specific
    // profile. Don't clear it if the user launched a web app, in order to not
    // break any subsequent multi-profile session restore.
    g_browser_process->local_state()->SetList(prefs::kProfilesLastActive,
                                              base::Value::List());
  }

  StartupProfileInfo profile_info;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
  profile_info = {ProfileManager::CreateInitialProfile(),
                  StartupProfileMode::kBrowserWindow};

  // TODO(port): fix this. See comments near the definition of |user_data_dir|.
  // It is better to CHECK-fail here than it is to silently exit because of
  // missing code in the above test.
  CHECK(profile_info.profile) << "Cannot get default profile.";

#else
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros has a special "primary" profile that is tied to the active ChromeOS
  // user identity. Lacros might attempt to load this profile synchorously via
  // `ProfileManager::GetPrimaryUserProfile()` or
  // `ProfileManager::GetActiveUserProfile()`. In combination with asynchronous
  // profile loading, this can lead to a crash (see https://crbug.com/1289527).
  // Load the primary Lacros profile before any other profile to ensure that the
  // primary profile is always loaded.
  // TODO(crbug.com/40203366): remove this once Lacros no longer uses
  // GetActiveUserProfile() and GetPrimaryUserProfile().
  ProfileManager::GetPrimaryUserProfile();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  profile_info =
      GetStartupProfile(/*cur_dir=*/base::FilePath(), parsed_command_line);

  if (profile_info.mode == StartupProfileMode::kError &&
      !last_used_profile_set) {
    profile_info = GetFallbackStartupProfile();
    base::UmaHistogramEnumeration(
        "ProfilePicker.StartupMode.FallbackProfileUsed", profile_info.mode);
  }

  if (profile_info.mode == StartupProfileMode::kError) {
    ProfileErrorType error_type =
        profile_dir_specified ? ProfileErrorType::CREATE_FAILURE_SPECIFIED
                              : ProfileErrorType::CREATE_FAILURE_ALL;

    // TODO(lwchkg): What diagnostics do you want to include in the feedback
    // report when an error occurs?
    ShowProfileErrorDialog(error_type, IDS_COULDNT_STARTUP_PROFILE_ERROR,
                           "Error creating initial profile.");
    return profile_info;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)

  UMA_HISTOGRAM_LONG_TIMES(
      "Startup.CreateFirstProfile", base::Time::Now() - start);
  return profile_info;
}

#if BUILDFLAG(IS_MAC)
OSStatus KeychainCallback(SecKeychainEvent keychain_event,
                          SecKeychainCallbackInfo* info, void* context) {
  return noErr;
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
void ProcessSingletonNotificationCallbackImpl(
    base::CommandLine command_line,
    const base::FilePath& current_directory) {
  // Drop the request if the browser process is already shutting down.
  if (!g_browser_process || g_browser_process->IsShuttingDown() ||
      browser_shutdown::HasShutdownStarted()) {
    return;
  }

#if BUILDFLAG(IS_WIN)
  // The uninstall command-line switch is handled by the origin process; see
  // ChromeMainDelegate::PostEarlyInitialization(...). The other process won't
  // be able to become the singleton process and will display a window asking
  // the user to close running Chrome instances.
  if (command_line.HasSwitch(switches::kUninstall)) {
    return;
  }
#endif

#if BUILDFLAG(IS_LINUX)
  // Set the global activation token sent as a command line switch by another
  // browser process. This also removes the switch after use to prevent any side
  // effects of leaving it in the command line after this point.
  base::nix::ExtractXdgActivationTokenFromCmdLine(command_line);
#endif

  StartupProfilePathInfo startup_profile_path_info =
      GetStartupProfilePath(current_directory, command_line,
                            /*ignore_profile_picker=*/false);
  DCHECK_NE(startup_profile_path_info.reason, StartupProfileModeReason::kError);
  base::UmaHistogramEnumeration(
      "ProfilePicker.StartupMode.NotificationCallback",
      StartupProfileModeFromReason(startup_profile_path_info.reason));
  base::UmaHistogramEnumeration(
      "ProfilePicker.StartupReason.NotificationCallback",
      startup_profile_path_info.reason);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, current_directory, startup_profile_path_info);

  // Record now as the last successful chrome start.
  if constexpr (kShouldRecordActiveUse) {
    GoogleUpdateSettings::SetLastRunTime();
  }
}
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)

#if !BUILDFLAG(IS_ANDROID)
bool ShouldInstallSodaDuringPostProfileInit(
    const base::CommandLine& command_line,
    const Profile* const profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(
             ash::features::kOnDeviceSpeechRecognition) &&
         ash::IsUserBrowserContext(const_cast<Profile*>(profile));
#elif !BUILDFLAG(IS_CHROMEOS_LACROS)
  return !command_line.HasSwitch(switches::kDisableComponentUpdate);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // !BUILDFLAG(IS_ANDROID)

void DisallowKeyedServiceFactoryRegistration() {
  // From this point, do not allow KeyedServiceFactories to be registered, all
  // factories should be registered in the main registration function
  // `ChromeBrowserMainExtraPartsProfiles::EnsureBrowserContextKeyedServiceFactoriesBuilt()`.
  BrowserContextDependencyManager::GetInstance()
      ->DisallowKeyedServiceFactoryRegistration(
          "ChromeBrowserMainExtraPartsProfiles::"
          "EnsureBrowserContextKeyedServiceFactoriesBuilt()");
}

#if !BUILDFLAG(IS_ANDROID)
void StartWatchingForProcessShutdownHangs() {
  // This HangWatcher scope is covering the shutdown phase up to the end of the
  // process. Intentionally leak this instance so that it is not destroyed
  // before process termination.
  base::HangWatcher::SetShuttingDown();
  auto* watcher = new base::WatchHangsInScope(base::Seconds(30));
  ANNOTATE_LEAKING_OBJECT_PTR(watcher);
  std::ignore = watcher;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// ChromeBrowserMainParts::ProfileInitManager ----------------------------------

// Runs `CallPostProfileInit()` on all existing and future profiles, the
// initial profile is processed first. The initial profile is nullptr when the
// profile picker is shown.
class ChromeBrowserMainParts::ProfileInitManager
    : public ProfileManagerObserver {
 public:
  ProfileInitManager(ChromeBrowserMainParts* browser_main,
                     Profile* initial_profile);
  ~ProfileInitManager() override;

  ProfileInitManager(const ProfileInitManager&) = delete;
  ProfileInitManager& operator=(const ProfileInitManager&) = delete;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
  // Raw pointer. This is safe because `ChromeBrowserMainParts` owns `this`.
  const raw_ptr<ChromeBrowserMainParts> browser_main_;
};

ChromeBrowserMainParts::ProfileInitManager::ProfileInitManager(
    ChromeBrowserMainParts* browser_main,
    Profile* initial_profile)
    : browser_main_(browser_main) {
  // `initial_profile` is null when the profile picker is shown.
  if (initial_profile) {
    browser_main_->CallPostProfileInit(initial_profile);
  }

  // Run `CallPostProfileInit()` on the other existing and future profiles.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Register the observer first, in case `OnProfileAdded()` causes a creation.
  profile_manager_observer_.Observe(profile_manager);
  for (auto* profile : profile_manager->GetLoadedProfiles()) {
    DCHECK(profile);
    if (profile == initial_profile) {
      continue;
    }
    OnProfileAdded(profile);
  }
}

ChromeBrowserMainParts::ProfileInitManager::~ProfileInitManager() = default;

void ChromeBrowserMainParts::ProfileInitManager::OnProfileAdded(
    Profile* profile) {
  if (profile->IsSystemProfile() || profile->IsGuestSession()) {
    // Ignore the system profile that is used for displaying the profile picker,
    // and the "parent" guest profile. `CallPostProfileInit()` should be called
    // only for profiles that are used for browsing.
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ignore ChromeOS helper profiles (sign-in, lockscreen, etc).
  if (!ash::ProfileHelper::IsUserProfile(profile)) {
    // Notify of new profile initialization only for regular profiles. The
    // startup profile initialization is triggered by another code path.
    return;
  }
#endif

  browser_main_->CallPostProfileInit(profile);
}

void ChromeBrowserMainParts::ProfileInitManager::OnProfileManagerDestroying() {
  profile_manager_observer_.Reset();
}

// BrowserMainParts ------------------------------------------------------------

ChromeBrowserMainParts::ChromeBrowserMainParts(bool is_integration_test,
                                               StartupData* startup_data)
    : is_integration_test_(is_integration_test), startup_data_(startup_data) {
  DCHECK(startup_data_);
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (is_integration_test_) {
    extensions::ComponentLoader::DisableHelpAppForTesting();
  }
#endif
}

ChromeBrowserMainParts::~ChromeBrowserMainParts() {
  // Delete parts in the reverse of the order they were added.
  while (!chrome_extra_parts_.empty())
    chrome_extra_parts_.pop_back();
}

void ChromeBrowserMainParts::SetupMetrics() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::SetupMetrics");
  CHECK(metrics::SubprocessMetricsProvider::CreateInstance());
  metrics::MetricsService* metrics = browser_process_->metrics_service();
  metrics->GetSyntheticTrialRegistry()->AddObserver(
      variations::VariationsIdsProvider::GetInstance());
  metrics->GetSyntheticTrialRegistry()->AddObserver(
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance());
  // TODO(crbug.com/40946277): Investiagte the reason why the mojo connection
  // is often created and closed for the same render process on lacros-chrome.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  synthetic_trial_syncer_ = content::SyntheticTrialSyncer::Create(
      metrics->GetSyntheticTrialRegistry());
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Now that field trials have been created, initializes metrics recording.
  metrics->InitializeMetricsRecordingState();

  startup_data_->chrome_feature_list_creator()
      ->browser_field_trials()
      ->RegisterSyntheticTrials();
}

// static
void ChromeBrowserMainParts::StartMetricsRecording() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::StartMetricsRecording");

  // Register synthetic field trials for the sampling profiler configurations
  // that were already chosen.
  std::string trial_name, group_name;
  if (ThreadProfilerConfiguration::Get()->GetSyntheticFieldTrial(&trial_name,
                                                                 &group_name)) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(trial_name,
                                                              group_name);
  }
  auto* heap_profiler_controller =
      heap_profiling::HeapProfilerController::GetInstance();
  if (heap_profiler_controller &&
      heap_profiler_controller->GetSyntheticFieldTrial(trial_name,
                                                       group_name)) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        trial_name, group_name,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
  }

#if BUILDFLAG(IS_ANDROID)
  // Android updates the metrics service dynamically depending on whether the
  // application is in the foreground or not. Do not start here unless
  // kUmaBackgroundSessions is enabled.
  if (!base::FeatureList::IsEnabled(chrome::android::kUmaBackgroundSessions))
    return;
#endif

  g_browser_process->metrics_service()->CheckForClonedInstall();

#if BUILDFLAG(IS_WIN)
  // The last live timestamp is used to assess whether a browser crash occurred
  // due to a full system crash. Update the last live timestamp on a slow
  // schedule to get the bast possible accuracy for the assessment.
  g_browser_process->metrics_service()->StartUpdatingLastLiveTimestamp();
#endif

  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
}

void ChromeBrowserMainParts::RecordBrowserStartupTime() {
  // Don't record any metrics if UI was displayed before this point e.g.
  // warning dialogs or browser was started in background mode.
  if (startup_metric_utils::GetBrowser().WasMainWindowStartupInterrupted()) {
    return;
  }

  bool is_first_run = false;
#if !BUILDFLAG(IS_ANDROID)
  // On Android, first run is handled in Java code, and the C++ side of Chrome
  // doesn't know if this is the first run. This will cause some inaccuracy in
  // the UMA statistics, but this should be minor (first runs are rare).
  is_first_run = first_run::IsChromeFirstRun();
#endif  // BUILDFLAG(IS_ANDROID)

  // Record collected startup metrics.
  startup_metric_utils::GetBrowser().RecordBrowserMainMessageLoopStart(
      base::TimeTicks::Now(), is_first_run);
}

// -----------------------------------------------------------------------------
// TODO(viettrungluu): move more/rest of BrowserMain() into BrowserMainParts.

#if BUILDFLAG(IS_WIN)
#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT void __cdecl RelaunchChromeBrowserWithNewCommandLineIfNeeded();
}

DLLEXPORT void __cdecl RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  // Need an instance of AtExitManager to handle singleton creations and
  // deletions.  We need this new instance because, the old instance created
  // in ChromeMain() got destructed when the function returned.
  base::AtExitManager exit_manager;
  upgrade_util::RelaunchChromeBrowserWithNewCommandLineIfNeeded();
}
#endif

// content::BrowserMainParts implementation ------------------------------------

int ChromeBrowserMainParts::PreEarlyInitialization() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreEarlyInitialization");
  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PreEarlyInitialization();

  // Create BrowserProcess in PreEarlyInitialization() so that we can load
  // field trials (and all it depends upon).
  browser_process_ = std::make_unique<BrowserProcessImpl>(startup_data_);

#if BUILDFLAG(IS_ANDROID)
  startup_data_->CreateProfilePrefService();
#endif

  bool failed_to_load_resource_bundle = false;
  const int load_local_state_result =
      OnLocalStateLoaded(&failed_to_load_resource_bundle);

  // Reuses the MetricsServicesManager and GetMetricsServicesManagerClient
  // instances created in the FeatureListCreator so they won't be created
  // again.
  auto* chrome_feature_list_creator =
      startup_data_->chrome_feature_list_creator();
  browser_process_->SetMetricsServices(
      chrome_feature_list_creator->TakeMetricsServicesManager(),
      chrome_feature_list_creator->GetMetricsServicesManagerClient());

  if (load_local_state_result == chrome::RESULT_CODE_MISSING_DATA &&
      failed_to_load_resource_bundle) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kNoErrorDialogs)) {
      return chrome::RESULT_CODE_MISSING_DATA;
    }
    // Continue on and show error later (once UI has been initialized and main
    // message loop is running).
    return content::RESULT_CODE_NORMAL_EXIT;
  }

#if BUILDFLAG(IS_WIN)
  // If we are running stale binaries then relaunch and exit immediately.
  if (upgrade_util::IsRunningOldChrome()) {
    if (!upgrade_util::RelaunchChromeBrowser(
            *base::CommandLine::ForCurrentProcess())) {
      // The relaunch failed. Feel free to panic now.
      NOTREACHED_IN_MIGRATION();
    }

    // Note, cannot return RESULT_CODE_NORMAL_EXIT here as this code needs to
    // result in browser startup bailing.
    return chrome::RESULT_CODE_NORMAL_EXIT_UPGRADE_RELAUNCHED;
  }
#endif  // BUILDFLAG(IS_WIN)

  return load_local_state_result;
}

void ChromeBrowserMainParts::PostEarlyInitialization() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostEarlyInitialization");
  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PostEarlyInitialization();
}

void ChromeBrowserMainParts::ToolkitInitialized() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::ToolkitInitialized");

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->ToolkitInitialized();

  // Comes after the extra parts' calls since on GTK that builds the native
  // theme that, in turn, adds the GTK core color mixer; core mixers should all
  // be added before we add chrome mixers.
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(color::AddComponentsColorMixers));
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(AddChromeColorMixers));

  InitializeActionIdStringMapping();
}

void ChromeBrowserMainParts::PreCreateMainMessageLoop() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreCreateMainMessageLoop");

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PreCreateMainMessageLoop();

#if BUILDFLAG(ENABLE_UPDATER)
  updater::SchedulePeriodicTasks();
#endif  // BUILDFLAG(ENABLE_UPDATER)
}

void ChromeBrowserMainParts::PostCreateMainMessageLoop() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostCreateMainMessageLoop");

#if !BUILDFLAG(IS_ANDROID)
  // Initialize the upgrade detector here after `ChromeBrowserMainPartsAsh`
  // has had a chance to connect the DBus services.
  UpgradeDetector::GetInstance()->Init();
#endif

  sampling_profiler::ThreadProfiler::SetMainThreadTaskRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // TODO(sebmarchand): Allow this to be created earlier if startup tracing is
  // enabled.
  trace_event_system_stats_monitor_ =
      std::make_unique<tracing::TraceEventSystemStatsMonitor>();

  // device_event_log must be initialized after the message loop. Calls to
  // {DEVICE}_LOG prior to here will only be logged with VLOG. Some
  // platforms (e.g. chromeos) may have already initialized this.
  if (!device_event_log::IsInitialized())
    device_event_log::Initialize(0 /* default max entries */);

  // Set up and register ERP reporting client.
  reporting_client_ =
      reporting::ReportingClient::Create(content::GetUIThreadTaskRunner({}));

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PostCreateMainMessageLoop();
}

int ChromeBrowserMainParts::PreCreateThreads() {
  // IMPORTANT
  // Calls in this function should not post tasks or create threads as
  // components used to handle those tasks are not yet available. This work
  // should be deferred to PreMainMessageLoopRunImpl.

  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreCreateThreads");
  result_code_ = PreCreateThreadsImpl();

  if (result_code_ == content::RESULT_CODE_NORMAL_EXIT) {
    // These members must be initialized before exiting this function normally.
#if !BUILDFLAG(IS_ANDROID)
    DCHECK(browser_creator_.get());
#endif
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
    DCHECK(master_prefs_.get());
#endif

    for (auto& chrome_extra_part : chrome_extra_parts_)
      chrome_extra_part->PreCreateThreads();
  }

  // Create an instance of GpuModeManager to watch gpu mode pref change.
  g_browser_process->gpu_mode_manager();

  return result_code_;
}

int ChromeBrowserMainParts::OnLocalStateLoaded(
    bool* failed_to_load_resource_bundle) {
  *failed_to_load_resource_bundle = false;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_))
    return chrome::RESULT_CODE_MISSING_DATA;

  auto* platform_management_service =
      policy::ManagementServiceFactory::GetForPlatform();
  platform_management_service->UsePrefServiceAsCache(
      browser_process_->local_state());
  platform_management_service->RefreshCache(base::NullCallback());

#if BUILDFLAG(IS_WIN)
  if (first_run::IsChromeFirstRun()) {
    bool stats_default;
    if (GoogleUpdateSettings::GetCollectStatsConsentDefault(&stats_default)) {
      // |stats_default| == true means that the default state of consent for the
      // product at the time of install was to report usage statistics, meaning
      // "opt-out".
      metrics::RecordMetricsReportingDefaultState(
          browser_process_->local_state(),
          stats_default ? metrics::EnableMetricsDefault::OPT_OUT
                        : metrics::EnableMetricsDefault::OPT_IN);
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  std::string locale =
      startup_data_->chrome_feature_list_creator()->actual_locale();
  if (locale.empty()) {
    *failed_to_load_resource_bundle = true;
    return chrome::RESULT_CODE_MISSING_DATA;
  }
  browser_process_->SetApplicationLocale(locale);

  const int apply_first_run_result = ApplyFirstRunPrefs();
  if (apply_first_run_result != content::RESULT_CODE_NORMAL_EXIT)
    return apply_first_run_result;

  embedder_support::OriginTrialsSettingsStorage*
      origin_trials_settings_storage =
          browser_process_->GetOriginTrialsSettingsStorage();
  embedder_support::SetupOriginTrialsCommandLineAndSettings(
      browser_process_->local_state(), origin_trials_settings_storage);
  blink::OriginTrialsSettingsProvider::Get()->SetSettings(
      origin_trials_settings_storage->GetSettings());

  metrics::EnableExpiryChecker(chrome_metrics::kExpiredHistogramsHashes);

  return content::RESULT_CODE_NORMAL_EXIT;
}

int ChromeBrowserMainParts::ApplyFirstRunPrefs() {
// Android does first run in Java instead of native.
// Chrome OS has its own out-of-box-experience code.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  master_prefs_ = std::make_unique<first_run::MasterPrefs>();

  std::unique_ptr<installer::InitialPreferences> installer_initial_prefs =
      startup_data_->chrome_feature_list_creator()->TakeInitialPrefs();
  if (!installer_initial_prefs)
    return content::RESULT_CODE_NORMAL_EXIT;

  // On first run, we need to process the predictor preferences before the
  // browser's profile_manager object is created, but after ResourceBundle
  // is initialized.
  first_run::ProcessInitialPreferencesResult pip_result =
      first_run::ProcessInitialPreferences(user_data_dir_,
                                           std::move(installer_initial_prefs),
                                           master_prefs_.get());
  if (pip_result == first_run::EULA_EXIT_NOW)
    return chrome::RESULT_CODE_EULA_REFUSED;

  // TODO(macourteau): refactor preferences that are copied from
  // master_preferences into local_state, as a "local_state" section in
  // initial preferences. If possible, a generic solution would be preferred
  // over a copy one-by-one of specific preferences. Also see related TODO
  // in first_run.h.

  PrefService* local_state = g_browser_process->local_state();
  if (!master_prefs_->suppress_default_browser_prompt_for_version.empty()) {
    local_state->SetString(
        prefs::kBrowserSuppressDefaultBrowserPrompt,
        master_prefs_->suppress_default_browser_prompt_for_version);
  }
#if BUILDFLAG(IS_MAC)
  if (!master_prefs_->confirm_to_quit) {
    local_state->SetBoolean(prefs::kConfirmToQuitEnabled,
                            master_prefs_->confirm_to_quit);
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  return content::RESULT_CODE_NORMAL_EXIT;
}

int ChromeBrowserMainParts::PreCreateThreadsImpl() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreCreateThreadsImpl");

  if (browser_process_->GetApplicationLocale().empty()) {
    ShowMissingLocaleMessageBox();
    return chrome::RESULT_CODE_MISSING_DATA;
  }

#if !BUILDFLAG(IS_ANDROID)
  MaybeShowInvalidUserDataDirWarningDialog();
#endif  // !BUILDFLAG(IS_ANDROID)

  DCHECK(!user_data_dir_.empty());

  // Force MediaCaptureDevicesDispatcher to be created on UI thread.
  MediaCaptureDevicesDispatcher::GetInstance();

  // Android's first run is done in Java instead of native.
#if !BUILDFLAG(IS_ANDROID)
  // Cache first run state early.
  first_run::IsChromeFirstRun();
#endif  // !BUILDFLAG(IS_ANDROID)

  PrefService* local_state = browser_process_->local_state();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  browser_process_->platform_part()->InitializeCrosSettings();
  ash::StatsReportingController::Initialize(local_state);
  arc::StabilityMetricsManager::Initialize(local_state);
  ash::HWDataUsageController::Initialize(local_state);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  {
    TRACE_EVENT0(
        "startup",
        "ChromeBrowserMainParts::PreCreateThreadsImpl:InitBrowserProcessImpl");
    browser_process_->Init();
  }

#if !BUILDFLAG(IS_ANDROID)
  // These members must be initialized before returning from this function.
  // Android doesn't use StartupBrowserCreator.
  browser_creator_ = std::make_unique<StartupBrowserCreator>();
  // TODO(yfriedman): Refactor Android to re-use UMABrowsingActivityObserver
  chrome::UMABrowsingActivityObserver::Init();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_WIN)

  // Reset the command line in the crash report details, since we may have
  // just changed it to include experiments.
  crash_keys::SetCrashKeysFromCommandLine(
      *base::CommandLine::ForCurrentProcess());

  browser_process_->browser_policy_connector()->OnResourceBundleCreated();

// Android does first run in Java instead of native.
// Chrome OS has its own out-of-box-experience code.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (first_run::IsChromeFirstRun()) {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kApp) &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppId)) {
      browser_creator_->AddFirstRunTabs(master_prefs_->new_tabs);
    }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Create directory for user-level Native Messaging manifest files. This
    // makes it less likely that the directory will be created by third-party
    // software with incorrect owner or permission. See crbug.com/725513 .
    base::FilePath user_native_messaging_dir;
    CHECK(base::PathService::Get(chrome::DIR_USER_NATIVE_MESSAGING,
                                 &user_native_messaging_dir));
    if (!base::PathExists(user_native_messaging_dir))
      base::CreateDirectory(user_native_messaging_dir);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#if defined(ARCH_CPU_X86_64)
  // The use of Rosetta to run the x64 version of Chromium on Arm is neither
  // tested nor maintained, and there are reports of it crashing in weird ways
  // (e.g. https://crbug.com/1305353). Warn the user if this is the case, as
  // it's almost certainly accidental on their part.
  if (base::mac::GetCPUType() == base::mac::CPUType::kTranslatedIntel) {
    LOG(ERROR) << "The use of Rosetta to run the x64 version of Chromium on "
                  "Arm is neither tested nor maintained, and unexpected "
                  "behavior will likely result. Please check that all tools "
                  "that spawn Chromium are Arm-native.";
    base::PlatformThread::Sleep(base::Seconds(3));
  }
#endif  // defined(ARCH_CPU_X86_64)

  // Get the Keychain API to register for distributed notifications on the main
  // thread, which has a proper CFRunloop, instead of later on the I/O thread,
  // which doesn't. This ensures those notifications will get delivered
  // properly. See issue 37766.
  // (Note that the callback mask here is empty. I don't want to register for
  // any callbacks, I just want to initialize the mechanism.)

  // Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
  // Removal of its use is tracked in https://crbug.com/1348251 but deprecation
  // warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  SecKeychainAddCallback(&KeychainCallback, 0, nullptr);
#pragma clang diagnostic pop

#endif  // BUILDFLAG(IS_MAC)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  metrics::DesktopSessionDurationTracker::Initialize();
  ProfileActivityMetricsRecorder::Initialize();
  TouchModeStatsTracker::Initialize(
      metrics::DesktopSessionDurationTracker::Get(),
      ui::TouchUiController::Get());
#endif

  // Add Site Isolation switches as dictated by policy.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (local_state->GetBoolean(prefs::kSitePerProcess) &&
      site_isolation::SiteIsolationPolicy::IsEnterprisePolicyApplicable() &&
      !command_line->HasSwitch(switches::kSitePerProcess)) {
    command_line->AppendSwitch(switches::kSitePerProcess);
  }
  // IsolateOrigins policy is taken care of through SiteIsolationPrefsObserver
  // (constructed and owned by BrowserProcessImpl).

  // We need to set the policy for data URLs since they can be created in
  // any process. The ChromeContentBrowserClient will take care of child
  // processes.
  if (!local_state->GetBoolean(prefs::kDataURLWhitespacePreservationEnabled) &&
      !command_line->HasSwitch(net::kRemoveWhitespaceForDataURLs)) {
    command_line->AppendSwitch(net::kRemoveWhitespaceForDataURLs);
  }

#if BUILDFLAG(IS_ANDROID)
  // The admin should also be able to use these policies to force Site Isolation
  // off (on Android; using enterprise policies to disable Site Isolation is not
  // supported on other platforms).  Note that disabling either SitePerProcess
  // or IsolateOrigins via policy will disable both types of isolation.
  if ((local_state->IsManagedPreference(prefs::kSitePerProcess) &&
       !local_state->GetBoolean(prefs::kSitePerProcess)) ||
      (local_state->IsManagedPreference(prefs::kIsolateOrigins) &&
       local_state->GetString(prefs::kIsolateOrigins).empty())) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableSiteIsolationForPolicy);
  }
#endif

  // ChromeOS needs ui::ResourceBundle::InitSharedInstance to be called before
  // this.
  browser_process_->PreCreateThreads();

  // This must occur in PreCreateThreads() because it initializes global state
  // which is then read by all threads without synchronization. It must be after
  // browser_process_->PreCreateThreads() as that instantiates the IOThread
  // which is used in SetupMetrics().
  SetupMetrics();

  return content::RESULT_CODE_NORMAL_EXIT;
}

void ChromeBrowserMainParts::PostCreateThreads() {
  // This task should be posted after the IO thread starts, and prior to the
  // base version of the function being invoked. It is functionally okay to post
  // this task in method ChromeBrowserMainParts::BrowserThreadsStarted() which
  // we also need to add in this class, and call this method at the very top of
  // BrowserMainLoop::InitializeMainThread(). PostCreateThreads is preferred to
  // BrowserThreadsStarted as it matches the PreCreateThreads and CreateThreads
  // stages.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&sampling_profiler::ThreadProfiler::StartOnChildThread,
                     sampling_profiler::ProfilerThreadType::kIo));
// Sampling multiple threads might cause overhead on Android and we don't want
// to enable it unless the data is needed.
#if !BUILDFLAG(IS_ANDROID)
  // We pass in CreateCoreUnwindersFactory here since it lives in the chrome/
  // layer while TracingSamplerProfiler is outside of chrome/.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&tracing::TracingSamplerProfiler::
                         CreateOnChildThreadWithCustomUnwinders,
                     base::BindRepeating(&CreateCoreUnwindersFactory)));
#endif

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
  ChromeProcessSingleton::GetInstance()->StartWatching();
#endif

  tracing::SetupSystemTracingFromFieldTrial();
  tracing::SetupBackgroundTracingFromCommandLine();
  tracing::SetupPresetTracingFromFieldTrial();
  base::trace_event::EmitNamedTrigger(
      base::trace_event::kStartupTracingTriggerName);

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PostCreateThreads();
}

int ChromeBrowserMainParts::PreMainMessageLoopRun() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreMainMessageLoopRun");

  result_code_ = PreMainMessageLoopRunImpl();

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PreMainMessageLoopRun();

  return result_code_;
}

// PreMainMessageLoopRun calls these extra stages in the following order:
//  PreMainMessageLoopRunImpl()
//   ... initial setup.
//   PreProfileInit()
//   ... additional setup, including CreateProfile()
//   PostProfileInit()
//   ... additional setup
//   PreBrowserStart()
//   ... browser_creator_->Start
//   PostBrowserStart()

void ChromeBrowserMainParts::PreProfileInit() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreProfileInit");

  media::AudioManager::SetGlobalAppName(
      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME));

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PreProfileInit();

  DisallowKeyedServiceFactoryRegistration();

#if !BUILDFLAG(IS_ANDROID)
  // Ephemeral profiles may have been left behind if the browser crashed.
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .CleanUpEphemeralProfiles();
  // Files of deleted profiles can also be left behind after a crash.
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .CleanUpDeletedProfiles();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  javascript_dialog_extensions_client::InstallClient();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kViewsJSAppModalDialog))
    InstallChromeJavaScriptAppModalDialogViewFactory();
  else
    InstallChromeJavaScriptAppModalDialogViewCocoaFactory();
#else
  InstallChromeJavaScriptAppModalDialogViewFactory();
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  SetChromeAppModalDialogManagerDelegate();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  media_router::ChromeMediaRouterFactory::DoPlatformInit();
}

void ChromeBrowserMainParts::CallPostProfileInit(Profile* profile) {
  DCHECK(profile);
  bool is_initial_profile = !initialized_initial_profile_;
  initialized_initial_profile_ = true;

  PostProfileInit(profile, is_initial_profile);
}

void ChromeBrowserMainParts::PostProfileInit(Profile* profile,
                                             bool is_initial_profile) {
  if (is_initial_profile) {
    TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostProfileInit");
  }

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PostProfileInit(profile, is_initial_profile);

#if BUILDFLAG(IS_WIN)
  // Verify that the profile is not on a network share and if so prepare to show
  // notification to the user.
  if (NetworkProfileBubble::ShouldCheckNetworkProfile(profile)) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&NetworkProfileBubble::CheckNetworkProfile,
                       profile->GetPath()));
  }

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Create the spellcheck service. This will asynchronously retrieve the
  // Windows platform spellcheck dictionary language tags used to populate the
  // context menu for editable content.
  if (spellcheck::UseBrowserSpellChecker() &&
      profile->GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable) &&
      !base::FeatureList::IsEnabled(
          spellcheck::kWinDelaySpellcheckServiceInit)) {
    SpellcheckServiceFactory::GetForContext(profile);
  }
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  // Delete the media history database if it still exists.
  // TODO(crbug.com/40177301): Remove this.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DeleteMediaHistoryDatabase, profile->GetPath()));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  if (ShouldInstallSodaDuringPostProfileInit(
          *base::CommandLine::ForCurrentProcess(), profile)) {
    speech::SodaInstaller::GetInstance()->Init(profile->GetPrefs(),
                                               browser_process_->local_state());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_RLZ) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_initial_profile) {
    // Init the RLZ library. This just binds the dll and schedules a task on the
    // file thread to be run sometime later. If this is the first run we record
    // the installation event.
    int ping_delay =
        profile->GetPrefs()->GetInteger(prefs::kRlzPingDelaySeconds);
    // Negative ping delay means to send ping immediately after a first search
    // is recorded.
    rlz::RLZTracker::SetRlzDelegate(
        std::make_unique<ChromeRLZTrackerDelegate>());
    rlz::RLZTracker::InitRlzDelayed(
        first_run::IsChromeFirstRun(), ping_delay < 0,
        base::Seconds(abs(ping_delay)),
        ChromeRLZTrackerDelegate::IsGoogleDefaultSearch(profile),
        ChromeRLZTrackerDelegate::IsGoogleHomepage(profile),
        ChromeRLZTrackerDelegate::IsGoogleInStartpages(profile));
  }
#endif  // BUILDFLAG(ENABLE_RLZ) && !BUILDFLAG(IS_CHROMEOS_ASH)

  language::LanguageUsageMetrics::RecordAcceptLanguages(
      profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages));
  translate::TranslateMetricsLoggerImpl::LogApplicationStartMetrics(
      ChromeTranslateClient::CreateTranslatePrefs(profile->GetPrefs()));
// On ChromeOS results in a crash. https://crbug.com/1151558
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  language::LanguageUsageMetrics::RecordPageLanguages(
      *UrlLanguageHistogramFactory::GetForBrowserContext(profile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (headless::IsHeadlessMode()) {
    headless::ReportHeadlessActionMetrics();
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

void ChromeBrowserMainParts::PreBrowserStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreBrowserStart");
  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PreBrowserStart();

#if !BUILDFLAG(IS_ANDROID)
  // Start the tab manager here so that we give the most amount of time for the
  // other services to start up before we start adjusting the oom priority.
  g_browser_process->GetTabManager()->Start();

  if (base::FeatureList::IsEnabled(features::kReportPakFileIntegrity)) {
    CheckPakFileIntegrity();
  }
#endif

  // The RulesetService will make the filtering rules available to renderers
  // immediately after its construction, provided that the rules are already
  // available at no cost in an indexed format. This enables activating
  // subresource filtering, if needed, also for page loads on start-up.
  g_browser_process->subresource_filter_ruleset_service();
  // Also enable subresource filtering for fingerprinting protection.
  g_browser_process->fingerprinting_protection_ruleset_service();
}

void ChromeBrowserMainParts::PostBrowserStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostBrowserStart");
  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PostBrowserStart();

  browser_process_->browser_policy_connector()->OnBrowserStarted();

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
  // Allow ProcessSingleton to process messages.
  // This is done here instead of just relying on the main message loop's start
  // to avoid rendezvous in RunLoops that may precede MainMessageLoopRun.
  ChromeProcessSingleton::GetInstance()->Unlock(base::BindRepeating(
      &ChromeBrowserMainParts::ProcessSingletonNotificationCallback));
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)

  // Set up a task to delete old WebRTC log files for all profiles. Use a delay
  // to reduce the impact on startup time.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUtil::DeleteOldWebRtcLogFilesForAllProfiles),
      base::Minutes(1));

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebUsb)) {
    web_usb_detector_ = std::make_unique<WebUsbDetector>();
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&WebUsbDetector::Initialize,
                                  base::Unretained(web_usb_detector_.get())));
  }
#endif

  // At this point, StartupBrowserCreator::Start has run creating initial
  // browser windows and tabs, but no progress has been made in loading
  // content as the main message loop hasn't started processing tasks yet.
  // We setup to observe to the initial page load here to defer running
  // task posted via PostAfterStartupTask until its complete.
  AfterStartupTaskUtils::StartMonitoringStartup();
}

int ChromeBrowserMainParts::PreMainMessageLoopRunImpl() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreMainMessageLoopRunImpl");

  SCOPED_UMA_HISTOGRAM_LONG_TIMER("Startup.PreMainMessageLoopRunImplLongTime");

#if BUILDFLAG(IS_WIN)
  // Windows parental controls calls can be slow, so we do an early init here
  // that calculates this value off of the UI thread.
  InitializeWinParentalControls();
#endif

  // Now that the file thread has been started, start metrics.
  StartMetricsRecording();

  // Do any initializating in the browser process that requires all threads
  // running.
  browser_process_->PreMainMessageLoopRun();

#if BUILDFLAG(IS_WIN)
  // If the command line specifies 'uninstall' then we need to work here
  // unless we detect another chrome browser running.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUninstall)) {
    return DoUninstallTasks(browser_util::IsBrowserAlreadyRunning());
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHideIcons) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowIcons)) {
    return ChromeBrowserMainPartsWin::HandleIconsCommands(
        *base::CommandLine::ForCurrentProcess());
  }

  ui::SelectFileDialog::SetFactory(
      std::make_unique<ChromeSelectFileDialogFactory>());
#endif  // BUILDFLAG(IS_WIN)

  // In headless mode provide alternate SelectFileDialog factory overriding
  // any platform specific SelectFileDialog implementation that may have been
  // set.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (headless::IsHeadlessMode()) {
    headless::HeadlessSelectFileDialogFactory::SetUp();
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(CHROME_FOR_TESTING)
  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kChromeForTestingAllowed)) {
    LOG(ERROR) << "Chrome for Testing is disallowed by the system admin.";
    return static_cast<int>(chrome::RESULT_CODE_ACTION_DISALLOWED_BY_POLICY);
  }
#endif  // BUILDFLAG(CHROME_FOR_TESTING)

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMakeDefaultBrowser)) {
    bool is_managed = g_browser_process->local_state()->IsManagedPreference(
        prefs::kDefaultBrowserSettingEnabled);
    if (is_managed && !g_browser_process->local_state()->GetBoolean(
        prefs::kDefaultBrowserSettingEnabled)) {
      return static_cast<int>(chrome::RESULT_CODE_ACTION_DISALLOWED_BY_POLICY);
    }

    return shell_integration::SetAsDefaultBrowser()
               ? static_cast<int>(content::RESULT_CODE_NORMAL_EXIT)
               : static_cast<int>(chrome::RESULT_CODE_SHELL_INTEGRATION_FAILED);
  }

#if defined(USE_AURA)
  // Make sure aura::Env has been initialized.
  CHECK(aura::Env::GetInstance());
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_WIN)
  // We must call DoUpgradeTasks now that we own the browser singleton to
  // finish upgrade tasks (swap) and relaunch if necessary.
  if (upgrade_util::DoUpgradeTasks(*base::CommandLine::ForCurrentProcess()))
    return chrome::RESULT_CODE_NORMAL_EXIT_UPGRADE_RELAUNCHED;
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  // Begin relaunch processing immediately if User Data migration is required
  // to handle a version downgrade.
  if (downgrade_manager_.PrepareUserDataDirectoryForCurrentVersion(
          user_data_dir_)) {
    return chrome::RESULT_CODE_DOWNGRADE_AND_RELAUNCH;
  }
  downgrade_manager_.UpdateLastVersion(user_data_dir_);
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)

#if !BUILDFLAG(IS_CHROMEOS)
  // Initialize the chrome browser cloud management controller after
  // the browser process singleton is acquired to remove race conditions where
  // multiple browser processes start simultaneously.  The main
  // initialization of browser_policy_connector is performed inside
  // PreMainMessageLoopRun() so that policies can be applied as soon as
  // possible.
  //
  // Note that this protects against multiple browser process starts in
  // the same user data dir and not multiple starts across user data dirs.
  browser_process_->browser_policy_connector()->InitCloudManagementController(
      browser_process_->local_state(),
      browser_process_->system_network_context_manager()
          ->GetSharedURLLoaderFactory());
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Wait for the chrome browser cloud management enrollment to finish.
  // If enrollment is not mandatory, this function returns immediately.
  // Abort the launch process if required enrollment fails.
  if (!browser_process_->browser_policy_connector()
           ->chrome_browser_cloud_management_controller()
           ->WaitUntilPolicyEnrollmentFinished()) {
    return chrome::RESULT_CODE_CLOUD_POLICY_ENROLLMENT_FAILED;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  // Check if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, we do not
  // allow the user level Chrome to run. So we notify the user and uninstall
  // user level Chrome.
  // Note this check needs to happen here (after the process singleton was
  // obtained but before potentially creating the first run sentinel).
  if (ChromeBrowserMainPartsWin::CheckMachineLevelInstall())
    return chrome::RESULT_CODE_MACHINE_LEVEL_INSTALL_EXISTS;
#endif  // BUILDFLAG(IS_WIN)

  // Desktop construction occurs here, (required before profile creation).
  PreProfileInit();

#if BUILDFLAG(ENABLE_NACL)
  // NaClBrowserDelegateImpl is accessed inside CreateInitialProfile().
  // So make sure to create it before that.
  nacl::NaClBrowser::SetDelegate(std::make_unique<NaClBrowserDelegateImpl>(
      browser_process_->profile_manager()));
#endif  // BUILDFLAG(ENABLE_NACL)

  // This step is costly and is already measured in Startup.CreateFirstProfile
  // and more directly Profile.CreateAndInitializeProfile.
  StartupProfileInfo profile_info = CreateInitialProfile(
      user_data_dir_, *base::CommandLine::ForCurrentProcess());
  base::UmaHistogramEnumeration(
      "ProfilePicker.StartupMode.CreateInitialProfile", profile_info.mode);

  if (profile_info.mode == StartupProfileMode::kError)
    return content::RESULT_CODE_NORMAL_EXIT;

#if !BUILDFLAG(IS_ANDROID)
  // The first run sentinel must be created after the process singleton was
  // grabbed (where enabled) and no early return paths were otherwise hit above.
  first_run::CreateSentinelIfNeeded();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Autoload any profiles which are running background apps.
  // TODO(rlp): Do this on a separate thread. See http://crbug.com/99075.
  browser_process_->profile_manager()->AutoloadProfiles();
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Post-profile init ---------------------------------------------------------

  TranslateService::Initialize();
  if (base::FeatureList::IsEnabled(features::kGeoLanguage) ||
      language::GetOverrideLanguageModel() ==
          language::OverrideLanguageModel::GEO) {
    language::GeoLanguageProvider::GetInstance()->StartUp(
        browser_process_->local_state());
  }

  // Needs to be done before PostProfileInit, since login manager on CrOS is
  // called inside PostProfileInit.
  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());
  RegisterChromeWebUIConfigs();
  RegisterChromeUntrustedWebUIConfigs();

#if BUILDFLAG(IS_ANDROID)
  page_info::SetPageInfoClient(new ChromePageInfoClient());
#endif

  // Needs to be done before PostProfileInit, to allow connecting DevTools
  // before WebUI for the CrOS login that can be called inside PostProfileInit
  g_browser_process->CreateDevToolsProtocolHandler();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kAutoOpenDevToolsForTabs))
    g_browser_process->CreateDevToolsAutoOpener();

  // Needs to be done before PostProfileInit, since the SODA Installer setup is
  // called inside PostProfileInit and depends on it.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableComponentUpdate)) {
    component_updater::RegisterComponentsForUpdate();
  }

  // `profile` may be nullptr if the profile picker is shown.
  Profile* profile = profile_info.profile;
  // Call `PostProfileInit()`and set it up for profiles created later.
  profile_init_manager_ = std::make_unique<ProfileInitManager>(this, profile);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Execute first run specific code after the PrefService has been initialized
  // and preferences have been registered since some of the import code depends
  // on preferences.
  if (first_run::IsChromeFirstRun()) {
    // `profile` may be nullptr even on first run, for example when the
    // "BrowserSignin" policy is set to "Force". If so, skip the auto import.
    if (profile) {
      first_run::AutoImport(profile, master_prefs_->import_bookmarks_path);
    }

    // Note: This can pop-up the first run consent dialog on Linux & Mac.
    first_run::DoPostImportTasks(master_prefs_->make_chrome_default_for_user);

    // The first run dialog is modal, and spins a RunLoop, which could receive
    // a SIGTERM, and call chrome::AttemptExit(). Exit cleanly in that case.
    if (browser_shutdown::IsTryingToQuit())
      return content::RESULT_CODE_NORMAL_EXIT;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
  // Sets things up so that if we crash from this point on, a dialog will
  // popup asking the user to restart chrome. It is done this late to avoid
  // testing against a bunch of special cases that are taken care early on.
  ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment(
      *base::CommandLine::ForCurrentProcess());

  // Registers Chrome with the Windows Restart Manager, which will restore the
  // Chrome session when the computer is restarted after a system update.
  // This could be run as late as WM_QUERYENDSESSION for system update reboots,
  // but should run on startup if extended to handle crashes/hangs/patches.
  // Also, better to run once here than once for each HWND's WM_QUERYENDSESSION.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBrowserTest)) {
    ChromeBrowserMainPartsWin::RegisterApplicationRestart(
        *base::CommandLine::ForCurrentProcess());
  }
#endif  // BUILDFLAG(IS_WIN)

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(ChromeNetResourceProvider);
  media::SetLocalizedStringProvider(ChromeMediaLocalizedStringProvider);

#if !BUILDFLAG(IS_ANDROID)
  // In unittest mode, this will do nothing.  In normal mode, this will create
  // the global IntranetRedirectDetector instance, which will promptly go to
  // sleep for seven seconds (to avoid slowing startup), and wake up afterwards
  // to see if it should do anything else.
  //
  // A simpler way of doing all this would be to have some function which could
  // give the time elapsed since startup, and simply have this object check that
  // when asked to initialize itself, but this doesn't seem to exist.
  //
  // This can't be created in the BrowserProcessImpl constructor because it
  // needs to read prefs that get set after that runs.
  browser_process_->intranet_redirect_detector();
#endif

#if BUILDFLAG(ENABLE_PDF)
  chrome_pdf::features::SetIsOopifPdfPolicyEnabled(
      g_browser_process->local_state()->GetBoolean(
          prefs::kPdfViewerOutOfProcessIframeEnabled));
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugPrint)) {
    base::FilePath path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kDebugPrint);
    if (!path.empty())
      printing::PrintedDocument::SetDebugDumpPath(path);
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializeProcessForPrinting();

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::ShouldEarlyStartPrintBackendService()) {
    printing::EarlyStartPrintBackendService();
  }
#endif
#endif  // BUILDFLAG(ENABLE_PRINTING)

  HandleTestParameters(*base::CommandLine::ForCurrentProcess());

  // This has to come before the first GetInstance() call. PreBrowserStart()
  // seems like a reasonable place to put this, except on Android,
  // OfflinePageInfoHandler::Register() below calls GetInstance().
  // TODO(thestig): See if the Android code below can be moved to later.
  sessions::ContentSerializedNavigationDriver::SetInstance(
      ChromeSerializedNavigationDriver::GetInstance());

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageInfoHandler::Register();
#endif

#if BUILDFLAG(ENABLE_NACL)
  nacl::NaClProcessHost::EarlyStartup();
#endif  // BUILDFLAG(ENABLE_NACL)

  PreBrowserStart();

  variations::VariationsService* variations_service =
      browser_process_->variations_service();
  variations_service->PerformPreMainMessageLoopStartup();

#if BUILDFLAG(IS_ANDROID)
  // The profile picker is never shown on Android.
  DCHECK_EQ(profile_info.mode, StartupProfileMode::kBrowserWindow);
  DCHECK(profile);
  // Just initialize the policy prefs service here. Variations seed fetching
  // will be initialized when the app enters foreground mode.
  variations_service->set_policy_pref_service(profile->GetPrefs());
#else
  // We are in regular browser boot sequence. Open initial tabs and enter the
  // main message loop.
  std::vector<Profile*> last_opened_profiles;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS multiple profiles doesn't apply, and will break if we load
  // them this early as the cryptohome hasn't yet been mounted (which happens
  // only once we log in). And if we're launching a web app, we don't want to
  // restore the last opened profiles.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppId)) {
    last_opened_profiles =
        g_browser_process->profile_manager()->GetLastOpenedProfiles();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  // This step is costly.
  if (browser_creator_->Start(*base::CommandLine::ForCurrentProcess(),
                              base::FilePath(), profile_info,
                              last_opened_profiles)) {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    // Initialize autoupdate timer. Timer callback costs basically nothing
    // when browser is not in persistent mode, so it's OK to let it ride on
    // the main thread. This needs to be done here because we don't want
    // to start the timer when Chrome is run inside a test harness.
    browser_process_->StartAutoupdateTimer();
#endif  // BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    // On Linux, the running exe will be updated if an upgrade becomes
    // available while the browser is running.  We need to save the last
    // modified time of the exe, so we can compare to determine if there is
    // an upgrade while the browser is kept alive by a persistent extension.
    upgrade_util::SaveLastModifiedTimeOfExe();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

    // Record now as the last successful chrome start.
    if constexpr (kShouldRecordActiveUse) {
      GoogleUpdateSettings::SetLastRunTime();
    }

    // Create the RunLoop for MainMessageLoopRun() to use and transfer
    // ownership of the browser's lifetime to the BrowserProcess.
    DCHECK(!GetMainRunLoopInstance());
    GetMainRunLoopInstance() = std::make_unique<base::RunLoop>();
    browser_process_->SetQuitClosure(
        GetMainRunLoopInstance()->QuitWhenIdleClosure());
  }
  browser_creator_.reset();
#endif  // !BUILDFLAG(IS_ANDROID)

  PostBrowserStart();

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  // Clean up old user data directory, snapshots and disk cache directory.
  downgrade_manager_.DeleteMovedUserDataSoon(user_data_dir_);
#endif

  // This should be invoked as close as possible to the start of the browser's
  // main loop, but before the end of PreMainMessageLoopRun in order for
  // browser tests (which InterceptMainMessageLoopRun rather than
  // MainMessageLoopRun) to be able to see its side-effect.
  if (result_code_ <= 0)
    RecordBrowserStartupTime();

  return result_code_;
}

#if !BUILDFLAG(IS_ANDROID)
bool ChromeBrowserMainParts::ShouldInterceptMainMessageLoopRun() {
  // Some early return paths in PreMainMessageLoopRunImpl intentionally prevent
  // the main run loop from being created. Use this as a signal to indicate that
  // the main message loop shouldn't be run.
  return !!GetMainRunLoopInstance();
}
#endif

void ChromeBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
#if BUILDFLAG(IS_ANDROID)
  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop
  NOTREACHED_IN_MIGRATION();
#else
  DCHECK(base::CurrentUIThread::IsSet());

  run_loop = std::move(GetMainRunLoopInstance());

  // Trace the entry and exit of this main message loop. We don't use the
  // TRACE_EVENT_BEGIN0 macro because the tracing infrastructure doesn't expect
  // a synchronous event around the main loop of a thread.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "toplevel", "ChromeBrowserMainParts::MainMessageLoopRun", this);
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserMainParts::OnFirstIdle() {
  startup_metric_utils::GetBrowser().RecordBrowserMainLoopFirstIdle(
      base::TimeTicks::Now());
#if BUILDFLAG(IS_ANDROID)
  sharing::ShareHistory::CreateForProfile(
      ProfileManager::GetPrimaryUserProfile());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // If OneGroupPerRenderer feature is enabled, post a task to clean any left
  // over cgroups due to any unclean exits.
  if (base::FeatureList::IsEnabled(base::kOneGroupPerRenderer)) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&base::Process::CleanUpStaleProcessStates));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ChromeBrowserMainParts::PostMainMessageLoopRun() {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "toplevel", "ChromeBrowserMainParts::MainMessageLoopRun", this);
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostMainMessageLoopRun");
#if BUILDFLAG(IS_ANDROID)
  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop
  NOTREACHED_IN_MIGRATION();
#else
  // Shutdown the UpgradeDetector here before `ChromeBrowserMainPartsAsh`
  // disconnects DBus services in its PostDestroyThreads.
  UpgradeDetector::GetInstance()->Shutdown();

  // Start watching hangs up to the end of the process.
  StartWatchingForProcessShutdownHangs();

  // Two different types of hang detection cannot attempt to upload crashes at
  // the same time or they would interfere with each other. Do not start the
  // ShutdownWatcher if the HangWatcher is already collecting crash.
  // TODO(crbug.com/40840897): Migrate away from ShutdownWatcher and its old
  // timing.
  if (!base::HangWatcher::IsCrashReportingEnabled()) {
    // Start watching for jank during shutdown. It gets disarmed when
    // |shutdown_watcher_| object is destructed.
    constexpr base::TimeDelta kShutdownHangDelay{base::Seconds(300)};
    shutdown_watcher_ = std::make_unique<ShutdownWatcherHelper>();
    shutdown_watcher_->Arm(kShutdownHangDelay);
  }

  web_usb_detector_.reset();

  for (auto& chrome_extra_part : chrome_extra_parts_)
    chrome_extra_part->PostMainMessageLoopRun();

  TranslateService::Shutdown();

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
  ChromeProcessSingleton::GetInstance()->Cleanup();
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)

  browser_process_->metrics_service()->Stop();

  // BrowserProcessImpl::StartTearDown() makes SyntheticTrialRegistry
  // unavailable. Since SyntheticTrialSyncer depends on SyntheticTrialRegistry,
  // destroy before the tear-down.
  synthetic_trial_syncer_.reset();

  restart_last_session_ = browser_shutdown::ShutdownPreThreadsStop();
  browser_process_->StartTearDown();
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserMainParts::PostDestroyThreads() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, there is no quit/exit. So the browser's main message loop will
  // not finish.
  NOTREACHED_IN_MIGRATION();
#else

  for (auto& chrome_extra_part : chrome_extra_parts_) {
    chrome_extra_part->PostDestroyThreads();
  }

  browser_shutdown::RestartMode restart_mode =
      browser_shutdown::RestartMode::kNoRestart;

  if (restart_last_session_) {
    restart_mode = browser_shutdown::RestartMode::kRestartLastSession;

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
    if (BackgroundModeManager::should_restart_in_background())
      restart_mode = browser_shutdown::RestartMode::kRestartInBackground;
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
  }

  browser_process_->PostDestroyThreads();

  // We need to do this call as late as possible, but due to modularity, this
  // may be the last point in Chrome. This would be more effective if done at a
  // higher level on the stack, so that it is impossible for an early return to
  // bypass this code. Perhaps we need a *final* hook that is called on all
  // paths from content/browser/browser_main.
  //
  // Since we use |browser_process_|'s local state for this call, it must be
  // done before |browser_process_| is released.
  metrics::Shutdown(browser_process_->local_state());

  profile_init_manager_.reset();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // These controllers make use of `browser_process_->local_state()`, so they
  // must be destroyed before `browser_process_`.
  // Shutting down in the reverse order of Initialize().
  ash::HWDataUsageController::Shutdown();
  arc::StabilityMetricsManager::Shutdown();
  ash::StatsReportingController::Shutdown();
  browser_process_->platform_part()->ShutdownCrosSettings();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  if (result_code_ == chrome::RESULT_CODE_DOWNGRADE_AND_RELAUNCH) {
    // Process a pending User Data downgrade before restarting.
    downgrade_manager_.ProcessDowngrade(user_data_dir_);

    // It's impossible for there to also be a user-driven relaunch since the
    // browser never fully starts in this case.
    DCHECK(!restart_last_session_);
    restart_mode = browser_shutdown::RestartMode::kRestartThisSession;
  }
#endif  // BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)

  // From this point, the BrowserProcess class is no longer alive.
  browser_process_.reset();

  browser_shutdown::ShutdownPostThreadsStop(restart_mode);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  master_prefs_.reset();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  device_event_log::Shutdown();
#endif  // BUILDFLAG(IS_ANDROID)
}

// Public members:

void ChromeBrowserMainParts::AddParts(
    std::unique_ptr<ChromeBrowserMainExtraParts> parts) {
  chrome_extra_parts_.push_back(std::move(parts));
}

#if !BUILDFLAG(IS_ANDROID)
// static
std::unique_ptr<base::RunLoop> ChromeBrowserMainParts::TakeRunLoopForTest() {
  DCHECK(GetMainRunLoopInstance());
  return std::move(GetMainRunLoopInstance());
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
// static
bool ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
    base::CommandLine command_line,
    const base::FilePath& current_directory) {
  // Drop the request if the browser process is already shutting down.
  // Note that we're going to post an async task below. Even if the browser
  // process isn't shutting down right now, it could be by the time the task
  // starts running. So, an additional check needs to happen when it starts.
  // But regardless of any future check, there is no reason to post the task
  // now if we know we're already shutting down.
  if (!g_browser_process || g_browser_process->IsShuttingDown()) {
    return false;
  }

  // Drop the request if this or the requesting process is running with
  // automation enabled to avoid hard to cope with startup races.
  if (command_line.HasSwitch(switches::kEnableAutomation) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    return false;
  }

  // Drop the request if headless mode is in effect or the request is from
  // a headless Chrome process.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (headless::IsHeadlessMode() ||
      command_line.HasSwitch(switches::kHeadless)) {
    return false;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  // In order to handle this request on Windows, there is platform specific
  // code in browser_finder.cc that requires making outbound COM calls to
  // cross-apartment shell objects (via IVirtualDesktopManager). That is not
  // allowed within a SendMessage handler, which this function is a part of.
  // So, we post a task to asynchronously finish the command line processing.
  return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ProcessSingletonNotificationCallbackImpl,
                                std::move(command_line), current_directory));
}
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)
