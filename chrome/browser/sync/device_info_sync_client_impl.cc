// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/device_info_sync_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync_device_info/device_info_proto_enum_util.h"
#include "device/fido/public/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/android/cable_module_android.h"
#endif

namespace browser_sync {

DeviceInfoSyncClientImpl::DeviceInfoSyncClientImpl(Profile* profile)
    : profile_(profile) {}

DeviceInfoSyncClientImpl::~DeviceInfoSyncClientImpl() = default;

// syncer::DeviceInfoSyncClient:
std::string DeviceInfoSyncClientImpl::GetSigninScopedDeviceId() const {
// Since the local sync backend is currently only supported on Windows, Mac and
// Linux don't even check the pref on other os-es.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  syncer::SyncPrefs prefs(profile_->GetPrefs());
  if (prefs.IsLocalSyncEnabled()) {
    return "local_device";
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  return GetSigninScopedDeviceIdForProfile(profile_);
}

// syncer::DeviceInfoSyncClient:
bool DeviceInfoSyncClientImpl::GetSendTabToSelfReceivingEnabled() const {
  return true;
}

// syncer::DeviceInfoSyncClient:
syncer::DeviceInfo::SendTabReceivingType
DeviceInfoSyncClientImpl::GetSendTabToSelfReceivingType() const {
  return syncer::DeviceInfo::SendTabReceivingType::kChromeOrUnspecified;
}

// syncer::DeviceInfoSyncClient:
std::optional<syncer::DeviceInfo::SharingInfo>
DeviceInfoSyncClientImpl::GetLocalSharingInfo() const {
  return SharingSyncPreference::GetLocalSharingInfoForSync(
      profile_->GetPrefs());
}

// syncer::DeviceInfoSyncClient:
std::optional<std::string> DeviceInfoSyncClientImpl::GetFCMRegistrationToken()
    const {
  return SyncInvalidationsServiceFactory::GetForProfile(profile_)
      ->GetFCMRegistrationToken();
}

// syncer::DeviceInfoSyncClient:
std::optional<syncer::DataTypeSet>
DeviceInfoSyncClientImpl::GetInterestedDataTypes() const {
  return SyncInvalidationsServiceFactory::GetForProfile(profile_)
      ->GetInterestedDataTypes();
}

syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
DeviceInfoSyncClientImpl::GetPhoneAsASecurityKeyInfo() const {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(device::kWebAuthnPublishPrelinkingInfo)) {
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
  }
  return webauthn::authenticator::GetSyncDataIfRegistered();
#else
  return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
#endif
}

bool DeviceInfoSyncClientImpl::IsUmaEnabledOnCrOSDevice() const {
#if BUILDFLAG(IS_CHROMEOS)
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
#else
  return false;
#endif
}

bool DeviceInfoSyncClientImpl::GetDesktopToIOSPromoReceivingEnabled() const {
  return false;
}

MobilePromoOnDesktopPromoTypeSet
DeviceInfoSyncClientImpl::GetDesktopToIOSPromoReceivingTypes() const {
  // This is only required on iOS.
  return {};
}

syncer::DeviceInfo::GlicExperimentalTriggeringState
DeviceInfoSyncClientImpl::GetGlicExperimentalTriggeringState() const {
  auto* service = glic::GlicKeyedService::Get(profile_);
  if (!service) {
    return syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable;
  }
  return service->enabling().GetExperimentalTriggeringState();
}

std::optional<int>
DeviceInfoSyncClientImpl::GetGlicExperimentalTriggeringVersion() const {
  auto* service = glic::GlicKeyedService::Get(profile_);
  if (!service) {
    return std::nullopt;
  }
  return service->enabling().GetExperimentalTriggeringVersion();
}

}  // namespace browser_sync
