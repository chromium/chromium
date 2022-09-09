// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_button_base.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "base/check.h"
#include "ui/views/controls/button/button.h"

namespace ash {

namespace {

// Returns the corresponding view id of the `button_catalog_name`.
ViewID GetViewIDFromCatalogName(const QsButtonCatalogName button_catalog_name) {
  switch (button_catalog_name) {
    case QsButtonCatalogName::kSignOutButton:
      return VIEW_ID_QS_SIGN_OUT_BUTTON;
    case QsButtonCatalogName::kLockButton:
      return VIEW_ID_QS_LOCK_BUTTON;
    case QsButtonCatalogName::kSettingsButton:
      return VIEW_ID_QS_SETTINGS_BUTTON;
    case QsButtonCatalogName::kDateViewButton:
      return VIEW_ID_QS_DATE_VIEW_BUTTON;
    case QsButtonCatalogName::kPowerButton:
      return VIEW_ID_QS_POWER_BUTTON;
    case QsButtonCatalogName::kBatteryButton:
      return VIEW_ID_QS_BATTERY_BUTTON;
    case QsButtonCatalogName::kManageButton:
      return VIEW_ID_QS_MANAGE_BUTTON;
    case QsButtonCatalogName::kAvatarButton:
      return VIEW_ID_QS_USER_AVATAR_BUTTON;
    case QsButtonCatalogName::kCollapseButton: {
      if (!features::IsQsRevampEnabled())
        return VIEW_ID_QS_COLLAPSE_BUTTON;
      NOTREACHED();
      break;
    }
    case QsButtonCatalogName::kUnknown:
      NOTREACHED();
  }
  return VIEW_ID_NONE;
}

}  // namespace

QuickSettingsButtonDelegate::QuickSettingsButtonDelegate(
    const QsButtonCatalogName button_catalog_name,
    views::Button::PressedCallback on_button_activated_callback)
    : catalog_name_(button_catalog_name),
      callback_(std::move(on_button_activated_callback)) {}

QuickSettingsButtonDelegate::~QuickSettingsButtonDelegate() = default;

std::unique_ptr<views::Button> QuickSettingsButtonDelegate::CreateButton() {
  auto button = BuildButton(
      base::BindRepeating(&QuickSettingsButtonDelegate::OnButtonActivated,
                          weak_factory_.GetWeakPtr()));
  button->SetID(GetViewIDFromCatalogName(catalog_name_));
  return button;
}

void QuickSettingsButtonDelegate::OnButtonActivated(const ui::Event& event) {
  // Logs the metrics first here, in case that the object get destructed in
  // the `OnButtonPressed` method.
  quick_settings_metrics_util::RecordQsButtonActivated(catalog_name_, event);
  callback_.Run(event);
}

}  // namespace ash
