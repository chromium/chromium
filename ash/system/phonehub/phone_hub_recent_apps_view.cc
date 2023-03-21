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
#include "ash/system/phonehub/phone_connected_view.h"
#include "ash/system/phonehub/phone_hub_app_loading_icon.h"
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
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

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

constexpr int kRecentAppsHeaderSpacing = 220;

// The app icons in the LoadingView stagger the start of the loading animation
// to make the appearance of a ripple.
constexpr int kAnimationLoadingIconStaggerDelayInMs = 100;

// When the recent apps view is swapped in for the loading view, the opacities
// of the two views are animated to give the appearance of a fade-in.
constexpr int kLoadingViewFadeOutDurationInMs = 200;

void LayoutAppButtonsView(views::View* buttons_view) {
  const gfx::Rect child_area = buttons_view->GetContentsBounds();
  views::View::Views visible_children;
  base::ranges::copy_if(
      buttons_view->children(), std::back_inserter(visible_children),
      [](const auto* v) {
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

}  // namespace

PhoneHubRecentAppsView::HeaderView::HeaderView(
    views::ImageButton::PressedCallback callback) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(kRecentAppsHeaderSpacing);

  auto* label = AddChildView(std::make_unique<views::Label>());
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_RECENT_APPS_TITLE));
  label->SetLineHeight(kHeaderLabelLineHeight);
  label->SetFontList(label->font_list()
                         .DeriveWithSizeDelta(kHeaderTextFontSizeDip -
                                              label->font_list().GetFontSize())
                         .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  if (features::IsEcheNetworkConnectionStateEnabled()) {
    error_button_ =
        AddChildView(std::make_unique<views::ImageButton>(callback));
    gfx::ImageSkia image = gfx::CreateVectorIcon(
        kPhoneHubEcheErrorStatusIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorWarning));
    error_button_->SetImage(views::Button::STATE_NORMAL, image);
    error_button_->SetVisible(false);
  }
}

const char* PhoneHubRecentAppsView::HeaderView::GetClassName() const {
  return "HeaderView";
}

void PhoneHubRecentAppsView::HeaderView::SetErrorButtonVisible(
    bool is_visible) {
  if (error_button_) {
    error_button_->SetVisible(is_visible);
  }
}

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
    phonehub::PhoneHubManager* phone_hub_manager,
    PhoneConnectedView* connected_view)
    : recent_apps_interaction_handler_(recent_apps_interaction_handler),
      phone_hub_manager_(phone_hub_manager),
      connected_view_(connected_view) {
  SetID(PhoneHubViewID::kPhoneHubRecentAppsView);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  header_view_ = AddChildView(std::make_unique<HeaderView>(
      base::BindRepeating(&PhoneHubRecentAppsView::ShowConnectionErrorDialog,
                          base::Unretained(this))));

  // Group the non-header views under a view with FillLayout so that they stack
  // on top of each other when multiple are visible. This is important for
  // animating the transitions between views.
  auto* recent_apps_content = AddChildView(std::make_unique<views::View>());
  recent_apps_content->SetLayoutManager(std::make_unique<views::FillLayout>());

  recent_app_buttons_view_ = recent_apps_content->AddChildView(
      std::make_unique<RecentAppButtonsView>());
  placeholder_view_ =
      recent_apps_content->AddChildView(std::make_unique<PlaceholderView>());

  if (features::IsEcheNetworkConnectionStateEnabled()) {
    loading_view_ =
        recent_apps_content->AddChildView(std::make_unique<LoadingView>());
  }

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
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
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
  LayoutAppButtonsView(this);
}

const char* PhoneHubRecentAppsView::RecentAppButtonsView::GetClassName() const {
  return "RecentAppButtonView";
}

void PhoneHubRecentAppsView::RecentAppButtonsView::Reset() {
  RemoveAllChildViews();
}

PhoneHubRecentAppsView::LoadingView::LoadingView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetDefaultFlex(1);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  for (size_t i = 0; i < 5; i++) {
    app_loading_icons_.push_back(
        AddChildView(new AppLoadingIcon(AppIcon::kSizeNormal)));
  }
  more_apps_button_ = AddChildView(new PhoneHubMoreAppsButton());

  StartLoadingAnimation();
}

PhoneHubRecentAppsView::LoadingView::~LoadingView() = default;

