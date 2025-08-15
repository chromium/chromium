// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_SCREENSHOT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_SCREENSHOT_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"

namespace page_content_annotations {

class PageContentScreenshotService;

class PageContentScreenshotServiceFactory : public ProfileKeyedServiceFactory {
 public:
  PageContentScreenshotServiceFactory(
      const PageContentScreenshotServiceFactory&) = delete;
  PageContentScreenshotServiceFactory& operator=(
      const PageContentScreenshotServiceFactory&) = delete;

  static PageContentScreenshotService* GetForProfile(Profile* profile);

  static PageContentScreenshotServiceFactory* GetInstance();

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<PageContentScreenshotServiceFactory>;

  PageContentScreenshotServiceFactory();
  ~PageContentScreenshotServiceFactory() override;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_SCREENSHOT_SERVICE_FACTORY_H_
