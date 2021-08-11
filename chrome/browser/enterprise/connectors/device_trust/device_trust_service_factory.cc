// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

// static
DeviceTrustServiceFactory* DeviceTrustServiceFactory::GetInstance() {
  return base::Singleton<DeviceTrustServiceFactory>::get();
}

// static
DeviceTrustService* DeviceTrustServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DeviceTrustService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DeviceTrustServiceFactory::DeviceTrustServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DeviceTrustService",
          BrowserContextDependencyManager::GetInstance()) {}

DeviceTrustServiceFactory::~DeviceTrustServiceFactory() = default;

KeyedService* DeviceTrustServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  std::unique_ptr<AttestationService> attestation_service =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      std::make_unique<AshAttestationService>(profile);
#else
      std::make_unique<DesktopAttestationService>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return new DeviceTrustService(profile->GetPrefs(),
                                std::move(attestation_service),
                                std::make_unique<DeviceTrustSignalReporter>());
}

}  // namespace enterprise_connectors
