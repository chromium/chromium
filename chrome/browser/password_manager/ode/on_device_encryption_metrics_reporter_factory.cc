// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/on_device_encryption_metrics_reporter_factory.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/password_manager/core/browser/ode/password_trusted_vault_on_device_encryption_state_tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/service/sync_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/ode/chrome_passkey_on_device_encryption_state_tracker.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#endif

namespace password_manager {

// static
OnDeviceEncryptionMetricsReporter*
OnDeviceEncryptionMetricsReporterFactory::GetForProfile(Profile* profile) {
  return static_cast<OnDeviceEncryptionMetricsReporter*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
OnDeviceEncryptionMetricsReporterFactory*
OnDeviceEncryptionMetricsReporterFactory::GetInstance() {
  static base::NoDestructor<OnDeviceEncryptionMetricsReporterFactory> instance;
  return instance.get();
}

OnDeviceEncryptionMetricsReporterFactory::
    OnDeviceEncryptionMetricsReporterFactory()
    : ProfileKeyedServiceFactory("OnDeviceEncryptionMetricsReporter",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(SyncServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(PasskeyModelFactory::GetInstance());
  DependsOn(EnclaveManagerFactory::GetInstance());
#endif
}

OnDeviceEncryptionMetricsReporterFactory::
    ~OnDeviceEncryptionMetricsReporterFactory() = default;

std::unique_ptr<KeyedService>
OnDeviceEncryptionMetricsReporterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/540854648): Implement passkey tracker on Android.
  // Until passkeys state can be observed on Android, use a no-op tracker
  // which remains in kOnDeviceEncryptionStateNotAvailable and does not publish
  // passkey metrics.
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
#else
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile);
  EnclaveManagerInterface* enclave_manager =
      EnclaveManagerFactory::GetForProfile(profile);

  auto passkey_tracker =
      std::make_unique<ChromePasskeyOnDeviceEncryptionStateTracker>(
          sync_service, enclave_manager, passkey_model);
#endif

  auto password_tracker =
      std::make_unique<PasswordTrustedVaultOnDeviceEncryptionStateTracker>(
          sync_service);

  return std::make_unique<OnDeviceEncryptionMetricsReporter>(
      std::move(passkey_tracker), std::move(password_tracker),
      CHECK_DEREF(profile->GetPrefs()));
}

void OnDeviceEncryptionMetricsReporterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  OnDeviceEncryptionMetricsReporter::RegisterProfilePrefs(registry);
}

bool OnDeviceEncryptionMetricsReporterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool OnDeviceEncryptionMetricsReporterFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}

}  // namespace password_manager
