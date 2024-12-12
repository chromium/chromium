// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace passage_embeddings {

class EmbedderService;

class EmbedderServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static EmbedderService* GetForProfile(Profile* profile);

  static EmbedderServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<EmbedderServiceFactory>;

  EmbedderServiceFactory();
  ~EmbedderServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_SERVICE_FACTORY_H_
