// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class GeminiAntiscamProtectionService;

// Singleton that owns GeminiAntiscamProtectionService objects, one for each
// active Profile. It listsens to profile destroy events and destroy its
// associated service. It returns nullptr if the profile is not suitable (i.e.
// incognito, guest, or standard Safe Browsing).
class GeminiAntiscamProtectionServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static GeminiAntiscamProtectionService* GetForProfile(Profile* profile);

  // Returns the singleton instance of the factory.
  static GeminiAntiscamProtectionServiceFactory* GetInstance();

  GeminiAntiscamProtectionServiceFactory(
      const GeminiAntiscamProtectionServiceFactory&) = delete;
  GeminiAntiscamProtectionServiceFactory& operator=(
      const GeminiAntiscamProtectionServiceFactory&) = delete;

 private:
  friend base::NoDestructor<GeminiAntiscamProtectionServiceFactory>;

  GeminiAntiscamProtectionServiceFactory();
  ~GeminiAntiscamProtectionServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_FACTORY_H_
