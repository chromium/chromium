// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_more_apps_button.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_app_count_icon.h"
#include "ash/system/phonehub/phone_hub_small_app_icon.h"
#include "ash/system/phonehub/phone_hub_small_app_loading_icon.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

// Appearance constants in DIPs
constexpr int kMoreAppsButtonRowPadding = 20;
constexpr int kMoreAppsButtonColumnPadding = 2;
constexpr int kMoreAppsButtonBackgroundRadius = 120;

// Animation constants for loading card
constexpr float kAnimationLoadingCardOpacity = 1.0f;
constexpr int kAnimationLoadingCardDelayInMs = 83;
constexpr int kAnimationLoadingCardTransitDurationInMs = 200;
constexpr int kAnimationLoadingCardFreezeDurationInMs = 150;

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

  if (app_stream_launcher_data_model_->GetAppsListSortedByName()->empty()) {
    InitGlimmer();
    SetEnabled(false);
  } else {
    LoadAppList();
    SetEnabled(true);
  }

  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kMoreAppsButtonBackgroundRadius));
}

void PhoneHubMoreAppsButton::InitGlimmer() {
  for (auto i = 0; i < 4; i++) {
    auto* app_loading_icon = new SmallAppLoadingIcon();
    views::AnimationBuilder animation_builder;
    animation_builder.Once().SetOpacity(app_loading_icon,
                                        kAnimationLoadingCardOpacity);

    animation_builder.Repeatedly()
        .Offset(base::Milliseconds(kAnimationLoadingCardDelayInMs))
        .SetDuration(
            base::Milliseconds(kAnimationLoadingCardTransitDurationInMs))
        .SetOpacity(app_loading_icon, 0.0f, gfx::Tween::LINEAR)
        .Then()
        .Offset(base::Milliseconds(kAnimationLoadingCardFreezeDurationInMs))
        .Then()
        .SetDuration(
            base::Milliseconds(kAnimationLoadingCardTransitDurationInMs))
        .SetOpacity(app_loading_icon, kAnimationLoadingCardOpacity,
                    gfx::Tween::LINEAR);
    AddChildView(app_loading_icon);
  }
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