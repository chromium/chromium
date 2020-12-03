// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_FAKE_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_FAKE_H_

#include "chrome/browser/chromeos/borealis/borealis_service.h"

namespace content {
class BrowserContext;
}

namespace borealis {

class BorealisServiceFake : public BorealisService {
 public:
  // Causes the service for the given |context| to be a fake in tests. Returns a
  // handle to the fake, which will be owned by the service factory.
  static BorealisServiceFake* UseFakeForTesting(
      content::BrowserContext* context);

  ~BorealisServiceFake() override;

  BorealisAppLauncher& AppLauncher() override;
  BorealisContextManager& ContextManager() override;
  BorealisFeatures& Features() override;
  BorealisWindowManager& WindowManager() override;

  void SetAppLauncherForTesting(BorealisAppLauncher* app_launcher);
  void SetContextManagerForTesting(BorealisContextManager* context_manager);
  void SetFeaturesForTesting(BorealisFeatures* features);
  void SetWindowManagerForTesting(BorealisWindowManager* window_manager);

 private:
  BorealisAppLauncher* app_launcher_ = nullptr;
  BorealisContextManager* context_manager_ = nullptr;
  BorealisFeatures* features_ = nullptr;
  BorealisWindowManager* window_manager_ = nullptr;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_FAKE_H_
