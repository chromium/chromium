// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROFILE_GARBAGE_COLLECTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROFILE_GARBAGE_COLLECTION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace unexportable_keys {

// Factory to create services that perform garbage collection of unexportable
// keys associated with obsolete off-the-record profiles.
class UnexportableKeyProfileGarbageCollectionServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static UnexportableKeyProfileGarbageCollectionServiceFactory* GetInstance();
  KeyedService* GetServiceForBrowserContext(
      content::BrowserContext* context) const;

 private:
  friend class base::NoDestructor<
      UnexportableKeyProfileGarbageCollectionServiceFactory>;

  UnexportableKeyProfileGarbageCollectionServiceFactory();

  // ProfileKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace unexportable_keys

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROFILE_GARBAGE_COLLECTION_SERVICE_FACTORY_H_
