// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_wifi_header_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

NetworkListWifiHeaderView::NetworkListWifiHeaderView(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListNetworkHeaderView(delegate, IDS_ASH_STATUS_TRAY_NETWORK_WIFI) {
}

NetworkListWifiHeaderView::~NetworkListWifiHeaderView() = default;

BEGIN_METADATA(NetworkListWifiHeaderView, NetworkListNetworkHeaderView)
END_METADATA

}  // namespace ash