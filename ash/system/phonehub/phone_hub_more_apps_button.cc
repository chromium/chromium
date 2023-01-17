// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_more_apps_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_small_app_icon.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

// Appearance constants in DIPs
constexpr int kMoreAppsButtonRowPadding = 20;
constexpr int kMoreAppsButtonColumnPadding = 2;
constexpr int kMoreAppsButtonBackgroundRadius = 16;

PhoneHubMoreAppsButton::PhoneHubMoreAppsButton(
    phonehub::AppStreamLauncherDataModel* app_stream_launcher_data_model)
    : app_stream_launcher_data_model_(app_stream_launcher_data_model) {
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
      app_stream_launcher_data_model_->GetAppsList();
  if (!app_list->empty()) {
    auto app_count = std::min(app_list->size(), size_t{4});
    for (size_t i = 0; i < app_count; i++) {
      AddChildView(std::make_unique<SmallAppIcon>(app_list->at(i).icon));
    }
  }
}

BEGIN_METADATA(PhoneHubMoreAppsButton, views::View)
END_METADATA

}  // namespace ash