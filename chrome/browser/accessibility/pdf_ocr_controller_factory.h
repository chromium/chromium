// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace screen_ai {

class PdfOcrController;

// Factory to get or create an instance of PdfOcrController from a Profile.
class PdfOcrControllerFactory : public ProfileKeyedServiceFactory {
 public:
  static PdfOcrController* GetForProfile(Profile* profile);

  static PdfOcrControllerFactory* GetInstance();

  PdfOcrControllerFactory(const PdfOcrControllerFactory&) = delete;
  PdfOcrControllerFactory& operator=(const PdfOcrControllerFactory&) = delete;

 private:
  friend base::NoDestructor<PdfOcrControllerFactory>;

  PdfOcrControllerFactory();
  ~PdfOcrControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_FACTORY_H_
