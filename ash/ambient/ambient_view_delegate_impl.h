// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_VIEW_DELEGATE_IMPL_H_
#define ASH_AMBIENT_AMBIENT_VIEW_DELEGATE_IMPL_H_

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "base/memory/raw_ptr.h"

#include "ash/ambient/model/ambient_backend_model.h"
#include "base/observer_list.h"

namespace ash {

class AmbientController;

class AmbientViewDelegateImpl : public AmbientViewDelegate {
 public:
  explicit AmbientViewDelegateImpl(AmbientController* ambient_controller);
  AmbientViewDelegateImpl(const AmbientViewDelegateImpl&) = delete;
  AmbientViewDelegateImpl& operator=(AmbientViewDelegateImpl&) = delete;
  ~AmbientViewDelegateImpl() override;

  // AmbientViewDelegate:
  AmbientBackendModel* GetAmbientBackendModel() override;
  AmbientWeatherModel* GetAmbientWeatherModel() override;
  void AddObserver(AmbientViewDelegateObserver* observer) override;
  void RemoveObserver(AmbientViewDelegateObserver* observer) override;

  void NotifyObserversMarkerHit(AmbientPhotoConfig::Marker marker);

 private:
  const raw_ptr<AmbientController> ambient_controller_;  // Owned by Shell.

  base::ObserverList<AmbientViewDelegateObserver> view_delegate_observers_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_VIEW_DELEGATE_IMPL_H_
