// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace autofill {

class AutofillAiPersonalContextAccessManager;

class AutofillAiPersonalContextAccessManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AutofillAiPersonalContextAccessManager* GetForProfile(
      Profile* profile);

  static AutofillAiPersonalContextAccessManagerFactory* GetInstance();

  AutofillAiPersonalContextAccessManagerFactory(
      const AutofillAiPersonalContextAccessManagerFactory&) = delete;
  AutofillAiPersonalContextAccessManagerFactory& operator=(
      const AutofillAiPersonalContextAccessManagerFactory&) = delete;

 private:
  friend base::NoDestructor<AutofillAiPersonalContextAccessManagerFactory>;

  AutofillAiPersonalContextAccessManagerFactory();
  ~AutofillAiPersonalContextAccessManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
