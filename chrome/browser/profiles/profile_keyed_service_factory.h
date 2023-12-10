// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include "chrome/browser/profiles/profile_selections.h"

namespace profiles::testing {
class ScopedProfileSelectionsForFactoryTesting;
}

// Detailed doc: "./profile_keyed_service_factory.md"
//
// ProfileKeyedServiceFactory provides a `Profile`-specific interface for
// `KeyedServiceFactory` under chrome/.
//
// When a KeyedServiceFactory builds a service for a "Profile A", it can
// actually return a service attached to a "Profile B". A common case is when a
// service of the original profile is reused by the Off-The-Record (OTR) profile
// (ProfileSelection::kRedirectedToOriginal()). Furthermore, a service can also
// be created for either only the original profile, or only the OTR profile.
//
// `ProfileKeyedServiceFactory' provides control over how services are created
// by default for non-regular profiles and how services are redirected across
// profiles. The defaults can be overridden with the `ProfileSelections`
// constructor parameter.
//
// - Example of a factory redirecting in incognito:
//
// class MyRedirectingKeyedServiceFactory: public ProfileKeyedServiceFactory {
//  private:
//   MyRedirectingKeyedServiceFactory()
//       : ProfileKeyedServiceFactory(
//             "MyRedirectingKeyedService",
//             ProfileSelections::BuildRedirectedInIncognito())
//             {}
//   }
// };
//
//
// - Example of a service that does not exist in OTR (default behavior):
//
// class MyDefaultKeyedServiceFactory: public ProfileKeyedServiceFactory {
//  private:
//   MyDefaultKeyedServiceFactory()
//       : ProfileKeyedServiceFactory("MyDefaultKeyedService") {}
//   }
// };
//
// Any change to this class should also be reflected on
// `RefcountedProfileKeyedServiceFactory`.
class ProfileKeyedServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  ProfileKeyedServiceFactory(const ProfileKeyedServiceFactory&) = delete;
  ProfileKeyedServiceFactory& operator=(const ProfileKeyedServiceFactory&) =
      delete;

 protected:
  // Default constructor, will build the Factory with the default implementation
  // for `ProfileSelections`.
  explicit ProfileKeyedServiceFactory(const char* name);
  // Constructor taking in the overridden `ProfileSelections` for customized
  // Profile types service creation. This is the only way to override the
  // `ProfileSelections` value.
  ProfileKeyedServiceFactory(const char* name,
                             const ProfileSelections& profile_selections);
  ~ProfileKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  // Final implementation of `GetBrowserContextToUse()`.
  // Selects the given context to proper context to use based on the
  // mapping in `ProfileSelections`.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const final;

 private:
  friend class profiles::testing::ScopedProfileSelectionsForFactoryTesting;

  ProfileSelections profile_selections_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
