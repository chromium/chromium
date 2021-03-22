// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MARKER_MARKER_CONTROLLER_H_
#define ASH_MARKER_MARKER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/fast_ink/fast_ink_pointer_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

// A checked observer which receives notification of changes to the marker
// activation state.
class ASH_EXPORT MarkerObserver : public base::CheckedObserver {
 public:
  virtual void OnMarkerStateChanged(bool enabled) {}
};

// Controller for the Marker functionality. Enables/disables Marker as well as
// receives points and passes them off to be rendered.
class ASH_EXPORT MarkerController : public fast_ink::FastInkPointerController {
 public:
  MarkerController();
  MarkerController(const MarkerController&) = delete;
  MarkerController& operator=(const MarkerController&) = delete;
  ~MarkerController() override;

  static MarkerController* Get();

  // Adds/removes the specified |observer|.
  void AddObserver(MarkerObserver* observer);
  void RemoveObserver(MarkerObserver* observer);

  // fast_ink::FastInkPointerController:
  void SetEnabled(bool enabled) override;
  views::View* GetPointerView() const override;
  void CreatePointerView(base::TimeDelta presentation_delay,
                         aura::Window* root_window) override;
  void UpdatePointerView(ui::TouchEvent* event) override;
  void DestroyPointerView() override;

 private:
  void NotifyStateChanged(bool enabled);

  base::ObserverList<MarkerObserver> observers_;

  base::WeakPtrFactory<MarkerController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_MARKER_MARKER_CONTROLLER_H_
