// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/unified_cast_detailed_view_controller.h"

#include "ash/shell.h"
#include "ash/system/cast/tray_cast.h"
#include "ash/system/tray/detailed_view_delegate.h"

namespace ash {

UnifiedCastDetailedViewController::UnifiedCastDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {}

UnifiedCastDetailedViewController::~UnifiedCastDetailedViewController() =
    default;

views::View* UnifiedCastDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::CastDetailedView(detailed_view_delegate_.get());
  return view_;
}

}  // namespace ash
