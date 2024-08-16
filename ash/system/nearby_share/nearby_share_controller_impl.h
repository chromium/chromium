// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_CONTROLLER_IMPL_H_

#include "ash/public/cpp/nearby_share_controller.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

// Handles Nearby Share events from //chrome, providing an observer interface
// within //ash. Singleton, lives on UI thread.
class NearbyShareControllerImpl : public NearbyShareController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Relays high visibility state changes from the service to the pod button.
    virtual void OnHighVisibilityEnabledChanged(bool enabled) = 0;

    // Relay visibility state changes from the settings to the pod button.
    virtual void OnVisibilityChanged(
        ::nearby_share::mojom::Visibility visibility) = 0;
  };

  NearbyShareControllerImpl();
  NearbyShareControllerImpl(NearbyShareControllerImpl&) = delete;
  NearbyShareControllerImpl& operator=(NearbyShareControllerImpl&) = delete;
  ~NearbyShareControllerImpl() override;

  // NearbyShareController
  void HighVisibilityEnabledChanged(bool enabled) override;
  void VisibilityChanged(
      ::nearby_share::mojom::Visibility visibility) const override;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_CONTROLLER_IMPL_H_
