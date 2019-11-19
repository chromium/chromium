// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/unified_network_detailed_view_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/network/network_list_view.h"
#include "ash/system/tray/detailed_view_delegate.h"

namespace ash {

UnifiedNetworkDetailedViewController::UnifiedNetworkDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
}

UnifiedNetworkDetailedViewController::~UnifiedNetworkDetailedViewController() =
    default;

views::View* UnifiedNetworkDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::NetworkListView(
      detailed_view_delegate_.get(),
      Shell::Get()->session_controller()->login_status());
  view_->Init();
  return view_;
}

}  // namespace ash
