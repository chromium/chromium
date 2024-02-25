// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_H_

#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

#include "chrome/browser/profiles/profile_selections.h"

// Similar intermediate class as `ProfileKeyedServiceFactory` but for RefCounted
// Services.
// Follow `profile_keyed_service_factory.h` for the documentation and usages.
// Any change to this class should also be reflected on
// `ProfileKeyedServiceFactory`.
// For simplicity the unit tests are found in
// profile_keyed_service_factory_unittests.cc.
class RefcountedProfileKeyedServiceFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  RefcountedProfileKeyedServiceFactory(
      const RefcountedProfileKeyedServiceFactory&) = delete;
  RefcountedProfileKeyedServiceFactory& operator=(
      const RefcountedProfileKeyedServiceFactory&) = delete;

 protected:
  // Default constructor, will build the Factory with the default implementation
  // for `ProfileSelections`.
  explicit RefcountedProfileKeyedServiceFactory(const char* name);
  // Constructor taking in the overridden `ProfileSelections` for customized
  // Profile types service creation. This is the only way to override the
  // `ProfileSelections` value.
  RefcountedProfileKeyedServiceFactory(
      const char* name,
      const ProfileSelections& profile_selections);
  ~RefcountedProfileKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  // Final implementation of `GetBrowserContextToUse()`.
  // Selects the given context to proper context to use based on the
  // mapping in `ProfileSelections`.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const final;

 private:
  const ProfileSelections profile_selections_;
};

#endif  // CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_H_
