// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/policy/core/browser/android/policy_cache_updater_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/device_settings_lacros.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class PrefService;

namespace policy {
class ConfigurationPolicyProvider;
class LocalTestPolicyProvider;
class ProxyPolicyProvider;

#if !BUILDFLAG(IS_CHROMEOS)
class ChromeBrowserCloudManagementController;
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
  ChromeBrowserPolicyConnector(const ChromeBrowserPolicyConnector&) = delete;
  ChromeBrowserPolicyConnector& operator=(const ChromeBrowserPolicyConnector&) =
      delete;
  ~ChromeBrowserPolicyConnector() override;

  // Called once the resource bundle has been created. Calls through to super
  // class to notify observers.
  void OnResourceBundleCreated();

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  // Called to signal the browser has started.
  virtual void OnBrowserStarted();

  bool IsDeviceEnterpriseManaged() const override;

  bool HasMachineLevelPolicies() override;

  void Shutdown() override;

  ConfigurationPolicyProvider* GetPlatformProvider();

  ConfigurationPolicyProvider* local_test_policy_provider();
  void SetLocalTestPolicyProviderForTesting(
      ConfigurationPolicyProvider* provider);

  // If the kLocalTestPoliciesForNextStartup pref is non-empty, read and apply
  // the policies stored in it, and then clear the pref. This must be called
  // right after the `local_state` is created to ensure policies are applied
  // at the right time.
  void MaybeApplyLocalTestPolicies(PrefService* local_state);

#if !BUILDFLAG(IS_CHROMEOS)
  ChromeBrowserCloudManagementController*
  chrome_browser_cloud_management_controller() {
    return chrome_browser_cloud_management_controller_.get();
  }

  // On non-Android platforms, starts controller initialization right away.
  // On Android, delays controller initialization until platform policies have
  // been initialized, or starts controller initialization right away otherwise.
  void InitCloudManagementController(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(chromium:1502062): MachineLevelUserCloudPolicyManager is CBCM's policy
  // provider. Since CBCM doesn't exist in Lacros,
  // MachineLevelUserCloudPolicyManager shouldn't have to exist there either.
  // Refactor the code accordingly.
  MachineLevelUserCloudPolicyManager*
  machine_level_user_cloud_policy_manager() {
    return machine_level_user_cloud_policy_manager_;
  }
  void SetMachineLevelUserCloudPolicyManagerForTesting(
      MachineLevelUserCloudPolicyManager* manager);

  ProxyPolicyProvider* proxy_policy_provider() {
    return proxy_policy_provider_;
  }

  ConfigurationPolicyProvider* command_line_policy_provider() {
    return command_line_provider_;
  }

  // Set ProxyPolicyProvider for testing, caller needs to init and shutdown the
  // provider.
  void SetProxyPolicyProviderForTesting(
      ProxyPolicyProvider* proxy_policy_provider);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // BrowserPolicyConnector:
  // Command line switch only supports Dev and Canary channel, trunk and
  // browser tests on Win, Mac, Linux and Android.
  bool IsCommandLineSwitchSupported() const override;

  static void EnableCommandLineSupportForTesting();

  virtual base::flat_set<std::string> device_affiliation_ids() const;
  void SetDeviceAffiliatedIdsForTesting(
      const base::flat_set<std::string>& device_affiliation_ids);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Checks if the main / primary user is managed or not.
  // TODO(crbug.com/40788404): Remove once Lacros handles all profiles the same
  // way.
  bool IsMainUserManaged() const;

  // The device settings used in Lacros.
  crosapi::mojom::DeviceSettings* GetDeviceSettings() const;

  DeviceSettingsLacros* device_settings_lacros() {
    return device_settings_.get();
  }

  PolicyLoaderLacros* device_account_policy_loader() {
    return device_account_policy_loader_;
  }

  ConfigurationPolicyProvider* ash_policy_provider() {
    return ash_policy_provider_;
  }
#endif

 protected:
  // BrowserPolicyConnectorBase::
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

  base::flat_set<std::string> device_affiliation_ids_for_testing_;

 private:
  // Returns the policy provider that supplies platform policies.
  std::unique_ptr<ConfigurationPolicyProvider> CreatePlatformProvider();

#if !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ChromeBrowserCloudManagementController>
      chrome_browser_cloud_management_controller_;

  // Creates the MachineLevelUserCloudPolicyManager if the browser should be
  // enrolled in CBCM. On Android, the decision may be postponed until platform
  // policies have been loaded and it can be decided if an enrollment token is
  // available or not.
  void MaybeCreateCloudPolicyManager(
      std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>*
          providers);
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Invoked once it can be decided if cloud management is enabled. If enabled,
  // invoked with a MachineLevelUserCloudPolicyManager instance. Otherwise,
  // nullptr is passed on instead.
  void OnMachineLevelCloudPolicyManagerCreated(
      std::unique_ptr<MachineLevelUserCloudPolicyManager>
          machine_level_user_cloud_policy_manager);

  // If CBCM enrollment is needed, then this proxy points to a
  // MachineLevelUserCloudPolicyManager object. Otherwise, this is innocuous.
  // Using a proxy allows unblocking PolicyService creation on platforms where
  // cloud management decision depends on non-CBCM policy providers to be
  // initialized (e.g. Android). Once platform policies are loaded, the proxy
  // can refer to the actual policy manager if cloud management is enabled.
  // Owned by base class.
  raw_ptr<ProxyPolicyProvider> proxy_policy_provider_ = nullptr;

  // The MachineLevelUserCloudPolicyManager is not directly included in the
  // vector of policy providers (defined in the base class). A proxy policy
  // provider allows this object to be initialized after the policy service
  // is created. Owned by the proxy policy provider.
  raw_ptr<MachineLevelUserCloudPolicyManager>
      machine_level_user_cloud_policy_manager_ = nullptr;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<android::PolicyCacheUpdater> policy_cache_updater_;
#endif  // BUILDFLAG(IS_ANDROID)

  // Owned by base class.
  raw_ptr<ConfigurationPolicyProvider> platform_provider_ = nullptr;

  // Owned by base class.
  raw_ptr<ConfigurationPolicyProvider> command_line_provider_ = nullptr;

  raw_ptr<ConfigurationPolicyProvider> local_test_provider_for_testing_ =
      nullptr;
  std::unique_ptr<LocalTestPolicyProvider> local_test_provider_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<DeviceSettingsLacros> device_settings_ = nullptr;
  // Owned by |platform_provider_|.
  raw_ptr<PolicyLoaderLacros, DanglingUntriaged> device_account_policy_loader_ =
      nullptr;
  // Provides the user policy fetched/cached by ash-chrome. Owned by base class.
  raw_ptr<ConfigurationPolicyProvider> ash_policy_provider_ = nullptr;
#endif

  // Weak pointers needed for tasks that need to wait until it can be decided
  // if an enrollment token is available or not.
  base::WeakPtrFactory<ChromeBrowserPolicyConnector> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
