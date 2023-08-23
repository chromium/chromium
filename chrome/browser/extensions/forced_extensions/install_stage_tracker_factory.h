// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALL_STAGE_TRACKER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALL_STAGE_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class InstallStageTracker;

class InstallStageTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static InstallStageTracker* GetForBrowserContext(
      content::BrowserContext* context);

  static InstallStageTrackerFactory* GetInstance();

  InstallStageTrackerFactory(const InstallStageTrackerFactory&) = delete;
  InstallStageTrackerFactory& operator=(const InstallStageTrackerFactory&) =
      delete;

 private:
  friend class base::NoDestructor<InstallStageTrackerFactory>;

  InstallStageTrackerFactory();
  ~InstallStageTrackerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALL_STAGE_TRACKER_FACTORY_H_
