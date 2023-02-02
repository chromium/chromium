// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_recent_apps_view.h"

#include <memory>
#include <numeric>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_more_apps_button.h"
#include "ash/system/phonehub/phone_hub_recent_app_button.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/cxx17_backports.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using RecentAppsUiState =
    ::ash::phonehub::RecentAppsInteractionHandler::RecentAppsUiState;

// Appearance constants in DIPs.
constexpr gfx::Insets kRecentAppButtonFocusPadding(4);
constexpr int kHeaderLabelLineHeight = 48;
constexpr int kRecentAppButtonDefaultSpacing = 42;
constexpr int kRecentAppButtonMinSpacing = 20;
constexpr int kRecentAppButtonSize = 36;
constexpr int kMoreAppsButtonSize = 40;
constexpr int kRecentAppButtonsViewTopPadding = 4;
constexpr int kRecentAppButtonsViewHorizontalPadding = 6;
constexpr int kContentLabelLineHeightDip = 20;
constexpr int kContentTextLabelExtraMargin = 6;
constexpr auto kContentTextLabelInsetsDip =
    gfx::Insets::TLBR(0, kContentTextLabelExtraMargin, 0, 4);

// Typography.
constexpr int kHeaderTextFontSizeDip = 15;

// Max number of apps can be shown with more apps button
constexpr int kMaxAppsWithMoreAppsButton = 5;

// Sizing of more apps button.
constexpr gfx::Rect kMoreAppsButtonArea = gfx::Rect(57, 32);
constexpr int kMoreAppsButtonRadius = 16;

class HeaderView : public views::Label {
 public:
  HeaderView() {
    SetText(l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_RECENT_APPS_TITLE));
    SetLineHeight(kHeaderLabelLineHeight);
    SetFontList(font_list()
                    .DeriveWithSizeDelta(kHeaderTextFontSizeDip -
                                         font_list().GetFontSize())
                    .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
    SetAutoColorReadabilityEnabled(false);
    SetSubpixelRenderingEnabled(false);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
  ~HeaderView() override = default;
  HeaderView(HeaderView&) = delete;
  HeaderView operator=(HeaderView&) = delete;

  // views::View:
  const char* GetClassName() const override { return "HeaderView"; }
};

}  // namespace

class PhoneHubRecentAppsView::PlaceholderView : public views::Label {
 public:
  PlaceholderView() {
    SetText(
        l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_RECENT_APPS_PLACEHOLDER));
    SetLineHeight(kContentLabelLineHeightDip);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetAutoColorReadabilityEnabled(false);
    SetSubpixelRenderingEnabled(false);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    SetMultiLine(true);
    SetBorder(views::CreateEmptyBorder(kContentTextLabelInsetsDip));
  }
  ~PlaceholderView() override = default;
  PlaceholderView(PlaceholderView&) = delete;
  PlaceholderView operator=(PlaceholderView&) = delete;

  // views::View:
  const char* GetClassName() const override { return "ContentView"; }
};

PhoneHubRecentAppsView::PhoneHubRecentAppsView(
    phonehub::RecentAppsInteractionHandler* recent_apps_interaction_handler,
    phonehub::PhoneHubManager* phone_hub_manager)
    : recent_apps_interaction_handler_(recent_apps_interaction_handler),
      phone_hub_manager_(phone_hub_manager) {
  SetID(PhoneHubViewID::kPhoneHubRecentAppsView);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  AddChildView(std::make_unique<HeaderView>());
  recent_app_buttons_view_ =
      AddChildView(std::make_unique<RecentAppButtonsView>());
  placeholder_view_ = AddChildView(std::make_unique<PlaceholderView>());

  Update();
  recent_apps_interaction_handler_->AddObserver(this);
}

PhoneHubRecentAppsView::~PhoneHubRecentAppsView() {
  recent_apps_interaction_handler_->RemoveObserver(this);
}

const char* PhoneHubRecentAppsView::GetClassName() const {
  return "PhoneHubRecentAppsView";
}

PhoneHubRecentAppsView::RecentAppButtonsView::RecentAppButtonsView() {
  if (features::IsEcheLauncherIconsInMoreAppsButtonEnabled()) {
    views::BoxLayout* box_layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    box_layout->SetDefaultFlex(1);
    box_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
  }
}

PhoneHubRecentAppsView::RecentAppButtonsView::~RecentAppButtonsView() = default;

views::View* PhoneHubRecentAppsView::RecentAppButtonsView::AddRecentAppButton(
    std::unique_ptr<views::View> recent_app_button) {
  return AddChildView(std::move(recent_app_button));
}

// phonehub::RecentAppsInteractionHandler::Observer:
void PhoneHubRecentAppsView::OnRecentAppsUiStateUpdated() {
  Update();
}

// views::View:
gfx::Size PhoneHubRecentAppsView::RecentAppButtonsView::CalculatePreferredSize()
    const {
  int width = kTrayMenuWidth - kBubbleHorizontalSidePaddingDip * 2;
  int height = kRecentAppButtonSize + kRecentAppButtonFocusPadding.height() +
               kRecentAppButtonsViewTopPadding;
  if (features::IsEcheLauncherEnabled()) {
    height = kMoreAppsButtonSize + kRecentAppButtonFocusPadding.height() +
             kRecentAppButtonsViewTopPadding;
  }

  return gfx::Size(width, height);
}