gfx::Size PhoneHubRecentAppsView::LoadingView::CalculatePreferredSize() const {
  int width = kTrayMenuWidth - kBubbleHorizontalSidePaddingDip * 2;
  int height = kMoreAppsButtonSize + kRecentAppButtonFocusPadding.height() +
               kRecentAppButtonsViewTopPadding;

  return gfx::Size(width, height);
}

void PhoneHubRecentAppsView::LoadingView::Layout() {
  if (features::IsEcheLauncherIconsInMoreAppsButtonEnabled()) {
    views::View::Layout();
    return;
  }
  LayoutAppButtonsView(this);
}

const char* PhoneHubRecentAppsView::LoadingView::GetClassName() const {
  return "RecentAppLoadingView";
}

void PhoneHubRecentAppsView::LoadingView::StartLoadingAnimation() {
  for (size_t i = 0; i < app_loading_icons_.size(); i++) {
    app_loading_icons_[i]->StartLoadingAnimation(
        /*initial_delay=*/base::Milliseconds(
            i * kAnimationLoadingIconStaggerDelayInMs));
  }
  more_apps_button_->StartLoadingAnimation(/*initial_delay=*/base::Milliseconds(
      5 * kAnimationLoadingIconStaggerDelayInMs));
}

void PhoneHubRecentAppsView::LoadingView::StopLoadingAnimation() {
  for (AppLoadingIcon* app_loading_icon : app_loading_icons_) {
    app_loading_icon->StopLoadingAnimation();
  }
  more_apps_button_->StopLoadingAnimation();
}

void PhoneHubRecentAppsView::Update() {
  recent_app_buttons_view_->Reset();
  recent_app_button_list_.clear();

  RecentAppsUiState current_ui_state =
      recent_apps_interaction_handler_->ui_state();

  switch (current_ui_state) {
    case RecentAppsUiState::HIDDEN:
      placeholder_view_->SetVisible(false);
      if (loading_view_) {
        loading_view_->SetVisible(false);
      }
      SetVisible(false);
      break;
    case RecentAppsUiState::LOADING:
      if (features::IsEcheNetworkConnectionStateEnabled()) {
        placeholder_view_->SetVisible(false);
        loading_view_->SetVisible(true);
        header_view_->SetErrorButtonVisible(false);
        SetVisible(true);
        break;
      }
      [[fallthrough]];
    case RecentAppsUiState::CONNECTION_FAILED:
      if (features::IsEcheNetworkConnectionStateEnabled()) {
        placeholder_view_->SetVisible(false);
        loading_view_->SetVisible(true);
        header_view_->SetErrorButtonVisible(true);
        SetVisible(true);
        break;
      }
      [[fallthrough]];
    case RecentAppsUiState::PLACEHOLDER_VIEW:
      recent_app_buttons_view_->SetVisible(false);
      placeholder_view_->SetVisible(true);
      header_view_->SetErrorButtonVisible(false);
      if (loading_view_) {
        loading_view_->SetVisible(false);
      }
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
      if (features::IsEcheNetworkConnectionStateEnabled()) {
        header_view_->SetErrorButtonVisible(false);
        FadeOutLoadingView();
      }
      SetVisible(true);
      break;
  }
  PreferredSizeChanged();
}

void PhoneHubRecentAppsView::FadeOutLoadingView() {
  if (features::IsEcheNetworkConnectionStateEnabled() &&
      loading_view_->GetVisible()) {
    loading_view_->StopLoadingAnimation();
    recent_app_buttons_view_->SetVisible(true);

    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&LoadingView::SetVisible,
                                base::Unretained(loading_view_), false))
        .Once()
        .SetOpacity(loading_view_, 1.0f)
        .SetOpacity(recent_app_buttons_view_, 0.0f)
        .Then()
        .SetDuration(base::Milliseconds(kLoadingViewFadeOutDurationInMs))
        .SetOpacity(loading_view_, 0.0f, gfx::Tween::LINEAR)
        .SetOpacity(recent_app_buttons_view_, 1.0f, gfx::Tween::LINEAR);
  }
}

void PhoneHubRecentAppsView::SwitchToFullAppsList() {
  if (!features::IsEcheLauncherEnabled()) {
    return;
  }

  phone_hub_manager_->GetAppStreamLauncherDataModel()
      ->SetShouldShowMiniLauncher(true);
}

void PhoneHubRecentAppsView::ShowConnectionErrorDialog() {
  if (features::IsEcheNetworkConnectionStateEnabled()) {
    connected_view_->ShowAppStreamErrorDialog();
  }
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
