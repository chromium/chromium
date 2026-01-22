// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace accessibility_annotator {

class ContentAnnotatorService;

class ContentAnnotatorServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ContentAnnotatorService* GetForProfile(Profile* profile);
  static ContentAnnotatorServiceFactory* GetInstance();

  ContentAnnotatorServiceFactory(const ContentAnnotatorServiceFactory&) =
      delete;
  ContentAnnotatorServiceFactory& operator=(
      const ContentAnnotatorServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ContentAnnotatorServiceFactory>;

  ContentAnnotatorServiceFactory();
  ~ContentAnnotatorServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_FACTORY_H_