void PhoneHubRecentAppsView::RecentAppButtonsView::Layout() {
  if (features::IsEcheLauncherIconsInMoreAppsButtonEnabled()) {
    views::View::Layout();
    return;
  }

  const gfx::Rect child_area = GetContentsBounds();
  views::View::Views visible_children;
  base::ranges::copy_if(
      children(), std::back_inserter(visible_children), [](const auto* v) {
        return v->GetVisible() && (v->GetPreferredSize().width() > 0);
      });
  if (visible_children.empty()) {
    return;
  }
  const int visible_child_width =
      std::accumulate(visible_children.cbegin(), visible_children.cend(), 0,
                      [](int width, const auto* v) {
                        return width + v->GetPreferredSize().width();
                      });

  int spacing = 0;
  if (visible_children.size() > 1) {
    spacing = (child_area.width() - visible_child_width -
               kRecentAppButtonsViewHorizontalPadding * 2) /
              (static_cast<int>(visible_children.size()) - 1);
    spacing = base::clamp(spacing, kRecentAppButtonMinSpacing,
                          kRecentAppButtonDefaultSpacing);
  }

  int child_x = child_area.x() + kRecentAppButtonsViewHorizontalPadding;
  int child_y = child_area.y() + kRecentAppButtonsViewTopPadding +
                kRecentAppButtonFocusPadding.bottom();
  for (auto* child : visible_children) {
    // Most recent apps be added to the left and shift right as the other apps
    // are streamed.
    int width = child->GetPreferredSize().width();
    child->SetBounds(child_x, child_y, width, child->GetHeightForWidth(width));
    child_x += width + spacing;
  }
}

const char* PhoneHubRecentAppsView::RecentAppButtonsView::GetClassName() const {
  return "RecentAppButtonView";
}

void PhoneHubRecentAppsView::RecentAppButtonsView::Reset() {
  RemoveAllChildViews();
}

void PhoneHubRecentAppsView::Update() {
  recent_app_buttons_view_->Reset();
  recent_app_button_list_.clear();

  RecentAppsUiState current_ui_state =
      recent_apps_interaction_handler_->ui_state();

  switch (current_ui_state) {
    case RecentAppsUiState::HIDDEN:
      placeholder_view_->SetVisible(false);
      SetVisible(false);
      break;
    case RecentAppsUiState::PLACEHOLDER_VIEW:
      recent_app_buttons_view_->SetVisible(false);
      placeholder_view_->SetVisible(true);
      SetVisible(true);
      break;
    case RecentAppsUiState::ITEMS_VISIBLE:
      std::vector<phonehub::Notification::AppMetadata> recent_apps_list =
          recent_apps_interaction_handler_->FetchRecentAppMetadataList();

      for (const auto& recent_app : recent_apps_list) {
        auto pressed_callback = base::BindRepeating(
            &phonehub::RecentAppsInteractionHandler::NotifyRecentAppClicked,
            base::Unretained(recent_apps_interaction_handler_), recent_app,
            eche_app::mojom::AppStreamLaunchEntryPoint::RECENT_APPS);
        recent_app_button_list_.push_back(
            recent_app_buttons_view_->AddRecentAppButton(
                std::make_unique<PhoneHubRecentAppButton>(
                    recent_app.icon, recent_app.visible_app_name,
                    pressed_callback)));
      }

      if (features::IsEcheLauncherEnabled() &&
          recent_app_button_list_.size() >= kMaxAppsWithMoreAppsButton) {
        recent_app_button_list_.push_back(
            recent_app_buttons_view_->AddRecentAppButton(
                GenerateMoreAppsButton()));
      }

      recent_app_buttons_view_->SetVisible(true);
      placeholder_view_->SetVisible(false);
      SetVisible(true);
      break;
  }
  PreferredSizeChanged();
}

void PhoneHubRecentAppsView::SwitchToFullAppsList() {
  if (!features::IsEcheLauncherEnabled()) {
    return;
  }

  phone_hub_manager_->GetAppStreamLauncherDataModel()
      ->SetShouldShowMiniLauncher(true);
}

std::unique_ptr<views::View> PhoneHubRecentAppsView::GenerateMoreAppsButton() {
  if (features::IsEcheLauncherIconsInMoreAppsButtonEnabled()) {
    return std::make_unique<PhoneHubMoreAppsButton>(
        phone_hub_manager_->GetAppStreamLauncherDataModel(),
        base::BindRepeating(&PhoneHubRecentAppsView::SwitchToFullAppsList,
                            base::Unretained(this)));
  }

  auto more_apps_button = std::make_unique<views::ImageButton>(
      base::BindRepeating(&PhoneHubRecentAppsView::SwitchToFullAppsList,
                          base::Unretained(this)));
  gfx::ImageSkia image = gfx::CreateVectorIcon(
      kPhoneHubFullAppsListIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor));
  more_apps_button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::ImageSkiaOperations::ExtractSubset(image, kMoreAppsButtonArea));
  more_apps_button->SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kMoreAppsButtonRadius));
  more_apps_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_FULL_APPS_LIST_BUTTON_TITLE));

  return more_apps_button;
}

}  // namespace ash
