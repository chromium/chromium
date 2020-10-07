// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager_factory.h"

#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

VmCameraMicManager* VmCameraMicManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<VmCameraMicManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

VmCameraMicManagerFactory* VmCameraMicManagerFactory::GetInstance() {
  static base::NoDestructor<VmCameraMicManagerFactory> factory;
  return factory.get();
}

VmCameraMicManagerFactory::VmCameraMicManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "VmCameraMicManager",
          BrowserContextDependencyManager::GetInstance()) {}

VmCameraMicManagerFactory::~VmCameraMicManagerFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* VmCameraMicManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (ProfileHelper::IsPrimaryProfile(profile))
    return new VmCameraMicManager(profile);
  return nullptr;
}

}  // namespace chromeos
