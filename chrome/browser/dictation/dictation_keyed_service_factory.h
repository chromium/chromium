// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace dictation {

class DictationKeyedService;

class DictationKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DictationKeyedService* GetDictationKeyedService(
      content::BrowserContext* browser_context);
  static DictationKeyedServiceFactory* GetInstance();

  explicit DictationKeyedServiceFactory(
      base::PassKey<DictationKeyedServiceFactory>);
  DictationKeyedServiceFactory(const DictationKeyedServiceFactory&) = delete;
  DictationKeyedServiceFactory& operator=(const DictationKeyedServiceFactory&) =
      delete;
  ~DictationKeyedServiceFactory() override;

 private:
  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_FACTORY_H_
