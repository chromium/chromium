// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/receiver/boca_receiver_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/boca/receiver/boca_receiver_service.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
BocaReceiverServiceFactory* BocaReceiverServiceFactory::GetInstance() {
  static base::NoDestructor<BocaReceiverServiceFactory> instance;
  return instance.get();
}

// static
BocaReceiverService* BocaReceiverServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BocaReceiverService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

BocaReceiverServiceFactory::BocaReceiverServiceFactory()
    : ProfileKeyedServiceFactory(
          "BocaReceiverService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
}

BocaReceiverServiceFactory::~BocaReceiverServiceFactory() = default;

std::unique_ptr<KeyedService>
BocaReceiverServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<BocaReceiverService>(profile);
}

}  // namespace ash
