// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_feature_list_creator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/crash_keys.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/variations/feature_list_creator/feature_list_creator.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"
#include "components/variations/variations_ids_provider.h"
#include "components/webui/flags/flags_state.h"
#include "components/webui/flags/flags_storage.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/prefs/android/local_state_prefs_android.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/webui/flags/pref_service_flags_storage.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/first_run/first_run.h"
#include "chrome/installer/util/initial_preferences.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

// static
ChromeFeatureListCreator* ChromeFeatureListCreator::GetInstance() {
  static base::NoDestructor<ChromeFeatureListCreator> instance;
  return instance.get();
}

ChromeFeatureListCreator::~ChromeFeatureListCreator() = default;

void ChromeFeatureListCreator::CreateFeatureList() {
  if (!local_state_) {
    CreatePrefService();
  }
  // Convert active flags to switches before
  // `ui::ResourceBundle::InitSharedInstanceWithLocale()` because experiment
  // flags such as `--touch-optimized-ui` affect loaded resources.
  CHECK(!ui::ResourceBundle::HasSharedInstance(), base::NotFatalUntil::M161);
  variations::CreateFeatureList(*this);
}

variations::VariationsIdsProvider*
ChromeFeatureListCreator::CreateVariationsIdsProvider() {
  CHECK(network_time_tracker_);
  return variations::CreateVariationsIdsProvider(
      variations::VariationsIdsProvider::Mode::kUseSignedInState,
      network_time_tracker_.get());
}

void ChromeFeatureListCreator::SetApplicationLocale(const std::string& locale) {
  actual_locale_ = locale;
  metrics_services_manager_->GetVariationsService()->EnsureLocaleEquals(locale);
}

std::unique_ptr<PrefService> ChromeFeatureListCreator::TakePrefService() {
  return std::move(local_state_);
}

std::unique_ptr<metrics_services_manager::MetricsServicesManager>
ChromeFeatureListCreator::TakeMetricsServicesManager() {
  return std::move(metrics_services_manager_);
}

std::unique_ptr<policy::ChromeBrowserPolicyConnector>
ChromeFeatureListCreator::TakeChromeBrowserPolicyConnector() {
  return std::move(browser_policy_connector_);
}

std::unique_ptr<network_time::NetworkTimeTracker>
ChromeFeatureListCreator::TakeNetworkTimeTracker() {
  CHECK(network_time_tracker_);
  return std::move(network_time_tracker_);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<installer::InitialPreferences>
ChromeFeatureListCreator::TakeInitialPrefs() {
  return std::move(installer_initial_prefs_);
}
#endif

ChromeFeatureListCreator::ChromeFeatureListCreator()
    : network_time_tracker_(
          network_time::NetworkTimeTracker::CreateUninitialized()) {}

void ChromeFeatureListCreator::CreatePrefService() {
  CHECK(!local_state_);
  base::FilePath local_state_file;
  bool result =
      base::PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_file);
  CHECK(result, base::NotFatalUntil::M161);

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  RegisterLocalState(pref_registry.get());

#if BUILDFLAG(IS_CHROMEOS)
  // DBus must be initialized before constructing the policy connector.
  CHECK(ash::DBusThreadManager::IsInitialized());
  browser_policy_connector_ =
      std::make_unique<policy::BrowserPolicyConnectorAsh>();
#else
  browser_policy_connector_ =
      std::make_unique<policy::ChromeBrowserPolicyConnector>();
#endif  // BUILDFLAG(IS_CHROMEOS)

  local_state_ =
      chrome_prefs::CreateLocalState(local_state_file, std::move(pref_registry),
                                     browser_policy_connector_.get());

  // Apply local test policies from the kLocalTestPoliciesForNextStartup pref if
  // there are any.
  browser_policy_connector_->MaybeApplyLocalTestPolicies(local_state_.get());

#if BUILDFLAG(IS_WIN)
  first_run::MaybeSetInitialLocaleFromInstaller(*local_state_);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
  // Make LocalStatePrefs available to Java as soon as `local_state_` is
  // created (before BrowserProcessImpl takes ownership of it).
  chrome_browser_prefs::OnLocalStatePrefsLoaded();
#endif
}

flags_ui::FlagsState& ChromeFeatureListCreator::GetFlagsState() {
  return CHECK_DEREF(about_flags::GetCurrentFlagsState());
}

std::unique_ptr<flags_ui::FlagsStorage>
ChromeFeatureListCreator::CreateFlagsStorage() {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, flags are passed on the command line when Chrome gets launched
  // by session_manager. There are separate sets of flags for the login screen
  // environment and user sessions. session_manager populates the former from
  // signed device settings, while flags for user session are stored in
  // preferences and applied via a chrome restart upon user login, see
  // UserSessionManager::RestartToApplyPerSessionFlagsIfNeed for the latter.
  return std::make_unique<ash::about_flags::ReadOnlyFlagsStorage>(
      base::CommandLine::ForCurrentProcess());
#else
  return std::make_unique<flags_ui::PrefServiceFlagsStorage>(
      local_state_.get());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

crash_keys::SwitchFilterFunction ChromeFeatureListCreator::GetSwitchFilter() {
  return &crash_keys::IsBoringSwitch;
}

metrics_services_manager::MetricsServicesManager&
ChromeFeatureListCreator::GetOrCreateMetricsServicesManager() {
  // `variations::CreateFeatureList()` calls this once, before
  // `TakeMetricsServicesManager()`.
  CHECK(!metrics_services_manager_);

  // `variations::CreateFeatureList()` calls this right after it converts
  // about:flags to switches. Apply command-line switches to `local_state_` now
  // so switches added from about:flags take effect (including during early
  // Android startup).
  local_state_->UpdateCommandLinePrefStore(
      base::MakeRefCounted<ChromeCommandLinePrefStore>(
          base::CommandLine::ForCurrentProcess()));

  std::unique_ptr<variations::SeedResponse> initial_seed;
// Android does first run in Java instead of native.
// ChromeOS has its own out-of-box-experience code.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  if (first_run::IsChromeFirstRun()) {
    installer_initial_prefs_ = first_run::LoadInitialPrefs();
    if (installer_initial_prefs_) {
      initial_seed = first_run::ExtractVariationsSeedFromInitialPrefs(
          *installer_initial_prefs_);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  auto client = std::make_unique<ChromeMetricsServicesManagerClient>(
      local_state_.get(), std::move(initial_seed));
  metrics_services_manager_ =
      std::make_unique<metrics_services_manager::MetricsServicesManager>(
          std::move(client));
  return *metrics_services_manager_;
}

#if BUILDFLAG(IS_CHROMEOS)
void ChromeFeatureListCreator::WillSetUpFieldTrials(
    base::FeatureList& feature_list) {
  // On ChromeOS, the platform needs to be able to access the
  // FeatureList::Accessor. On other platforms, this API should not be used.
  cros_feature_list_accessor_ = feature_list.ConstructAccessor();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

variations::PlatformFieldTrials&
ChromeFeatureListCreator::GetPlatformFieldTrials() {
  return browser_field_trials_;
}

std::vector<base::FeatureList::FeatureOverrideInfo>
ChromeFeatureListCreator::GetExtraFeatureOverrides(
    const base::CommandLine& command_line) {
  return content::GetSwitchDependentFeatureOverrides(command_line);
}
