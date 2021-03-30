// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/guest_signin_observer_factory.h"

#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

class GuestSigninObserver : public KeyedService,
                            public signin::IdentityManager::Observer {
 public:
  explicit GuestSigninObserver(Profile* profile) : profile_(profile) {
    IdentityManagerFactory::GetForProfile(profile)->AddObserver(this);
  }

  void Shutdown() override {
    IdentityManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
      case signin::PrimaryAccountChangeEvent::Type::kSet:
        base::UmaHistogramBoolean("Profile.EphemeralGuest.Signin", true);
        break;
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        base::UmaHistogramBoolean("Profile.EphemeralGuest.Signin", false);
        break;
      case signin::PrimaryAccountChangeEvent::Type::kNone:
        break;
    }
  }

 private:
  Profile* profile_;
};

}  // namespace

// static
GuestSigninObserverFactory* GuestSigninObserverFactory::GetInstance() {
  return base::Singleton<GuestSigninObserverFactory>::get();
}

GuestSigninObserverFactory::GuestSigninObserverFactory()
    : BrowserContextKeyedServiceFactory(
          "GuestSigninObserver",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

KeyedService* GuestSigninObserverFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile(Profile::FromBrowserContext(context));
  if (profile->IsEphemeralGuestProfile())
    return new GuestSigninObserver(profile);
  return nullptr;
}

bool GuestSigninObserverFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
