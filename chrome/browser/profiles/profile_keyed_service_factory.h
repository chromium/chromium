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
// Purpose of this API:
// Provide a Profile type specific implementation logic for
// `KeyedServiceFactory` under chrome/.
// When a KeyedServiceFactory is building a service for a "Profile A", it can
// actually return a service that is attached to a "Profile B". Common
// cases is that an Off-The-Record profile uses it's parent service (redirecting
// to Original) or not use any service at all (no service for OTR).

// `ProfileKeyedServiceFactory' is an intermediate interface to create
// KeyedServiceFactory under chrome/ that provides a more restricted default
// creation of services for non regular profiles. Main purpose of this class is
// to provide an easy and efficient way to provide the redirection logic for
// each main profile types using `ProfileSelections` instance. Those profile
// choices are overridable by setting the proper combination of
// `ProfileSelection` and Profile type in the `ProfileSelections` passed in the
// constructor.
//
// - Example usage, for a factory redirecting in incognito.
//
// class MyRedirectingKeyedServiceFactory: public ProfileKeyedServiceFactory {
//  private:
//   MyRedirectingKeyedServiceFactory()
//       : ProfileKeyedServiceFactory(
//             "MyRedirectingKeyedService",
//             ProfileSelections::BuildRedirectedInIncognitoNonExperimental())
//             {}
//   }
// };
//
//
// - Example service that does not exist in OTR (default behavior):
//
// class MyDefaultKeyedServiceFactory: public ProfileKeyedServiceFactory {
//  private:
//   MyDefaultKeyedServiceFactory()
//       : ProfileKeyedServiceFactory("MyDefaultKeyedService") {}
//   }
// };
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
  // Check `ProfileSelections::BuildDefault()` for details on which Profile the
  // service will be constructed for.
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

#endif  // !CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
