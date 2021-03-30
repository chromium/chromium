// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/browser_policy_connector.h"

#if defined(OS_ANDROID)
#include "components/policy/core/browser/android/policy_cache_updater_android.h"
#endif

class PrefService;

namespace policy {
class ConfigurationPolicyProvider;

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeBrowserCloudManagementController;
class MachineLevelUserCloudPolicyManager;
#endif

// Extends BrowserPolicyConnector with the setup shared among the desktop
// implementations and Android.
class ChromeBrowserPolicyConnector : public BrowserPolicyConnector {
 public:
  // Service initialization delay time in millisecond on startup. (So that
  // displaying Chrome's GUI does not get delayed.)
  static const int64_t kServiceInitializationStartupDelay = 5000;

  // Builds an uninitialized ChromeBrowserPolicyConnector, suitable for testing.
  // Init() should be called to create and start the policy machinery.
  ChromeBrowserPolicyConnector();

  ~ChromeBrowserPolicyConnector() override;

  // Called once the resource bundle has been created. Calls through to super
  // class to notify observers.
  void OnResourceBundleCreated();

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  bool IsEnterpriseManaged() const override;

  bool HasMachineLevelPolicies() override;

  void Shutdown() override;

  ConfigurationPolicyProvider* GetPlatformProvider();

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  ChromeBrowserCloudManagementController*
  chrome_browser_cloud_management_controller() {
    return chrome_browser_cloud_management_controller_.get();
  }
  MachineLevelUserCloudPolicyManager*
  machine_level_user_cloud_policy_manager() {
    return machine_level_user_cloud_policy_manager_;
  }
#endif

  // BrowserPolicyConnector:
  // Command line switch only supports Dev and Canary channel, trunk and browser
  // tests on Win, Mac, Linux and Android.
  bool IsCommandLineSwitchSupported() const override;

  static void EnableCommandLineSupportForTesting();

  virtual base::flat_set<std::string> device_affiliation_ids() const;

 protected:
  // BrowserPolicyConnectorBase::
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

 private:
  std::unique_ptr<ConfigurationPolicyProvider> CreatePlatformProvider();

  // Owned by base class.
  ConfigurationPolicyProvider* platform_provider_ = nullptr;

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ChromeBrowserCloudManagementController>
      chrome_browser_cloud_management_controller_;
  // Owned by base class.
  MachineLevelUserCloudPolicyManager* machine_level_user_cloud_policy_manager_ =
      nullptr;
#endif

  ConfigurationPolicyProvider* command_line_provider_ = nullptr;

#if defined(OS_ANDROID)
  std::unique_ptr<android::PolicyCacheUpdater> pollicy_cache_updater_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserPolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
