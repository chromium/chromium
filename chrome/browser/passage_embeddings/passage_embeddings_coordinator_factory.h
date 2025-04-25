// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_COORDINATOR_FACTORY_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_COORDINATOR_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename>
class NoDestructor;
}  // namespace base

namespace passage_embeddings {

class PassageEmbeddingsCoordinator;

class PassageEmbeddingsCoordinatorFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance for `profile`. Creates it if there is no instance.
  static PassageEmbeddingsCoordinator* GetForProfile(Profile* profile);

  static PassageEmbeddingsCoordinatorFactory* GetInstance();

 private:
  friend base::NoDestructor<PassageEmbeddingsCoordinatorFactory>;

  PassageEmbeddingsCoordinatorFactory();
  ~PassageEmbeddingsCoordinatorFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_COORDINATOR_FACTORY_H_
