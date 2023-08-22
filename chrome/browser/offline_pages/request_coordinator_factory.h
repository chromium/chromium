// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_REQUEST_COORDINATOR_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_PAGES_REQUEST_COORDINATOR_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace offline_pages {

class RequestCoordinator;

// A factory to create one unique RequestCoordinator.
class RequestCoordinatorFactory : public ProfileKeyedServiceFactory {
 public:
  static RequestCoordinatorFactory* GetInstance();
  static RequestCoordinator* GetForBrowserContext(
      content::BrowserContext* context);

  RequestCoordinatorFactory(const RequestCoordinatorFactory&) = delete;
  RequestCoordinatorFactory& operator=(const RequestCoordinatorFactory&) =
      delete;

 private:
  friend base::NoDestructor<RequestCoordinatorFactory>;

  RequestCoordinatorFactory();
  ~RequestCoordinatorFactory() override {}

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_REQUEST_COORDINATOR_FACTORY_H_
