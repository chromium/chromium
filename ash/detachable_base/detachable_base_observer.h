// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DETACHABLE_BASE_DETACHABLE_BASE_OBSERVER_H_
#define ASH_DETACHABLE_BASE_DETACHABLE_BASE_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"

namespace ash {

// Registered with DetachableBaseHandler to observe the detachable base status.
class ASH_EXPORT DetachableBaseObserver {
 public:
  virtual ~DetachableBaseObserver() = default;

  // Called when the detachable base pairing status changes. For example when a
  // new detachable base is paired, or when the current detachable base gets
  // detached.
  virtual void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus status) = 0;

  // Called when the state of whether the current detachable base requires a
  // firmware update changes.
  // |requires_update|: Whether the base currently requires a firmware update.
  virtual void OnDetachableBaseRequiresUpdateChanged(bool requires_update) = 0;
};

}  // namespace ash

#endif  // ASH_DETACHABLE_BASE_DETACHABLE_BASE_OBSERVER_H_
