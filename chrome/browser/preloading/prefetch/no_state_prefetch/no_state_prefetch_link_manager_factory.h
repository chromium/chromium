// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_LINK_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_LINK_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace prerender {

class NoStatePrefetchLinkManager;

class NoStatePrefetchLinkManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static NoStatePrefetchLinkManager* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static NoStatePrefetchLinkManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<NoStatePrefetchLinkManagerFactory>;

  NoStatePrefetchLinkManagerFactory();
  ~NoStatePrefetchLinkManagerFactory() override {}

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_LINK_MANAGER_FACTORY_H_
