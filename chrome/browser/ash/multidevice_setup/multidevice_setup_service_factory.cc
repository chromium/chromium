// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/android_sms/android_sms_app_manager.h"
#include "chrome/browser/ash/android_sms/android_sms_pairing_state_tracker_impl.h"
#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ash/cryptauth/gcm_device_info_provider_impl.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/multidevice_setup/auth_token_validator_factory.h"
#include "chrome/browser/ash/multidevice_setup/auth_token_validator_impl.h"
#include "chrome/browser/ash/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {
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
        android_sms::AndroidSmsServiceFactory::GetForBrowserContext(context);
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

  const raw_ptr<Profile> profile_;
  std::unique_ptr<MultiDeviceSetupService> multidevice_setup_service_;
};

}  // namespace

// static
MultiDeviceSetupService* MultiDeviceSetupServiceFactory::GetForProfile(
    Profile* profile) {
  if (!profile) {
    PA_LOG(WARNING) << "Missing profile. Unable to return "
                       "MultiDeviceSetupService, returning nullptr instead";
    return nullptr;
  }

  MultiDeviceSetupServiceHolder* holder =
      static_cast<MultiDeviceSetupServiceHolder*>(
          MultiDeviceSetupServiceFactory::GetInstance()
              ->GetServiceForBrowserContext(profile, true));
  if (!holder) {
    PA_LOG(WARNING)
        << "Missing MultiDeviceSetupServiceHolder. Unable to return "
           "MultiDeviceSetupService, returning nullptr instead.";
    return nullptr;
  }

  return holder->multidevice_setup_service();
}

// static
MultiDeviceSetupServiceFactory* MultiDeviceSetupServiceFactory::GetInstance() {
  static base::NoDestructor<MultiDeviceSetupServiceFactory> factory;
  return factory.get();
}

MultiDeviceSetupServiceFactory::MultiDeviceSetupServiceFactory()
    : ProfileKeyedServiceFactory(
          "MultiDeviceSetupService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(AuthTokenValidatorFactory::GetInstance());
  DependsOn(OobeCompletionTrackerFactory::GetInstance());
  DependsOn(android_sms::AndroidSmsServiceFactory::GetInstance());
}

MultiDeviceSetupServiceFactory::~MultiDeviceSetupServiceFactory() = default;

std::unique_ptr<KeyedService>
MultiDeviceSetupServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
          Profile::FromBrowserContext(context)->GetPrefs())) {
    PA_LOG(WARNING)
        << "No Multidevice Features allowed. Unable to return "
           "MultiDeviceSetupServiceHolder, returning nullptr instead.";
    return nullptr;
  }

  return std::make_unique<MultiDeviceSetupServiceHolder>(context);
}

}  // namespace multidevice_setup
}  // namespace ash
