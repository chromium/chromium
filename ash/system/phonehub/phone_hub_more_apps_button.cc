// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_more_apps_button.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_app_count_icon.h"
#include "ash/system/phonehub/phone_hub_small_app_icon.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

// Appearance constants in DIPs
constexpr int kMoreAppsButtonRowPadding = 20;
constexpr int kMoreAppsButtonColumnPadding = 2;
constexpr int kMoreAppsButtonBackgroundRadius = 16;

PhoneHubMoreAppsButton::PhoneHubMoreAppsButton(
    phonehub::AppStreamLauncherDataModel* app_stream_launcher_data_model,
    views::Button::PressedCallback callback)
    : views::Button(callback),
      app_stream_launcher_data_model_(app_stream_launcher_data_model) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_FULL_APPS_LIST_BUTTON_TITLE));
  InitLayout();
  app_stream_launcher_data_model_->AddObserver(this);
}

PhoneHubMoreAppsButton::~PhoneHubMoreAppsButton() {
  app_stream_launcher_data_model_->RemoveObserver(this);
}

// TODO: Add a loading state before all apps loads
void PhoneHubMoreAppsButton::InitLayout() {
  table_layout_ = SetLayoutManager(std::make_unique<views::TableLayout>());
  table_layout_->AddColumn(views::LayoutAlignment::kStretch,
                           views::LayoutAlignment::kStretch, 1.0,
                           views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  table_layout_->AddPaddingColumn(views::TableLayout::kFixedSize,
                                  kMoreAppsButtonColumnPadding);
  table_layout_->AddColumn(views::LayoutAlignment::kStretch,
                           views::LayoutAlignment::kStretch, 1.0,
                           views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  table_layout_->AddRows(1, kMoreAppsButtonRowPadding);
  table_layout_->AddPaddingRow(views::TableLayout::kFixedSize,
                               kMoreAppsButtonColumnPadding);
  table_layout_->AddRows(1, kMoreAppsButtonRowPadding);

  OnAppListChanged();

  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kMoreAppsButtonBackgroundRadius));
}

void PhoneHubMoreAppsButton::OnShouldShowMiniLauncherChanged() {}

void PhoneHubMoreAppsButton::OnAppListChanged() {
  LoadAppList();
}

void PhoneHubMoreAppsButton::LoadAppList() {
  RemoveAllChildViews();
  const std::vector<phonehub::Notification::AppMetadata>* app_list =
      app_stream_launcher_data_model_->GetAppsListSortedByName();
  if (!app_list->empty()) {
    auto app_count = std::min(app_list->size(), size_t{3});
    for (size_t i = 0; i < app_count; i++) {
      AddChildView(std::make_unique<SmallAppIcon>(app_list->at(i).icon));
    }
  }

  AddChildView(std::make_unique<AppCountIcon>(app_list->size()));
}

BEGIN_METADATA(PhoneHubMoreAppsButton, views::Button)
END_METADATA

}  // namespace ash