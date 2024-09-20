// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_FACTORY_H_
#define CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_FACTORY_H_

#include <map>
#include <memory>

#include "base/run_loop.h"
#include "base/types/pass_key.h"
#include "chrome/browser/dips/dips_browser_signin_detector.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

class DIPSBrowserSigninDetectorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  using PassKey = base::PassKey<DIPSBrowserSigninDetectorFactory>;

  explicit DIPSBrowserSigninDetectorFactory(PassKey);
  static DIPSBrowserSigninDetectorFactory* GetInstance();
  static DIPSBrowserSigninDetector* GetForBrowserContext(
      content::BrowserContext* browser_context);

  void EnableWaitForServiceForTesting();
  // Blocks until a DIPSBrowserSigninDetector has been created for
  // `browser_context`.
  void WaitForServiceForTesting(content::BrowserContext* browser_context);

 private:
  ~DIPSBrowserSigninDetectorFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // A RunLoop per BrowserContext, for WaitForServiceForTesting().
  mutable std::optional<std::map<std::string, base::RunLoop>>
      context_runloops_for_testing_;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_FACTORY_H_
