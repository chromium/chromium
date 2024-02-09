// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_item_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

namespace {
using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::NetworkType;
}  // namespace

NetworkListItemView::NetworkListItemView(ViewClickListener* listener)
    : HoverHighlightView(listener) {}

NetworkListItemView::~NetworkListItemView() = default;

std::u16string NetworkListItemView::GetLabel() {
  if (network_properties_->type == NetworkType::kCellular) {
    ActivationStateType activation_state =
        network_properties_->type_state->get_cellular()->activation_state;
    if (activation_state == ActivationStateType::kActivating) {
      if (network_properties_->type_state->get_cellular()->has_nick_name) {
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATING_WITH_NICK_NAME,
            base::UTF8ToUTF16(network_properties_->name),
            base::UTF8ToUTF16(network_properties_->type_state->get_cellular()
                                  ->network_operator));
      }
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATING,
          base::UTF8ToUTF16(network_properties_->name));
    }
    if (network_properties_->type_state->get_cellular()->has_nick_name) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_ITEM_TITLE,
          base::UTF8ToUTF16(network_properties_->name),
          base::UTF8ToUTF16(network_properties_->type_state->get_cellular()
                                ->network_operator));
    }
  }
  // Otherwise just show the network name or 'Ethernet'.
  if (network_properties_->type == NetworkType::kEthernet)
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET);
  return base::UTF8ToUTF16(network_properties_->name);
}

BEGIN_METADATA(NetworkListItemView)
END_METADATA

}  // namespace ash