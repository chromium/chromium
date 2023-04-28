// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/unified_cast_detailed_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/cast/cast_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedCastDetailedViewController::UnifiedCastDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {}

UnifiedCastDetailedViewController::~UnifiedCastDetailedViewController() =
    default;

std::unique_ptr<views::View> UnifiedCastDetailedViewController::CreateView() {
  DCHECK(!view_);
  auto view = std::make_unique<CastDetailedView>(detailed_view_delegate_.get());
  view_ = view.get();
  return view;
}

std::u16string UnifiedCastDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_CAST_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

}  // namespace ash
