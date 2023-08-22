// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_FACTORY_H_
#define CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace consent_auditor {
class ConsentAuditor;
}

class Profile;

class ConsentAuditorFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the singleton instance of ChromeConsentAuditorFactory.
  static ConsentAuditorFactory* GetInstance();

  // Returns the ContextAuditor associated with |profile|.
  static consent_auditor::ConsentAuditor* GetForProfile(Profile* profile);

  ConsentAuditorFactory(const ConsentAuditorFactory&) = delete;
  ConsentAuditorFactory& operator=(const ConsentAuditorFactory&) = delete;

 private:
  friend base::NoDestructor<ConsentAuditorFactory>;

  ConsentAuditorFactory();
  ~ConsentAuditorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_FACTORY_H_
