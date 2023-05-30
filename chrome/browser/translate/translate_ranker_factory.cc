// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_ranker_factory.h"

#include "base/files/file_path.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace translate {

// static
TranslateRankerFactory* TranslateRankerFactory::GetInstance() {
  static base::NoDestructor<TranslateRankerFactory> instance;
  return instance.get();
}

// static
translate::TranslateRanker* TranslateRankerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<translate::TranslateRanker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

TranslateRankerFactory::TranslateRankerFactory()
    : ProfileKeyedServiceFactory(
          "TranslateRanker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Translate is enabled in guest profiles.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

TranslateRankerFactory::~TranslateRankerFactory() = default;

KeyedService* TranslateRankerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new TranslateRankerImpl(
      TranslateRankerImpl::GetModelPath(browser_context->GetPath()),
      TranslateRankerImpl::GetModelURL(), ukm::UkmRecorder::Get());
}

}  // namespace translate
