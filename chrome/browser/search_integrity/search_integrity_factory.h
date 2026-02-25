// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_FACTORY_H_
#define CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace search_integrity {

class SearchIntegrity;

// Factory to create and manage a single instance of `SearchIntegrity` per
// `Profile`. This ensures that `SearchIntegrity` is created on demand and
// its lifetime is tied to the `Profile`.
class SearchIntegrityFactory : public ProfileKeyedServiceFactory {
 public:
  static SearchIntegrity* GetForProfile(Profile* profile);
  static SearchIntegrityFactory* GetInstance();

  SearchIntegrityFactory(const SearchIntegrityFactory&) = delete;
  SearchIntegrityFactory& operator=(const SearchIntegrityFactory&) = delete;

 private:
  friend base::NoDestructor<SearchIntegrityFactory>;

  SearchIntegrityFactory();
  ~SearchIntegrityFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace search_integrity

#endif  // CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_FACTORY_H_
