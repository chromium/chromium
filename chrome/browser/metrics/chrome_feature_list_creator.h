// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_FEATURE_LIST_CREATOR_H_
#define CHROME_BROWSER_METRICS_CHROME_FEATURE_LIST_CREATOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_browser_field_trials.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/installer/util/initial_preferences.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"

namespace ash {
class ChromeBrowserMainPartsAsh;
}  // namespace ash

class ChromeMetricsServicesManagerClient;

// The ChromeFeatureListCreator creates the FeatureList and classes required for
// setting up field trials, e.g. VariationsService, MetricsServicesManager etc.
// before the full browser loop starts. The |local_state| is instantiated, and
// its ownership will be taken by BrowserProcessImpl when the full browser
// starts. Note: On Chrome OS, this class depends on BrowserPolicyConnectorAsh
// whose behavior depends on DBusThreadManager being initialized.
class ChromeFeatureListCreator {
 public:
  ChromeFeatureListCreator();

  ChromeFeatureListCreator(const ChromeFeatureListCreator&) = delete;
  ChromeFeatureListCreator& operator=(const ChromeFeatureListCreator&) = delete;

  ~ChromeFeatureListCreator();

  // Initializes all necessary parameters to create the feature list and calls
  // base::FeatureList::SetInstance() to set the global instance.
  void CreateFeatureList();

  // Sets the application locale and verifies (via a CHECK) that it matches
  // what was used when creating field trials.
  void SetApplicationLocale(const std::string& locale);

  // Overrides cached UI strings on the resource bundle once it is initialized.
  void OverrideCachedUIStrings();

  // Gets the MetricsServicesManagerClient* used in this class.
  metrics_services_manager::MetricsServicesManagerClient*
  GetMetricsServicesManagerClient();

  // Passes ownership of the |local_state_| to the caller.
  std::unique_ptr<PrefService> TakePrefService();

  // Passes ownership of the |metrics_services_manager_| to the caller.
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
  TakeMetricsServicesManager();

  // Passes ownership of the |browser_policy_connector_| to the caller.
  std::unique_ptr<policy::ChromeBrowserPolicyConnector>
  TakeChromeBrowserPolicyConnector();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<installer::InitialPreferences> TakeInitialPrefs();
#endif

  PrefService* local_state() { return local_state_.get(); }
  policy::ChromeBrowserPolicyConnector* browser_policy_connector() {
    return browser_policy_connector_.get();
  }
  const std::string& actual_locale() { return actual_locale_; }

  ChromeBrowserFieldTrials* browser_field_trials() {
    return browser_field_trials_.get();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Get the FeatureList::Accessor, clearing immediately -- this must only be
  // used by ChromeBrowserMainPartsAsh.
  std::unique_ptr<base::FeatureList::Accessor> GetAndClearFeatureListAccessor(
      base::PassKey<ash::ChromeBrowserMainPartsAsh> key) {
    return std::move(cros_feature_list_accessor_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  void CreatePrefService();
  void ConvertFlagsToSwitches();

  // Sets up the field trials and related initialization. Call only after
  // about:flags have been converted to switches. However,
  // |command_line_variation_ids| should be the value of the
  // "--force-variation-ids" switch before it is mutated. See
  // VariationsFieldTrialCreator::SetUpFieldTrials() for the format of
  // |command_line_variation_ids|.
  void SetUpFieldTrials(const std::string& command_line_variation_ids);

  void CreateMetricsServices();

  // Imports variations initial preference any preferences (to local state)
  // needed for first run. This is always called and early outs if not
  // first-run.
  void SetupInitialPrefs();

  // Must be destroyed after |local_state_|.
  std::unique_ptr<policy::ChromeBrowserPolicyConnector>
      browser_policy_connector_;

  // If TakePrefService() is called, the caller will take the ownership
  // of this variable. Stop using this variable afterwards.
  std::unique_ptr<PrefService> local_state_;

  // The locale used by the application. It is set when initializing the
  // ResouceBundle.
  std::string actual_locale_;

  // This is owned by |metrics_services_manager_| but we need to expose it.
  raw_ptr<ChromeMetricsServicesManagerClient, AcrossTasksDanglingUntriaged>
      metrics_services_manager_client_;

  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  std::unique_ptr<ChromeBrowserFieldTrials> browser_field_trials_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<installer::InitialPreferences> installer_initial_prefs_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the platform needs to be able to access the
  // FeatureList::Accessor. On other platforms, this API should not be used.
  std::unique_ptr<base::FeatureList::Accessor> cros_feature_list_accessor_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

#endif  // CHROME_BROWSER_METRICS_CHROME_FEATURE_LIST_CREATOR_H_
