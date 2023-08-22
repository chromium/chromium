// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_IN_MEMORY_URL_INDEX_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_IN_MEMORY_URL_INDEX_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class InMemoryURLIndex;
class Profile;

class InMemoryURLIndexFactory : public ProfileKeyedServiceFactory {
 public:
  static InMemoryURLIndex* GetForProfile(Profile* profile);
  static InMemoryURLIndexFactory* GetInstance();

 private:
  friend base::NoDestructor<InMemoryURLIndexFactory>;

  InMemoryURLIndexFactory();
  ~InMemoryURLIndexFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_IN_MEMORY_URL_INDEX_FACTORY_H_
