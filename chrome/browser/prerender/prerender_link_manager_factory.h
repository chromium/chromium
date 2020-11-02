// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace prerender {

class PrerenderLinkManager;

class PrerenderLinkManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrerenderLinkManager* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static PrerenderLinkManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrerenderLinkManagerFactory>;

  PrerenderLinkManagerFactory();
  ~PrerenderLinkManagerFactory() override {}

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_FACTORY_H_
