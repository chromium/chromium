// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
#define ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace ui {
class ColorProvider;
}  // namespace ui

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

  ActiveNetworkIcon(const ActiveNetworkIcon&) = delete;
  ActiveNetworkIcon& operator=(const ActiveNetworkIcon&) = delete;

  ~ActiveNetworkIcon() override;

  // Provides the a11y and tooltip strings for |type|. Output parameters can
  // be null.
  void GetConnectionStatusStrings(Type type,
                                  std::u16string* a11y_name,
                                  std::u16string* a11y_desc,
                                  std::u16string* tooltip);

  // Returns a network icon (which may be empty) and sets |animating| if
  // provided.
  gfx::ImageSkia GetImage(const ui::ColorProvider* color_provider,
                          Type type,
                          network_icon::IconType icon_type,
                          bool* animating);

  void PurgeNetworkIconCache();

 private:
  gfx::ImageSkia GetSingleImage(const ui::ColorProvider* color_provider,
                                network_icon::IconType icon_type,
                                bool* animating);
  gfx::ImageSkia GetDualImagePrimary(const ui::ColorProvider* color_provider,
                                     network_icon::IconType icon_type,
                                     bool* animating);
  gfx::ImageSkia GetDualImageCellular(const ui::ColorProvider* color_provider,
                                      network_icon::IconType icon_type,
                                      bool* animating);
  gfx::ImageSkia GetDefaultImageImpl(
      const ui::ColorProvider* color_provider,
      const chromeos::network_config::mojom::NetworkStateProperties*
          default_network,
      network_icon::IconType icon_type,
      bool* animating);

  // Called when there is no default network. Provides an empty or disabled
  // wifi icon and sets |animating| if provided to false.
  gfx::ImageSkia GetDefaultImageForNoNetwork(
      const ui::ColorProvider* color_provider,
      network_icon::IconType icon_type,
      bool* animating);

  void SetCellularUninitializedMsg();

  // TrayNetworkStateObserver
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;
  void DeviceStateListChanged() override;

  const chromeos::network_config::mojom::NetworkStateProperties*
  GetNetworkForType(Type type);

  raw_ptr<TrayNetworkStateModel> model_;

  int cellular_uninitialized_msg_ = 0;
  base::Time uninitialized_state_time_;
  base::OneShotTimer purge_timer_;
  base::WeakPtrFactory<ActiveNetworkIcon> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
