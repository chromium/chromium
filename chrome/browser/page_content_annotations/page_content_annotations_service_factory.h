// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace page_content_annotations {
class PageContentAnnotationsService;
}  // namespace page_content_annotations

class Profile;

// LazyInstance that owns all PageContentAnnotationsService(s) and associates
// them with Profiles.
class PageContentAnnotationsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the PageContentAnnotationsService for the profile.
  //
  // Returns null if the features that allow for this to provide useful
  // information are disabled.
  static page_content_annotations::PageContentAnnotationsService* GetForProfile(
      Profile* profile);

  // Gets the LazyInstance that owns all PageContentAnnotationsService(s).
  static PageContentAnnotationsServiceFactory* GetInstance();

  PageContentAnnotationsServiceFactory(
      const PageContentAnnotationsServiceFactory&) = delete;
  PageContentAnnotationsServiceFactory& operator=(
      const PageContentAnnotationsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PageContentAnnotationsServiceFactory>;

  PageContentAnnotationsServiceFactory();
  ~PageContentAnnotationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
