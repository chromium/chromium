// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/buttons.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/supervised/supervised_icon_string.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_view.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Constants used with QsRevamp.
constexpr int kManagedStateHighlightRadius = 16;
constexpr SkScalar kManagedStateCornerRadii[] = {16, 16, 16, 16,
                                                 16, 16, 16, 16};
constexpr auto kManagedStateBorderInsets = gfx::Insets::TLBR(0, 12, 0, 12);
constexpr gfx::Size kManagedStateImageSize(20, 20);

// Helper function for getting ContentLayerColor.
inline SkColor GetContentLayerColor(AshColorProvider::ContentLayerType type) {
  return AshColorProvider::Get()->GetContentLayerColor(type);
}

// Shows enterprise managed device information.
void ShowEnterpriseInfo(UnifiedSystemTrayController* controller,
                        const ui::Event& event) {
  quick_settings_metrics_util::RecordQsButtonActivated(
      QsButtonCatalogName::kManagedButton);
  controller->HandleEnterpriseInfoAction();
}

}  // namespace

ManagedStateView::ManagedStateView(PressedCallback callback,
                                   int label_id,
                                   const gfx::VectorIcon& icon)
    : Button(std::move(callback)), icon_(icon) {
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kUnifiedSystemInfoSpacing));

  if (features::IsQsRevampEnabled()) {
    // Image goes first.
    image_ = AddChildView(std::make_unique<views::ImageView>());
    label_ = AddChildView(std::make_unique<views::Label>());

    // Inset the icon and label so they aren't too close to the rounded corners.
    layout_manager->set_inside_border_insets(kManagedStateBorderInsets);
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
  } else {
    // Label goes first.
    label_ = AddChildView(std::make_unique<views::Label>());
    image_ = AddChildView(std::make_unique<views::ImageView>());
    // Shrink the label if needed so the icon fits.
    layout_manager->SetFlexForView(label_, 1);
  }

  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  label_->SetText(l10n_util::GetStringUTF16(label_id));

  if (features::IsQsRevampEnabled()) {
    image_->SetPreferredSize(kManagedStateImageSize);
  } else {
    image_->SetPreferredSize(
        gfx::Size(kUnifiedSystemInfoHeight, kUnifiedSystemInfoHeight));
  }

  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(
      features::IsQsRevampEnabled()
          ? cros_tokens::kCrosSysFocusRing
          : static_cast<ui::ColorId>(ui::kColorAshFocusRing));
  if (features::IsQsRevampEnabled()) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kManagedStateHighlightRadius);
  } else {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  }
}

views::View* ManagedStateView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltip events should be handled by this top-level view.
  return HitTestPoint(point) ? this : nullptr;
}

void ManagedStateView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  label_->SetEnabledColor(GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  image_->SetImage(gfx::CreateVectorIcon(
      *icon_, GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kIconColorSecondary)));
  if (features::IsQsRevampEnabled()) {
    const std::pair<SkColor, float> base_color_and_opacity =
        AshColorProvider::Get()->GetInkDropBaseColorAndOpacity();
    views::InkDrop::Get(this)->SetBaseColor(base_color_and_opacity.first);
  }
}

void ManagedStateView::PaintButtonContents(gfx::Canvas* canvas) {
  if (!features::IsQsRevampEnabled()) {
    return;
  }
  // Draw a button outline similar to ChannelIndicatorQuickSettingsView's
  // VersionButton outline.
  cc::PaintFlags flags;
  flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
      ColorProvider::ContentLayerType::kSeparatorColor));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  canvas->DrawPath(
      SkPath().addRoundRect(gfx::RectToSkRect(GetLocalBounds()),
                            kManagedStateCornerRadii, SkPathDirection::kCW),
      flags);
}

BEGIN_METADATA(ManagedStateView, views::Button)
END_METADATA

////////////////////////////////////////////////////////////////////////////////

EnterpriseManagedView::EnterpriseManagedView(
    UnifiedSystemTrayController* controller)
    : ManagedStateView(base::BindRepeating(&ShowEnterpriseInfo,
                                           base::Unretained(controller)),
                       IDS_ASH_ENTERPRISE_DEVICE_MANAGED_SHORT,
                       features::IsQsRevampEnabled()
                           ? kQuickSettingsManagedIcon
                           : kUnifiedMenuManagedIcon) {
  DCHECK(Shell::Get());
  SetID(VIEW_ID_QS_MANAGED_BUTTON);
  Shell::Get()->system_tray_model()->enterprise_domain()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  Update();
}

