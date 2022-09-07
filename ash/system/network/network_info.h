// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_INFO_H_
#define ASH_SYSTEM_NETWORK_NETWORK_INFO_H_

#include <string>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

// Includes information necessary about a network for displaying the appropriate
// UI to the user.
struct NetworkInfo {
  NetworkInfo();
  explicit NetworkInfo(const std::string& guid);
  ~NetworkInfo();

  bool operator==(const NetworkInfo& other) const;
  bool operator!=(const NetworkInfo& other) const { return !(*this == other); }

  std::string guid;
  std::u16string label;
  std::u16string tooltip;
  gfx::ImageSkia image;
  bool disable = false;
  bool secured = false;
  bool connectable = false;
  bool sim_locked = false;
  // Only set for eSIM cellular networks. This is used to uniquely identity
  // eSIM networks.
  std::string sim_eid;
  // Initialized in .cc file because full (non-forward) mojom headers are large.
  chromeos::network_config::mojom::ConnectionStateType connection_state;
  chromeos::network_config::mojom::NetworkType type;
  chromeos::network_config::mojom::OncSource source;
  // Used by cellular networks, for other network types, activation_status is
  // set to a default value of kUnknown.
  chromeos::network_config::mojom::ActivationStateType activation_state;
  int battery_percentage = 0;
  int signal_strength = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_INFO_H_
