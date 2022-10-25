// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_image_fetcher_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autofill/autofill_image_fetcher_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"

namespace autofill {

// static
AutofillImageFetcher* AutofillImageFetcherFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutofillImageFetcherImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AutofillImageFetcherFactory* AutofillImageFetcherFactory::GetInstance() {
  static base::NoDestructor<AutofillImageFetcherFactory> instance;
  return instance.get();
}

AutofillImageFetcherFactory::AutofillImageFetcherFactory()
    : ProfileKeyedServiceFactory(
          "AutofillImageFetcher",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AutofillImageFetcherFactory::~AutofillImageFetcherFactory() = default;

KeyedService* AutofillImageFetcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AutofillImageFetcherImpl(
      Profile::FromBrowserContext(context)->GetProfileKey());
}

}  // namespace autofill
