// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_
#define CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

class Profile;

// NOTE(crbug/1230193): The DSE auto-granted permissions have been disabled and
// all of the previously granted permissions are reverted in the initialization
// step.
// Helper class to manage DSE permissions. It keeps the setting valid by
// watching change to the CCTLD and DSE.
// Glossary:
//     DSE: Default Search Engine
//     CCTLD: Country Code Top Level Domain (e.g. google.com.au)
class SearchPermissionsService : public KeyedService {
 public:
  // Use
  // `SearchPermissionService::Factory::BuildServiceInstanceForBrowserContext`
  // instead.
  explicit SearchPermissionsService(Profile* profile);
  ~SearchPermissionsService() override;

  // Delegate for search engine related functionality. Can be overridden for
  // testing.
  class SearchEngineDelegate {
   public:
    virtual ~SearchEngineDelegate() = default;

    // Returns the origin of the DSE. If the current DSE is Google this will
    // return the current CCTLD.
    virtual url::Origin GetDSEOrigin() = 0;
  };

  // Factory implementation will not create a service in incognito.
  class Factory : public ProfileKeyedServiceFactory {
   public:
    static SearchPermissionsService* GetForBrowserContext(
        content::BrowserContext* context);

    static Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    bool ServiceIsCreatedWithBrowserContext() const override;
    std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
        content::BrowserContext* profile) const override;
  };

  // Returns whether the given origin matches the DSE origin.
  bool IsDseOrigin(const url::Origin& origin);

  // KeyedService:
  void Shutdown() override;

 private:
  friend class SearchPermissionsServiceTest;

  // Record the content settings for notifications and geolocation on the DSE
  // origin. Called at initialization or when the DSE origin changes.
  void RecordEffectiveDSEOriginPermissions(Profile* profile);

  void SetSearchEngineDelegateForTest(
      std::unique_ptr<SearchEngineDelegate> delegate);

  std::unique_ptr<SearchEngineDelegate> delegate_;
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_
