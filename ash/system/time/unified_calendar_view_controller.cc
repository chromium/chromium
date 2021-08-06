// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/unified_calendar_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedCalendarViewController::UnifiedCalendarViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {}

UnifiedCalendarViewController::~UnifiedCalendarViewController() {}

// TODO(jiaminc@): return calendar view once it's implemented.
views::View* UnifiedCalendarViewController::CreateView() {
  return nullptr;
}

std::u16string UnifiedCalendarViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_CALENDAR_BUBBLE_ACCESSIBLE_DESCRIPTION);
}

}  // namespace ash
