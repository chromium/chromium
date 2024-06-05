// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_more_apps_button.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_app_count_icon.h"
#include "ash/system/phonehub/phone_hub_app_icon.h"
#include "ash/system/phonehub/phone_hub_app_loading_icon.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

// Appearance constants in DIPs
constexpr int kMoreAppsButtonRowPadding = 20;
constexpr int kMoreAppsButtonColumnPadding = 2;
constexpr int kMoreAppsButtonBackgroundRadius = 18;

// The app icons in the LoadingView stagger the start of the loading animation
// to make the appearance of a ripple.
constexpr int kAnimationLoadingIconStaggerDelayInMs = 100;

class MoreAppsButtonBackground : public views::Background {
 public:
  MoreAppsButtonBackground() = default;
  MoreAppsButtonBackground(const MoreAppsButtonBackground&) = delete;
  MoreAppsButtonBackground& operator=(const MoreAppsButtonBackground&) = delete;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
    canvas->DrawCircle(view->GetContentsBounds().CenterPoint(),
                       kMoreAppsButtonBackgroundRadius, flags);
  }
};

PhoneHubMoreAppsButton::PhoneHubMoreAppsButton() {
  InitLayout();
}

PhoneHubMoreAppsButton::PhoneHubMoreAppsButton(
    phonehub::AppStreamLauncherDataModel* app_stream_launcher_data_model,
    views::Button::PressedCallback callback)
    : views::Button(std::move(callback)),
      app_stream_launcher_data_model_(app_stream_launcher_data_model) {
  CHECK(app_stream_launcher_data_model_);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_FULL_APPS_LIST_BUTTON_TITLE));
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_FULL_APPS_LIST_BUTTON_TITLE));
  InitLayout();
  app_stream_launcher_data_model_->AddObserver(this);
}

PhoneHubMoreAppsButton::~PhoneHubMoreAppsButton() {
  if (app_stream_launcher_data_model_) {
    app_stream_launcher_data_model_->RemoveObserver(this);
  }
}

void PhoneHubMoreAppsButton::InitLayout() {
  table_layout_ = SetLayoutManager(std::make_unique<views::TableLayout>());
  table_layout_->AddPaddingColumn(/*horizontal_resize=*/3, /*width=*/0);
  table_layout_->AddColumn(views::LayoutAlignment::kStretch,
                           views::LayoutAlignment::kStretch,
                           views::TableLayout::kFixedSize,
                           views::TableLayout::ColumnSize::kUsePreferred,
                           /*fixed_width=*/0, /*min_width=*/0);
  table_layout_->AddPaddingColumn(views::TableLayout::kFixedSize,
                                  kMoreAppsButtonColumnPadding);
  table_layout_->AddColumn(views::LayoutAlignment::kStretch,
                           views::LayoutAlignment::kStretch,
                           views::TableLayout::kFixedSize,
                           views::TableLayout::ColumnSize::kUsePreferred,
                           /*fixed_width=*/0, /*min_width=*/0);
  table_layout_->AddPaddingColumn(/*horizontal_resize=*/3, /*width=*/0);
  table_layout_->AddRows(/*n=*/1, views::TableLayout::kFixedSize,
                         kMoreAppsButtonRowPadding);
  table_layout_->AddPaddingRow(views::TableLayout::kFixedSize,
                               kMoreAppsButtonColumnPadding);
  table_layout_->AddRows(/*n=*/1, views::TableLayout::kFixedSize,
                         kMoreAppsButtonRowPadding);

  SetEnabled(false);
  SetBackground(std::make_unique<MoreAppsButtonBackground>());
  if (!app_stream_launcher_data_model_) {
    return;
  }

  if (app_stream_launcher_data_model_->GetAppsListSortedByName()->empty()) {
    load_app_list_latency_ = base::TimeTicks::Now();
    StartLoadingAnimation(/*initial_delay=*/std::nullopt);
    SetEnabled(false);
    phone_hub_metrics::LogMoreAppsButtonAnimationOnShow(
        phone_hub_metrics::MoreAppsButtonLoadingState::kAnimationShown);
  } else {
    LoadAppList();
    SetEnabled(true);

    // Reset the latency variable to a default state when there is already an
    // apps list from the app stream launcher data model. This ensures we don't
    // log the latency metric since the loaded list should already be shown.
    load_app_list_latency_ = base::TimeTicks();
    phone_hub_metrics::LogMoreAppsButtonAnimationOnShow(
        phone_hub_metrics::MoreAppsButtonLoadingState::kMoreAppsButtonLoaded);
  }
}

void PhoneHubMoreAppsButton::StartLoadingAnimation(
    std::optional<base::TimeDelta> initial_delay) {
  app_loading_icons_.clear();
  RemoveAllChildViews();
  for (size_t i = 0; i < 4; i++) {
    AppLoadingIcon* app_loading_icon =
        AddChildView(new AppLoadingIcon(AppIcon::kSizeSmall));
    app_loading_icons_.push_back(app_loading_icon);

    size_t x = i % 2;
    size_t y = i / 2;
    base::TimeDelta stagger_delay =
        (x + y) * base::Milliseconds(kAnimationLoadingIconStaggerDelayInMs);
    if (initial_delay) {
      stagger_delay += *initial_delay;
    }

    app_loading_icon->StartLoadingAnimation(stagger_delay);
  }
}

void PhoneHubMoreAppsButton::StopLoadingAnimation() {
  for (AppLoadingIcon* app_loading_icon : app_loading_icons_) {
    app_loading_icon->StopLoadingAnimation();
  }
}

void PhoneHubMoreAppsButton::OnShouldShowMiniLauncherChanged() {}

void PhoneHubMoreAppsButton::OnAppListChanged() {
  LoadAppList();

  if (load_app_list_latency_ != base::TimeTicks()) {
    phone_hub_metrics::LogMoreAppsButtonFullAppsLatency(base::TimeTicks::Now() -
                                                        load_app_list_latency_);
    load_app_list_latency_ = base::TimeTicks();
  }
}

void PhoneHubMoreAppsButton::LoadAppList() {
  CHECK(app_stream_launcher_data_model_);
  app_loading_icons_.clear();
  RemoveAllChildViews();
  const std::vector<phonehub::Notification::AppMetadata>* app_list =
      app_stream_launcher_data_model_->GetAppsListSortedByName();
  if (!app_list->empty()) {
    auto app_count = std::min(app_list->size(), size_t{3});
    for (size_t i = 0; i < app_count; i++) {
      AddChildView(std::make_unique<AppIcon>(app_list->at(i).color_icon,
                                             AppIcon::kSizeSmall));
    }
  }

  AddChildView(std::make_unique<AppCountIcon>(app_list->size()));
  SetEnabled(true);
}

BEGIN_METADATA(PhoneHubMoreAppsButton)
END_METADATA

}  // namespace ash
