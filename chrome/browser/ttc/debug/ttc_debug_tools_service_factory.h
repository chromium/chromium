// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_TOOLS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_TOOLS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace ttc {

class TtcDebugToolsService;

class TtcDebugToolsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns null unless ttc::kTtc and ttc::kTtcDebugTools are enabled and
  // `context` is a regular profile.
  static TtcDebugToolsService* GetForBrowserContext(
      content::BrowserContext* context);
  static TtcDebugToolsServiceFactory* GetInstance();

  TtcDebugToolsServiceFactory(const TtcDebugToolsServiceFactory&) = delete;
  TtcDebugToolsServiceFactory& operator=(const TtcDebugToolsServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<TtcDebugToolsServiceFactory>;

  TtcDebugToolsServiceFactory();
  ~TtcDebugToolsServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_TOOLS_SERVICE_FACTORY_H_
