// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FEATURE_STATUS_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_FEATURE_STATUS_PROVIDER_H_

#include "ash/webui/eche_app_ui/feature_status.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace eche_app {

// Provides the current status of the eche feature and notifies observers
// when the status changes.
class FeatureStatusProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the status has changed; use GetStatus() for the new status.
    virtual void OnFeatureStatusChanged() = 0;
  };

  FeatureStatusProvider(const FeatureStatusProvider&) = delete;
  FeatureStatusProvider& operator=(const FeatureStatusProvider&) = delete;
  virtual ~FeatureStatusProvider();

  virtual FeatureStatus GetStatus() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  FeatureStatusProvider();

  void NotifyStatusChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FEATURE_STATUS_PROVIDER_H_
