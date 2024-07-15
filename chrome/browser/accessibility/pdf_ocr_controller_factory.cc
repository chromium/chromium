// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"

#include "chrome/browser/accessibility/pdf_ocr_controller.h"
#include "chrome/browser/profiles/profile.h"

namespace screen_ai {
// static
PdfOcrController* PdfOcrControllerFactory::GetForProfile(Profile* profile) {
  return static_cast<PdfOcrController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PdfOcrControllerFactory* PdfOcrControllerFactory::GetInstance() {
  static base::NoDestructor<PdfOcrControllerFactory> instance;
  return instance.get();
}

PdfOcrControllerFactory::PdfOcrControllerFactory()
    : ProfileKeyedServiceFactory(
          "PdfOcrController",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

PdfOcrControllerFactory::~PdfOcrControllerFactory() = default;

std::unique_ptr<KeyedService>
PdfOcrControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PdfOcrController>(
      Profile::FromBrowserContext(context));
}

}  // namespace screen_ai
