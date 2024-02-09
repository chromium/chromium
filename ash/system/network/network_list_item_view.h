// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_ITEM_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

class ViewClickListener;

// Base class used in configuring the view shown for a single network in
// the detailed Network page within the quick settings.
class ASH_EXPORT NetworkListItemView : public HoverHighlightView {
  METADATA_HEADER(NetworkListItemView, HoverHighlightView)

 public:
  NetworkListItemView(const NetworkListItemView&) = delete;
  NetworkListItemView& operator=(const NetworkListItemView&) = delete;
  ~NetworkListItemView() override;

  virtual void UpdateViewForNetwork(
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
          network_properties) = 0;

  const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
  network_properties() {
    return network_properties_;
  }

 protected:
  explicit NetworkListItemView(ViewClickListener* listener);

  std::u16string GetLabel();

  chromeos::network_config::mojom::NetworkStatePropertiesPtr
      network_properties_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_ITEM_VIEW_H_