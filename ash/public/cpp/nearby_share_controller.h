// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NEARBY_SHARE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_NEARBY_SHARE_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"

namespace ash {

// Relays events from //chrome to //ash for Nearby Share.
class ASH_PUBLIC_EXPORT NearbyShareController {
 public:
  virtual ~NearbyShareController() = default;

  // To be called whenever Nearby Share's  High Visibility state changes.
  virtual void HighVisibilityEnabledChanged(bool enabled) = 0;

  // Call on change in Nearby Share selected Visibility.
  virtual void VisibilityChanged(
      ::nearby_share::mojom::Visibility visibility) const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NEARBY_SHARE_CONTROLLER_H_
