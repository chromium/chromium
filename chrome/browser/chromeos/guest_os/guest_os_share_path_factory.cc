// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/guest_os/guest_os_share_path_factory.h"

#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace guest_os {

// static
GuestOsSharePath* GuestOsSharePathFactory::GetForProfile(Profile* profile) {
  return static_cast<GuestOsSharePath*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GuestOsSharePathFactory* GuestOsSharePathFactory::GetInstance() {
  static base::NoDestructor<GuestOsSharePathFactory> factory;
  return factory.get();
}

GuestOsSharePathFactory::GuestOsSharePathFactory()
    : BrowserContextKeyedServiceFactory(
          "GuestOsSharePath",
          BrowserContextDependencyManager::GetInstance()) {}

GuestOsSharePathFactory::~GuestOsSharePathFactory() = default;

KeyedService* GuestOsSharePathFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GuestOsSharePath(profile);
}

}  // namespace guest_os
