// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_mobile_header_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

NetworkListMobileHeaderView::NetworkListMobileHeaderView(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListNetworkHeaderView(delegate,
                                   IDS_ASH_STATUS_TRAY_NETWORK_MOBILE,
                                   kPhoneHubPhoneIcon) {}

NetworkListMobileHeaderView::~NetworkListMobileHeaderView() = default;

BEGIN_METADATA(NetworkListMobileHeaderView, NetworkListNetworkHeaderView)
END_METADATA

}  // namespace ash