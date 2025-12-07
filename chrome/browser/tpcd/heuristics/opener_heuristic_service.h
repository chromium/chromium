// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_SERVICE_H_
#define CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"

namespace content {
class BrowserContext;
}

namespace privacy_sandbox {
class TrackingProtectionSettings;
}

class OpenerHeuristicServiceFactory;

class OpenerHeuristicService
    : public KeyedService,
      privacy_sandbox::TrackingProtectionSettingsObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnBackfillPopupHeuristicGrants(
        content::BrowserContext* browser_context,
        bool success) = 0;
  };

  OpenerHeuristicService(base::PassKey<OpenerHeuristicServiceFactory>,
                         content::BrowserContext* context);
  ~OpenerHeuristicService() override;

  static OpenerHeuristicService* Get(content::BrowserContext* context);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // KeyedService overrides:
  void Shutdown() override;

  // TrackingProtectionSettingsObserver overrides:
  void OnTrackingProtection3pcdChanged() override;

  void NotifyBackfillPopupHeuristicGrants(bool success);

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  base::ScopedObservation<privacy_sandbox::TrackingProtectionSettings,
                          privacy_sandbox::TrackingProtectionSettingsObserver>
      tracking_protection_settings_observation_{this};
  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<OpenerHeuristicService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_SERVICE_H_
