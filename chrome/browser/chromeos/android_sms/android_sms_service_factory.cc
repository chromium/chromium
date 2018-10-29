// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

namespace android_sms {

namespace {

bool IsFeatureAllowed(content::BrowserContext* context) {
  return multidevice_setup::IsFeatureAllowed(
      multidevice_setup::mojom::Feature::kMessages,
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace

// static
AndroidSmsServiceFactory* AndroidSmsServiceFactory::GetInstance() {
  static base::NoDestructor<AndroidSmsServiceFactory> factory_instance;
  return factory_instance.get();
}

// static
AndroidSmsService* AndroidSmsServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<AndroidSmsService*>(
      AndroidSmsServiceFactory::GetInstance()->GetServiceForBrowserContext(
          browser_context, true));
}

AndroidSmsServiceFactory::AndroidSmsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AndroidSmsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::multidevice_setup::MultiDeviceSetupClientFactory::
                GetInstance());
}

AndroidSmsServiceFactory::~AndroidSmsServiceFactory() = default;

KeyedService* AndroidSmsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!IsFeatureAllowed(context) ||
      !base::FeatureList::IsEnabled(
          chromeos::features::kAndroidMessagesIntegration) ||
      !base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup) ||
      !base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (ProfileHelper::Get()->GetUserByProfile(profile) == nullptr)
    return nullptr;

  return new AndroidSmsService(context);
}

bool AndroidSmsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AndroidSmsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace android_sms

}  // namespace chromeos
