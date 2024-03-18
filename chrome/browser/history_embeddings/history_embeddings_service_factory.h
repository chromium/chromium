// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace history_embeddings {
class HistoryEmbeddingsService;
}  // namespace history_embeddings

class HistoryEmbeddingsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // This can return nullptr in tests.
  static history_embeddings::HistoryEmbeddingsService* GetForProfile(
      Profile* profile);

  static HistoryEmbeddingsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<HistoryEmbeddingsServiceFactory>;

  HistoryEmbeddingsServiceFactory();
  ~HistoryEmbeddingsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_FACTORY_H_
