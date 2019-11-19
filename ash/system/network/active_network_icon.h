// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
#define ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class TrayNetworkStateModel;

// Provides an interface to network_icon for the default network. This class
// supports two interfaces:
// * Single: A single icon is shown to represent the active network state.
// * Dual: One or two icons are shown to represent the active network state:
// ** Primary: The state of the primary active network. If Cellular, a
//    a technology badge is used to represent the network.
// ** Cellular (enabled devices only): The state of the Cellular connection if
//    available regardless of whether it is the active network.
// NOTE : GetSingleDefaultImage is partially tested in network_icon_unittest.cc,
// and partially in active_network_icon_unittest.cc.
// TODO(stevenjb): Move all test coverage to active_network_icon_unittest.cc and
// test Dual icon methods.
// This class is also responsible for periodically purging the icon cache.
class ASH_EXPORT ActiveNetworkIcon : public TrayNetworkStateObserver {
 public:
  enum class Type {
    kSingle,    // A single network icon in the tray.
    kPrimary,   // Multiple network icons: primary (non mobile) icon.
    kCellular,  // Multiple network icons: cellular icon.
  };

  explicit ActiveNetworkIcon(TrayNetworkStateModel* model);
  ~ActiveNetworkIcon() override;

  // Provides the a11y and tooltip strings for |type|. Output parameters can
  // be null.
  void GetConnectionStatusStrings(Type type,
                                  base::string16* a11y_name,
                                  base::string16* a11y_desc,
                                  base::string16* tooltip);

  // Returns a network icon (which may be empty) and sets |animating| if
  // provided.
  gfx::ImageSkia GetImage(Type type,
                          network_icon::IconType icon_type,
                          bool* animating);

 private:
  gfx::ImageSkia GetSingleImage(network_icon::IconType icon_type,
                                bool* animating);
  gfx::ImageSkia GetDualImagePrimary(network_icon::IconType icon_type,
                                     bool* animating);
  gfx::ImageSkia GetDualImageCellular(network_icon::IconType icon_type,
                                      bool* animating);
  gfx::ImageSkia GetDefaultImageImpl(
      const chromeos::network_config::mojom::NetworkStateProperties*
          default_network,
      network_icon::IconType icon_type,
      bool* animating);

  // Called when there is no default network., Provides an empty or disabled
  // wifi icon and sets |animating| if provided to false.
  gfx::ImageSkia GetDefaultImageForNoNetwork(network_icon::IconType icon_type,
                                             bool* animating);

  void SetCellularUninitializedMsg();

  // TrayNetworkStateObserver
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;

  void PurgeNetworkIconCache();
  const chromeos::network_config::mojom::NetworkStateProperties*
  GetNetworkForType(Type type);

  TrayNetworkStateModel* model_;

  int cellular_uninitialized_msg_ = 0;
  base::Time uninitialized_state_time_;
  base::OneShotTimer purge_timer_;
  base::WeakPtrFactory<ActiveNetworkIcon> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ActiveNetworkIcon);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
