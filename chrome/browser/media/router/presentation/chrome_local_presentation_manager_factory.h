// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_CHROME_LOCAL_PRESENTATION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_CHROME_LOCAL_PRESENTATION_MANAGER_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/media_router/browser/presentation/local_presentation_manager_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace media_router {

// A LocalPresentationManagerFactory that shares implementations between a
// profile and its associated incognito profiles.
class ChromeLocalPresentationManagerFactory
    : public LocalPresentationManagerFactory {
 public:
  // For test use only.
  static ChromeLocalPresentationManagerFactory* GetInstance();

  ChromeLocalPresentationManagerFactory(
      const ChromeLocalPresentationManagerFactory&) = delete;
  ChromeLocalPresentationManagerFactory& operator=(
      const ChromeLocalPresentationManagerFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<
      ChromeLocalPresentationManagerFactory>;

  ChromeLocalPresentationManagerFactory();
  ~ChromeLocalPresentationManagerFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_CHROME_LOCAL_PRESENTATION_MANAGER_FACTORY_H_
