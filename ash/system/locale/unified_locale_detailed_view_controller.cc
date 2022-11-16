// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/unified_locale_detailed_view_controller.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/locale/locale_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedLocaleDetailedViewController::UnifiedLocaleDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {}

UnifiedLocaleDetailedViewController::~UnifiedLocaleDetailedViewController() =
    default;

std::unique_ptr<views::View> UnifiedLocaleDetailedViewController::CreateView() {
  DCHECK(!view_);
  auto view =
      std::make_unique<LocaleDetailedView>(detailed_view_delegate_.get());
  view_ = view.get();
  return view;
}

std::u16string UnifiedLocaleDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_LOCALE_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

}  // namespace ash
