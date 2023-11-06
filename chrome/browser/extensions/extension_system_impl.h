// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"

class Profile;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace chromeos {
class DeviceLocalAccountManagementPolicyProvider;
class SigninScreenPolicyProvider;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace value_store {
class ValueStoreFactory;
class ValueStoreFactoryImpl;
}  // namespace value_store

namespace extensions {

class ExtensionSystemSharedFactory;
class UninstallPingSender;
class InstallGate;
class ExtensionsPermissionsTracker;

// The ExtensionSystem for ProfileImpl and OffTheRecordProfileImpl.
// Implementation details: non-shared services are owned by
// ExtensionSystemImpl, a KeyedService with separate incognito
// instances. A private Shared class (also a KeyedService,
// but with a shared instance for incognito) keeps the common services.
class ExtensionSystemImpl : public ExtensionSystem {
 public:
  using InstallUpdateCallback = ExtensionSystem::InstallUpdateCallback;

  explicit ExtensionSystemImpl(Profile* profile);

  ExtensionSystemImpl(const ExtensionSystemImpl&) = delete;
  ExtensionSystemImpl& operator=(const ExtensionSystemImpl&) = delete;

  ~ExtensionSystemImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  void InitForRegularProfile(bool extensions_enabled) override;

  ExtensionService* extension_service() override;  // shared
  ManagementPolicy* management_policy() override;  // shared
  ServiceWorkerManager* service_worker_manager() override;  // shared
  UserScriptManager* user_script_manager() override;        // shared
  StateStore* state_store() override;                              // shared
  StateStore* rules_store() override;                              // shared
  StateStore* dynamic_user_scripts_store() override;               // shared
  scoped_refptr<value_store::ValueStoreFactory> store_factory()
      override;                            // shared
  QuotaService* quota_service() override;  // shared
  AppSorting* app_sorting() override;      // shared
  const base::OneShotEvent& ready() const override;
  bool is_ready() const override;
  ContentVerifier* content_verifier() override;  // shared
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const std::string& public_key,
                     const base::FilePath& unpacked_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override;
  void PerformActionBasedOnOmahaAttributes(
      const std::string& extension_id,
      const base::Value::Dict& attributes) override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;

 private:
  friend class ExtensionSystemSharedFactory;

  // Owns the Extension-related systems that have a single instance
  // shared between normal and incognito profiles.
  class Shared : public KeyedService {
   public:
    explicit Shared(Profile* profile);
    ~Shared() override;

    // Initialization takes place in phases.
    virtual void InitPrefs();
    // This must not be called until all the providers have been created.
    void RegisterManagementPolicyProviders();
    void InitInstallGates();
    void Init(bool extensions_enabled);

    // KeyedService implementation.
    void Shutdown() override;

    StateStore* state_store();
    StateStore* rules_store();
    StateStore* dynamic_user_scripts_store();
    scoped_refptr<value_store::ValueStoreFactory> store_factory() const;
    ExtensionService* extension_service();
    ManagementPolicy* management_policy();
    ServiceWorkerManager* service_worker_manager();
    UserScriptManager* user_script_manager();
    QuotaService* quota_service();
    AppSorting* app_sorting();
    const base::OneShotEvent& ready() const { return ready_; }
    bool is_ready() const { return ready_.is_signaled(); }
    ContentVerifier* content_verifier();

   private:
    raw_ptr<Profile> profile_;

    // The services that are shared between normal and incognito profiles.

    std::unique_ptr<StateStore> state_store_;
    std::unique_ptr<StateStore> rules_store_;
    std::unique_ptr<StateStore> dynamic_user_scripts_store_;
    scoped_refptr<value_store::ValueStoreFactoryImpl> store_factory_;
    std::unique_ptr<ServiceWorkerManager> service_worker_manager_;
    // Shared memory region manager for scripts statically declared in extension
    // manifests. This region is shared between all extensions.
    std::unique_ptr<UserScriptManager> user_script_manager_;
    // ExtensionService depends on StateStore and Blocklist.
    std::unique_ptr<ExtensionService> extension_service_;
    std::unique_ptr<ManagementPolicy> management_policy_;
    std::unique_ptr<QuotaService> quota_service_;
    std::unique_ptr<AppSorting> app_sorting_;
    std::unique_ptr<InstallGate> update_install_gate_;

    // For verifying the contents of extensions read from disk.
    scoped_refptr<ContentVerifier> content_verifier_;

    std::unique_ptr<UninstallPingSender> uninstall_ping_sender_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<chromeos::DeviceLocalAccountManagementPolicyProvider>
        device_local_account_management_policy_provider_;
    std::unique_ptr<chromeos::SigninScreenPolicyProvider>
        signin_screen_policy_provider_;
    std::unique_ptr<InstallGate> kiosk_app_update_install_gate_;
    std::unique_ptr<ExtensionsPermissionsTracker>
        extensions_permissions_tracker_;
#endif

    base::OneShotEvent ready_;
  };

  raw_ptr<Profile> profile_;

  raw_ptr<Shared> shared_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_
