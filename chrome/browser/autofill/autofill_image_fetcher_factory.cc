// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_image_fetcher_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {

// static
AutofillImageFetcher* AutofillImageFetcherFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutofillImageFetcher*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
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

KeyedService* AutofillImageFetcherFactory::BuildAutofillImageFetcher(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  AutofillImageFetcher* service =
      new AutofillImageFetcher(profile->GetDefaultStoragePartition()
                                   ->GetURLLoaderFactoryForBrowserProcess(),
                               std::make_unique<ImageDecoderImpl>());
  return service;
}

KeyedService* AutofillImageFetcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildAutofillImageFetcher(context);
}

}  // namespace autofill
