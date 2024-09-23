// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class RealTimeUrlLookupServiceBase;

// Singleton that owns ChromeEnterpriseRealTimeUrlLookupService objects, one for
// each active Profile. It listens to profile destroy events and destroy its
// associated service. It returns nullptr if the profile is in the Incognito
// mode.
class ChromeEnterpriseRealTimeUrlLookupServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  //
  // This method returns RealTimeUrlLookupServiceBase* instead of
  // ChromeEnterpriseRealTimeUrlLookupService* to fix a UBSAN error in browser
  // tests (b/331696208).  RealTimeUrlLookupServiceBase is the common base
  // class of ChromeEnterpriseRealTimeUrlLookupService and
  // FakeRealTimeUrlLookupService.  Callers of GetForProfile() only need the
  // public interface of the real-time URL lookup service defined by the
  // base class.
  static RealTimeUrlLookupServiceBase* GetForProfile(
      Profile* profile);

  // Get the singleton instance.
  static ChromeEnterpriseRealTimeUrlLookupServiceFactory* GetInstance();

  ChromeEnterpriseRealTimeUrlLookupServiceFactory(
      const ChromeEnterpriseRealTimeUrlLookupServiceFactory&) = delete;
  ChromeEnterpriseRealTimeUrlLookupServiceFactory& operator=(
      const ChromeEnterpriseRealTimeUrlLookupServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ChromeEnterpriseRealTimeUrlLookupServiceFactory>;

  ChromeEnterpriseRealTimeUrlLookupServiceFactory();
  ~ChromeEnterpriseRealTimeUrlLookupServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
