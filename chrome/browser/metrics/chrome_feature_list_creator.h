// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_FEATURE_LIST_CREATOR_H_
#define CHROME_BROWSER_METRICS_CHROME_FEATURE_LIST_CREATOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_browser_field_trials.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/installer/util/initial_preferences.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"

class ChromeMetricsServicesManagerClient;

// The ChromeFeatureListCreator creates the FeatureList and classes required for
// setting up field trials, e.g. VariationsService, MetricsServicesManager etc.
// before the full browser loop starts. The |local_state| is instantiated, and
// its ownership will be taken by BrowserProcessImpl when the full browser
// starts. Note: On Chrome OS, this class depends on
// BrowserPolicyConnectorChromeOS whose behavior depends on DBusThreadManager
// being initialized.
class ChromeFeatureListCreator {
 public:
  ChromeFeatureListCreator();
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

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
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

 private:
  void CreatePrefService();
  void ConvertFlagsToSwitches();
  void SetupFieldTrials();
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
  ChromeMetricsServicesManagerClient* metrics_services_manager_client_;

  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  std::unique_ptr<ChromeBrowserFieldTrials> browser_field_trials_;

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<installer::InitialPreferences> installer_initial_prefs_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeFeatureListCreator);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_FEATURE_LIST_CREATOR_H_
