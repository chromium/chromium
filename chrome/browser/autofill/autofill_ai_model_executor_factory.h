// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class KeyedService;
class Profile;

namespace autofill {

class AutofillAiModelExecutor;

class AutofillAiModelExecutorFactory : public ProfileKeyedServiceFactory {
 public:
  static AutofillAiModelExecutor* GetForProfile(Profile* profile);
  static AutofillAiModelExecutorFactory* GetInstance();

 private:
  friend base::NoDestructor<AutofillAiModelExecutorFactory>;

  AutofillAiModelExecutorFactory();
  ~AutofillAiModelExecutorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_
