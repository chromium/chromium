// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class KeyedService;
class Profile;

namespace page_content_annotations {

class PageContentExtractionService;

// Singleton that owns all `PageContentExtractionService` instances, each mapped
// to one profile. Listens for profile destructions and clean up the associated
// PageContentExtractionServices.
class PageContentExtractionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the `PageContentExtractionService` instance for `profile`. Create
  // it if there is no instance.
  static PageContentExtractionService* GetForProfile(Profile* profile);

  // Gets the singleton instance of this factory class.
  static PageContentExtractionServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<PageContentExtractionServiceFactory>;

  PageContentExtractionServiceFactory();
  ~PageContentExtractionServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_FACTORY_H_
