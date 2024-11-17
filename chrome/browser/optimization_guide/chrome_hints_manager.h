// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_HINTS_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_HINTS_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "components/optimization_guide/core/hints_manager.h"

class OptimizationGuideLogger;
class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

class ChromeHintsManager : public HintsManager,
                           public NavigationPredictorKeyedService::Observer {
 public:
  ChromeHintsManager(
      Profile* profile,
      PrefService* pref_service,
      base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
      optimization_guide::TopHostProvider* top_host_provider,
      optimization_guide::TabUrlProvider* tab_url_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<optimization_guide::PushNotificationManager>
          push_notification_manager,
      signin::IdentityManager* identity_manager,
      OptimizationGuideLogger* optimization_guide_logger);

  ~ChromeHintsManager() override;

  // Unhooks the observer from the navigation predictor service.
  void Shutdown();

  // NavigationPredictorKeyedService::Observer:
  void OnPredictionUpdated(
      const NavigationPredictorKeyedService::Prediction& prediction) override;

 private:
  // A reference to the profile. Not owned.
  raw_ptr<Profile> profile_ = nullptr;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_HINTS_MANAGER_H_
