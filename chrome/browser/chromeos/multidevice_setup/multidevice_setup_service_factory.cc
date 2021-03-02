// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/ash/authpolicy/authpolicy_credentials_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_pairing_state_tracker_impl.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/cryptauth/gcm_device_info_provider_impl.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/auth_token_validator_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/auth_token_validator_impl.h"
#include "chrome/browser/chromeos/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace multidevice_setup {

namespace {

// Class that wraps MultiDeviceSetupService in a KeyedService.
class MultiDeviceSetupServiceHolder : public KeyedService {
 public:
  explicit MultiDeviceSetupServiceHolder(content::BrowserContext* context)
      : profile_(Profile::FromBrowserContext(context)) {
    const user_manager::User* user =
        ProfileHelper::Get()->GetUserByProfile(profile_);
    const user_manager::User* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();

    DCHECK(user);
    DCHECK(primary_user);

    bool is_secondary_user =
        user->GetAccountId() != primary_user->GetAccountId();

    android_sms::AndroidSmsService* android_sms_service =
        chromeos::android_sms::AndroidSmsServiceFactory::GetForBrowserContext(
            context);
    multidevice_setup_service_ = std::make_unique<MultiDeviceSetupService>(
        profile_->GetPrefs(),
        device_sync::DeviceSyncClientFactory::GetForProfile(profile_),
        AuthTokenValidatorFactory::GetForProfile(profile_),
        OobeCompletionTrackerFactory::GetForProfile(profile_),
        android_sms_service ? android_sms_service->android_sms_app_manager()
                            : nullptr,
        android_sms_service
            ? android_sms_service->android_sms_pairing_state_tracker()
            : nullptr,
        GcmDeviceInfoProviderImpl::GetInstance(), is_secondary_user);
  }

  MultiDeviceSetupService* multidevice_setup_service() {
    return multidevice_setup_service_.get();
  }

 private:
  // KeyedService:
  void Shutdown() override { multidevice_setup_service_.reset(); }

  Profile* const profile_;
  std::unique_ptr<MultiDeviceSetupService> multidevice_setup_service_;
};

}  // namespace

// static
MultiDeviceSetupService* MultiDeviceSetupServiceFactory::GetForProfile(
    Profile* profile) {
  if (!profile)
    return nullptr;

  MultiDeviceSetupServiceHolder* holder =
      static_cast<MultiDeviceSetupServiceHolder*>(
          MultiDeviceSetupServiceFactory::GetInstance()
              ->GetServiceForBrowserContext(profile, true));
  if (!holder)
    return nullptr;

  return holder->multidevice_setup_service();
}

// static
MultiDeviceSetupServiceFactory* MultiDeviceSetupServiceFactory::GetInstance() {
  static base::NoDestructor<MultiDeviceSetupServiceFactory> factory;
  return factory.get();
}

MultiDeviceSetupServiceFactory::MultiDeviceSetupServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MultiDeviceSetupService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(AuthTokenValidatorFactory::GetInstance());
  DependsOn(OobeCompletionTrackerFactory::GetInstance());
  DependsOn(android_sms::AndroidSmsServiceFactory::GetInstance());
}

MultiDeviceSetupServiceFactory::~MultiDeviceSetupServiceFactory() = default;

KeyedService* MultiDeviceSetupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
          Profile::FromBrowserContext(context)->GetPrefs())) {
    return nullptr;
  }

  return new MultiDeviceSetupServiceHolder(context);
}

}  // namespace multidevice_setup
}  // namespace chromeos
