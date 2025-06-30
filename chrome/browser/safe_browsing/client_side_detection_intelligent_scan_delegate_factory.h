// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"

class KeyedService;
class Profile;

namespace safe_browsing {

// Singleton that owns IntelligentScanDelegate objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns nullptr if the profile is in the Incognito mode.
class ClientSideDetectionIntelligentScanDelegateFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static ClientSideDetectionHost::IntelligentScanDelegate* GetForProfile(
      Profile* profile);

  // Returns the singleton instance.
  static ClientSideDetectionIntelligentScanDelegateFactory* GetInstance();

  ClientSideDetectionIntelligentScanDelegateFactory(
      const ClientSideDetectionIntelligentScanDelegateFactory&) = delete;
  ClientSideDetectionIntelligentScanDelegateFactory& operator=(
      const ClientSideDetectionIntelligentScanDelegateFactory&) = delete;

 private:
  friend base::NoDestructor<ClientSideDetectionIntelligentScanDelegateFactory>;

  ClientSideDetectionIntelligentScanDelegateFactory();
  ~ClientSideDetectionIntelligentScanDelegateFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_
