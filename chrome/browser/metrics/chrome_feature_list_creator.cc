// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_feature_list_creator.h"

#include <set>
#include <vector>

#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_crash_keys.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/owner_flags_storage.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

ChromeFeatureListCreator::ChromeFeatureListCreator() = default;

ChromeFeatureListCreator::~ChromeFeatureListCreator() = default;

void ChromeFeatureListCreator::CreateFeatureList() {
  CreatePrefService();
  ConvertFlagsToSwitches();
  CreateMetricsServices();
  SetupInitialPrefs();
  SetupFieldTrials();
}

void ChromeFeatureListCreator::SetApplicationLocale(const std::string& locale) {
  actual_locale_ = locale;
  metrics_services_manager_->GetVariationsService()->EnsureLocaleEquals(locale);
}

void ChromeFeatureListCreator::OverrideCachedUIStrings() {
  metrics_services_manager_->GetVariationsService()->OverrideCachedUIStrings();
}

metrics_services_manager::MetricsServicesManagerClient*
ChromeFeatureListCreator::GetMetricsServicesManagerClient() {
  return metrics_services_manager_client_;
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

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<installer::InitialPreferences>
ChromeFeatureListCreator::TakeInitialPrefs() {
  return std::move(installer_initial_prefs_);
}
#endif

void ChromeFeatureListCreator::CreatePrefService() {
  base::FilePath local_state_file;
  bool result =
      base::PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_file);
  DCHECK(result);

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  RegisterLocalState(pref_registry.get());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // DBus must be initialized before constructing the policy connector.
  CHECK(chromeos::DBusThreadManager::IsInitialized());
  browser_policy_connector_ =
      std::make_unique<policy::BrowserPolicyConnectorChromeOS>();
#else
  browser_policy_connector_ =
      std::make_unique<policy::ChromeBrowserPolicyConnector>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  local_state_ = chrome_prefs::CreateLocalState(
      local_state_file, browser_policy_connector_->GetPolicyService(),
      std::move(pref_registry), false, browser_policy_connector_.get());

// TODO(asvitkine): This is done here so that the pref is set before
// VariationsService queries the locale. This should potentially be moved to
// somewhere better, e.g. as a helper in first_run namespace.
#if defined(OS_WIN)
  if (first_run::IsChromeFirstRun()) {
    // During first run we read the google_update registry key to find what
    // language the user selected when downloading the installer. This
    // becomes our default language in the prefs.
    // Other platforms obey the system locale.
    std::wstring install_lang;
    if (GoogleUpdateSettings::GetLanguage(&install_lang)) {
      local_state_->SetString(language::prefs::kApplicationLocale,
                              base::WideToASCII(install_lang));
    }
  }
#endif  // defined(OS_WIN)
}

void ChromeFeatureListCreator::ConvertFlagsToSwitches() {
  // Convert active flags into switches. This needs to be done before
  // ui::ResourceBundle::InitSharedInstanceWithLocale as some loaded resources
  // are affected by experiment flags (--touch-optimized-ui in particular).
  DCHECK(!ui::ResourceBundle::HasSharedInstance());
  TRACE_EVENT0("startup", "ChromeFeatureListCreator::ConvertFlagsToSwitches");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, flags are passed on the command line when Chrome gets
  // launched by session_manager. There are separate sets of flags for the login
  // screen environment and user sessions. session_manager populates the former
  // from signed device settings, while flags for user session are stored in
  // preferences and applied via a chrome restart upon user login, see
  // UserSessionManager::RestartToApplyPerSessionFlagsIfNeed for the latter.
  ash::about_flags::ReadOnlyFlagsStorage flags_storage(
      ash::about_flags::ParseFlagsFromCommandLine());
#else
  flags_ui::PrefServiceFlagsStorage flags_storage(local_state_.get());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  about_flags::ConvertFlagsToSwitches(&flags_storage,
                                      base::CommandLine::ForCurrentProcess(),
                                      flags_ui::kAddSentinels);
}

void ChromeFeatureListCreator::SetupFieldTrials() {
  browser_field_trials_ =
      std::make_unique<ChromeBrowserFieldTrials>(local_state_.get());

  // Initialize FieldTrialList to support FieldTrials. If an instance already
  // exists, this is likely a test scenario with a ScopedFeatureList active,
  // so use that one to apply any overrides.
  if (!base::FieldTrialList::GetInstance()) {
    // Note: This is intentionally leaked since it needs to live for the
    // duration of the browser process and there's no benefit in cleaning it up
    // at exit.
    base::FieldTrialList* leaked_field_trial_list = new base::FieldTrialList(
        metrics_services_manager_->CreateEntropyProvider());
    ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
    ignore_result(leaked_field_trial_list);
  }

  auto feature_list = std::make_unique<base::FeatureList>();

  // Associate parameters chosen in about:flags and create trial/group for them.
  flags_ui::PrefServiceFlagsStorage flags_storage(local_state_.get());
  std::vector<std::string> variation_ids =
      about_flags::RegisterAllFeatureVariationParameters(&flags_storage,
                                                         feature_list.get());

  variations::VariationsService* variations_service =
      metrics_services_manager_->GetVariationsService();
  variations_service->SetupFieldTrials(
      cc::switches::kEnableGpuBenchmarking, switches::kEnableFeatures,
      switches::kDisableFeatures, variation_ids,
      content::GetSwitchDependentFeatureOverrides(
          *base::CommandLine::ForCurrentProcess()),
      std::move(feature_list), browser_field_trials_.get());
  variations::InitCrashKeys();
}

void ChromeFeatureListCreator::CreateMetricsServices() {
  auto client =
      std::make_unique<ChromeMetricsServicesManagerClient>(local_state_.get());
  metrics_services_manager_client_ = client.get();
  metrics_services_manager_ =
      std::make_unique<metrics_services_manager::MetricsServicesManager>(
          std::move(client));
}

void ChromeFeatureListCreator::SetupInitialPrefs() {
// Android does first run in Java instead of native.
// Chrome OS has its own out-of-box-experience code.
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // On first run, we need to process the predictor preferences before the
  // browser's profile_manager object is created, but after ResourceBundle
  // is initialized.
  if (!first_run::IsChromeFirstRun())
    return;

  installer_initial_prefs_ = first_run::LoadInitialPrefs();
  if (!installer_initial_prefs_)
    return;

  // Store the initial VariationsService seed in local state, if it exists
  // in master prefs. Note: The getters we call remove them from the installer
  // master prefs, which is why both the seed and signature are retrieved here
  // and not within the ifs below.
  std::string compressed_variations_seed =
      installer_initial_prefs_->GetCompressedVariationsSeed();
  std::string variations_seed_signature =
      installer_initial_prefs_->GetVariationsSeedSignature();

  if (!compressed_variations_seed.empty()) {
    local_state_->SetString(variations::prefs::kVariationsCompressedSeed,
                            compressed_variations_seed);

    if (!variations_seed_signature.empty()) {
      local_state_->SetString(variations::prefs::kVariationsSeedSignature,
                              variations_seed_signature);
    }
    // Set the variation seed date to the current system time. If the user's
    // clock is incorrect, this may cause some field trial expiry checks to
    // not do the right thing until the next seed update from the server,
    // when this value will be updated.
    local_state_->SetInt64(variations::prefs::kVariationsSeedDate,
                           base::Time::Now().ToInternalValue());
  }
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
}
