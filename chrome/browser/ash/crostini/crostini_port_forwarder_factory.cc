// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_port_forwarder_factory.h"

#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace crostini {

// static
CrostiniPortForwarder* CrostiniPortForwarderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CrostiniPortForwarder*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniPortForwarderFactory* CrostiniPortForwarderFactory::GetInstance() {
  static base::NoDestructor<CrostiniPortForwarderFactory> factory;
  return factory.get();
}

CrostiniPortForwarderFactory::CrostiniPortForwarderFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniPortForwarderService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

CrostiniPortForwarderFactory::~CrostiniPortForwarderFactory() = default;

KeyedService* CrostiniPortForwarderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniPortForwarder(profile);
}

}  // namespace crostini
