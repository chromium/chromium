// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_PAGE_EMBEDDINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_PAGE_EMBEDDINGS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename>
class NoDestructor;
}  // namespace base

namespace passage_embeddings {

class PageEmbeddingsService;

class PageEmbeddingsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance for `profile`. Creates it if there is no instance.
  static PageEmbeddingsService* GetForProfile(Profile* profile);

  static PageEmbeddingsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<PageEmbeddingsServiceFactory>;

  PageEmbeddingsServiceFactory();
  ~PageEmbeddingsServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_PAGE_EMBEDDINGS_SERVICE_FACTORY_H_