EnterpriseManagedView::~EnterpriseManagedView() {
  Shell::Get()->system_tray_model()->enterprise_domain()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void EnterpriseManagedView::SetNarrowLayout(bool narrow) {
  narrow_layout_ = narrow;
  Update();
}

void EnterpriseManagedView::OnDeviceEnterpriseInfoChanged() {
  Update();
}

void EnterpriseManagedView::OnEnterpriseAccountDomainChanged() {
  Update();
}

void EnterpriseManagedView::OnLoginStatusChanged(LoginStatus status) {
  Update();
}

void EnterpriseManagedView::Update() {
  EnterpriseDomainModel* model =
      Shell::Get()->system_tray_model()->enterprise_domain();
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  std::string enterprise_domain_manager = model->enterprise_domain_manager();
  std::string account_domain_manager = model->account_domain_manager();

  bool visible = session_controller->ShouldDisplayManagedUI() ||
                 model->active_directory_managed() ||
                 !enterprise_domain_manager.empty() ||
                 !account_domain_manager.empty();
  SetVisible(visible);

  if (!visible) {
    return;
  }

  // Display both device and user management if the feature is enabled.
  std::u16string managed_string;
  if (enterprise_domain_manager.empty() && account_domain_manager.empty()) {
    managed_string = l10n_util::GetStringFUTF16(
        IDS_ASH_ENTERPRISE_DEVICE_MANAGED, ui::GetChromeOSDeviceName());
  } else if (!enterprise_domain_manager.empty() &&
             !account_domain_manager.empty() &&
             enterprise_domain_manager != account_domain_manager) {
    managed_string =
        l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY_MULTIPLE,
                                   base::UTF8ToUTF16(enterprise_domain_manager),
                                   base::UTF8ToUTF16(account_domain_manager));
  } else {
    std::u16string display_domain_manager =
        enterprise_domain_manager.empty()
            ? base::UTF8ToUTF16(account_domain_manager)
            : base::UTF8ToUTF16(enterprise_domain_manager);
    managed_string = l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY,
                                                display_domain_manager);
    if (features::IsQsRevampEnabled()) {
      // Narrow layout uses the string "Managed" and wide layout uses the full
      // string "Managed by example.com".
      label()->SetText(narrow_layout_
                           ? l10n_util::GetStringUTF16(
                                 IDS_ASH_ENTERPRISE_DEVICE_MANAGED_SHORT)
                           : managed_string);
    }
  }
  SetTooltipText(managed_string);
}

BEGIN_METADATA(EnterpriseManagedView, ManagedStateView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////

SupervisedUserView::SupervisedUserView()
    : ManagedStateView(PressedCallback(),
                       IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL,
                       GetSupervisedUserIcon()) {
  SetID(VIEW_ID_QS_SUPERVISED_BUTTON);
  bool visible = Shell::Get()->session_controller()->IsUserChild();
  SetVisible(visible);
  if (visible) {
    SetTooltipText(GetSupervisedUserMessage());
  }

  // TODO(crbug/1026821) Add SupervisedUserView::ButtonPress() overload
  // to show a similar ui to enterprise managed accounts. Disable button
  // state for now.
  SetState(ButtonState::STATE_DISABLED);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
}

BEGIN_METADATA(SupervisedUserView, ManagedStateView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////

UserAvatarButton::UserAvatarButton(PressedCallback callback)
    : Button(std::move(callback)) {
  // QsRevamp doesn't use an avatar button. DCHECK because it's a map lookup.
  DCHECK(!features::IsQsRevampEnabled());
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(kUnifiedCircularButtonFocusPadding));
  AddChildView(CreateUserAvatarView(0 /* user_index */));
  SetTooltipText(GetUserItemAccessibleString(0 /* user_index */));
  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

  views::InstallCircleHighlightPathGenerator(this);
}

BEGIN_METADATA(UserAvatarButton, views::Button)
END_METADATA

}  // namespace ash
