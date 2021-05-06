// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_ranker_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace translate {

// static
TranslateRankerFactory* TranslateRankerFactory::GetInstance() {
  return base::Singleton<TranslateRankerFactory>::get();
}

// static
translate::TranslateRanker* TranslateRankerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<translate::TranslateRanker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

TranslateRankerFactory::TranslateRankerFactory()
    : BrowserContextKeyedServiceFactory(
          "TranslateRanker",
          BrowserContextDependencyManager::GetInstance()) {}

TranslateRankerFactory::~TranslateRankerFactory() {}

KeyedService* TranslateRankerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new TranslateRankerImpl(
      TranslateRankerImpl::GetModelPath(browser_context->GetPath()),
      TranslateRankerImpl::GetModelURL(), ukm::UkmRecorder::Get());
}

content::BrowserContext* TranslateRankerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace translate
