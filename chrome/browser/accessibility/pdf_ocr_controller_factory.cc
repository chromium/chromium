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
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

PdfOcrControllerFactory::~PdfOcrControllerFactory() = default;

KeyedService* PdfOcrControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PdfOcrController(Profile::FromBrowserContext(context));
}

}  // namespace screen_ai
