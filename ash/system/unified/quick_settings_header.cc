// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_header.h"

#include <memory>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/update_model.h"
#include "ash/system/supervised/supervised_icon_string.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/update/eol_notice_quick_settings_view.h"
#include "ash/system/update/extended_updates_notice_quick_settings_view.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// The bottom padding is 0 so this view is flush with the feature tiles.
constexpr auto kHeaderPadding = gfx::Insets::TLBR(16, 16, 0, 16);

// Horizontal space between header buttons.
constexpr int kButtonSpacing = 8;

// Header button size when the button is narrow (e.g. two column layout).
constexpr gfx::Size kNarrowButtonSize(180, 32);

// Header button size when the button is wide (e.g. one column layout).
constexpr gfx::Size kWideButtonSize(408, 32);

constexpr int kManagedStateCornerRadius = 16;
constexpr float kManagedStateStrokeWidth = 1.0f;
constexpr auto kManagedStateBorderInsets = gfx::Insets::TLBR(0, 12, 0, 12);
constexpr gfx::Size kManagedStateImageSize(20, 20);

// Shows account settings in OS settings, which includes a link to install or
// open the Family Link app to see supervision settings.
void ShowAccountSettings() {
  quick_settings_metrics_util::RecordQsButtonActivated(
      QsButtonCatalogName::kSupervisedButton);
  Shell::Get()->system_tray_model()->client()->ShowAccountSettings();
}

}  // namespace

class QuickSettingsHeader::ManagedStateView : public views::Button {
  METADATA_HEADER(ManagedStateView, views::Button)

 public:
  ManagedStateView(base::OnceClosure callback,
                   int label_id,
                   const gfx::VectorIcon& icon)
      : views::Button(std::move(callback)), icon_(icon) {
    auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kUnifiedSystemInfoSpacing));

    // Image goes first.
    image_ = AddChildView(std::make_unique<views::ImageView>());
    label_ = AddChildView(std::make_unique<views::Label>());

    // Inset the icon and label so they aren't too close to the rounded corners.
    layout_manager->set_inside_border_insets(kManagedStateBorderInsets);
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetSubpixelRenderingEnabled(false);
    label_->SetText(l10n_util::GetStringUTF16(label_id));

    image_->SetPreferredSize(kManagedStateImageSize);
    label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
    TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody2,
                                          *label_);
    SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kManagedStateCornerRadius);
  }

  ManagedStateView(const ManagedStateView&) = delete;

  ManagedStateView& operator=(const ManagedStateView&) = delete;

  ~ManagedStateView() override = default;

  views::Label* label() { return label_; }

 private:
  // views::Button:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override {
    // Tooltip events should be handled by this top-level view.
    return HitTestPoint(point) ? this : nullptr;
  }

  // views::Button:
  // TODO(b/311234537): consider to remove this override and set color ids.
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    const std::pair<SkColor, float> base_color_and_opacity =
        AshColorProvider::Get()->GetInkDropBaseColorAndOpacity();
    views::InkDrop::Get(this)->SetBaseColor(base_color_and_opacity.first);
    image_->SetImage(gfx::CreateVectorIcon(
        *icon_,
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurfaceVariant)));
  }

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // Draw a button outline similar to ChannelIndicatorQuickSettingsView's
    // VersionButton outline.
    cc::PaintFlags flags;
    flags.setColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSeparator));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kManagedStateStrokeWidth);
    flags.setAntiAlias(true);
    const float half_stroke_width = kManagedStateStrokeWidth / 2.0f;
    gfx::RectF bounds(GetLocalBounds());
    bounds.Inset(half_stroke_width);
    canvas->DrawRoundRect(bounds, kManagedStateCornerRadius, flags);
  }

  // Owned by views hierarchy.
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> image_ = nullptr;

  const raw_ref<const gfx::VectorIcon> icon_;
};

BEGIN_METADATA(QuickSettingsHeader, ManagedStateView)
END_METADATA

class QuickSettingsHeader::EnterpriseManagedView
    : public ManagedStateView,
      public EnterpriseDomainObserver,
      public SessionObserver {
  METADATA_HEADER(EnterpriseManagedView, ManagedStateView)

 public:
  explicit EnterpriseManagedView(UnifiedSystemTrayController* controller)
      : ManagedStateView(base::BindRepeating(
                             &QuickSettingsHeader::ShowEnterpriseInfo,
                             base::Unretained(controller),
                             base::FeatureList::IsEnabled(
                                 ash::features::kImprovedManagementDisclosure)),
                         IDS_ASH_ENTERPRISE_DEVICE_MANAGED_SHORT,
                         kQuickSettingsManagedIcon) {
    DCHECK(Shell::Get());
    SetID(VIEW_ID_QS_MANAGED_BUTTON);
    SetProperty(views::kElementIdentifierKey, kEnterpriseManagedView);
    Shell::Get()->system_tray_model()->enterprise_domain()->AddObserver(this);
    Shell::Get()->session_controller()->AddObserver(this);
    Update();
  }

  EnterpriseManagedView(const EnterpriseManagedView&) = delete;
  EnterpriseManagedView& operator=(const EnterpriseManagedView&) = delete;

  ~EnterpriseManagedView() override {
    Shell::Get()->system_tray_model()->enterprise_domain()->RemoveObserver(
        this);
    Shell::Get()->session_controller()->RemoveObserver(this);
  }

  // Adjusts the layout for a narrower appearance, using a shorter label for
  // the button.
  void SetNarrowLayout(bool narrow) {
    narrow_layout_ = narrow;
    Update();
  }

 private:
  // EnterpriseDomainObserver:
  void OnDeviceEnterpriseInfoChanged() override { Update(); }
  void OnEnterpriseAccountDomainChanged() override { Update(); }

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override { Update(); }

  // Updates the view visibility and displayed string.
  void Update() {
    EnterpriseDomainModel* model =
        Shell::Get()->system_tray_model()->enterprise_domain();
    SessionControllerImpl* session_controller =
        Shell::Get()->session_controller();
    const std::string enterprise_domain_manager =
        model->enterprise_domain_manager();
    const std::string account_domain_manager = model->account_domain_manager();

    const bool visible = session_controller->ShouldDisplayManagedUI() ||
                         !enterprise_domain_manager.empty() ||
                         !account_domain_manager.empty();
    SetVisible(visible);

    if (!visible) {
      return;
    }

    // Display device and user management based on the enterprise enrollment
    // state.
    std::u16string managed_string;
    if (enterprise_domain_manager.empty() && account_domain_manager.empty()) {
      managed_string = l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED, ui::GetChromeOSDeviceName());
    } else if (!enterprise_domain_manager.empty() &&
               !account_domain_manager.empty() &&
               enterprise_domain_manager != account_domain_manager) {
      managed_string = l10n_util::GetStringFUTF16(
          IDS_ASH_SHORT_MANAGED_BY_MULTIPLE,
          base::UTF8ToUTF16(enterprise_domain_manager),
          base::UTF8ToUTF16(account_domain_manager));
    } else {
      const std::u16string display_domain_manager =
          enterprise_domain_manager.empty()
              ? base::UTF8ToUTF16(account_domain_manager)
              : base::UTF8ToUTF16(enterprise_domain_manager);
      managed_string = l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY,
                                                  display_domain_manager);
      // Narrow layout uses the string "Managed" and wide layout uses the full
      // string "Managed by example.com".
      label()->SetText(narrow_layout_
                           ? l10n_util::GetStringUTF16(
                                 IDS_ASH_ENTERPRISE_DEVICE_MANAGED_SHORT)
                           : managed_string);
    }
    SetTooltipText(managed_string);
  }

  // See SetNarrowLayout().
  bool narrow_layout_ = false;
};

BEGIN_METADATA(QuickSettingsHeader, EnterpriseManagedView)
END_METADATA

QuickSettingsHeader::QuickSettingsHeader(
    UnifiedSystemTrayController* controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kHeaderPadding,
      kButtonSpacing));

  enterprise_managed_view_ =
      AddChildView(std::make_unique<EnterpriseManagedView>(controller));

  // A view that shows whether the user is supervised or a child.
  supervised_view_ = AddChildView(std::make_unique<ManagedStateView>(
      base::BindRepeating(&ShowAccountSettings),
      IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL, GetSupervisedUserIcon()));
  supervised_view_->SetID(VIEW_ID_QS_SUPERVISED_BUTTON);
  const bool visible =
      Shell::Get()->system_tray_model()->IsInUserChildSession();
  supervised_view_->SetVisible(visible);
  if (visible) {
    supervised_view_->SetTooltipText(GetSupervisedUserMessage());
  }

  const bool is_active_state =
      Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE;
  if (is_active_state) {
    if (Shell::Get()->system_tray_model()->update_model()->show_eol_notice()) {
      eol_notice_ =
          AddChildView(std::make_unique<EolNoticeQuickSettingsView>());
    } else if (Shell::Get()
                   ->system_tray_model()
                   ->update_model()
                   ->show_extended_updates_notice()) {
      extended_updates_notice_ = AddChildView(
          std::make_unique<ExtendedUpdatesNoticeQuickSettingsView>());
    }
  }

  // If the release track is not "stable" then show the channel indicator UI.
  auto channel = Shell::Get()->shell_delegate()->GetChannel();
  if (channel_indicator_utils::IsDisplayableChannel(channel) && !eol_notice_ &&
      !extended_updates_notice_) {
    channel_view_ =
        AddChildView(std::make_unique<ChannelIndicatorQuickSettingsView>(
            channel, is_active_state && Shell::Get()
                                            ->system_tray_model()
                                            ->client()
                                            ->IsUserFeedbackEnabled()));
  }

  UpdateVisibilityAndLayout();
}

QuickSettingsHeader::~QuickSettingsHeader() = default;

void QuickSettingsHeader::ChildVisibilityChanged(views::View* child) {
  UpdateVisibilityAndLayout();
}

views::View* QuickSettingsHeader::GetManagedButtonForTest() {
  return enterprise_managed_view_;
}

views::View* QuickSettingsHeader::GetSupervisedButtonForTest() {
  return supervised_view_;
}

views::Label* QuickSettingsHeader::GetManagedButtonLabelForTest() {
  return enterprise_managed_view_->label();
}

views::Label* QuickSettingsHeader::GetSupervisedButtonLabelForTest() {
  return supervised_view_->label();
}

views::View* QuickSettingsHeader::GetExtendedUpdatesViewForTest() {
  return extended_updates_notice_;
}

void QuickSettingsHeader::UpdateVisibilityAndLayout() {
  // The managed view and the supervised view are never shown together.
  DCHECK(!enterprise_managed_view_->GetVisible() ||
         !supervised_view_->GetVisible());

  // The notice views are never shown together.
  DCHECK(!!channel_view_ + !!eol_notice_ + !!extended_updates_notice_ <= 1);

  // Make `this` view visible if a child is visible.
  bool managed_view_visible =
      enterprise_managed_view_->GetVisible() || supervised_view_->GetVisible();
  bool notice_view_visible =
      channel_view_ || eol_notice_ || extended_updates_notice_;

  SetVisible(managed_view_visible || notice_view_visible);

  // Update button sizes for one column vs. two columns.
  bool two_columns = managed_view_visible && notice_view_visible;
  gfx::Size size = two_columns ? kNarrowButtonSize : kWideButtonSize;
  enterprise_managed_view_->SetPreferredSize(size);
  supervised_view_->SetPreferredSize(size);
  if (channel_view_) {
    channel_view_->SetPreferredSize(size);
  }
  if (eol_notice_) {
    eol_notice_->SetPreferredSize(size);
  }
  if (extended_updates_notice_) {
    extended_updates_notice_->SetPreferredSize(size);
  }

  // Use custom narrow layouts when two columns are showing.
  enterprise_managed_view_->SetNarrowLayout(two_columns);
  if (channel_view_) {
    channel_view_->SetNarrowLayout(two_columns);
  }
  if (eol_notice_) {
    eol_notice_->SetNarrowLayout(two_columns);
  }
  if (extended_updates_notice_) {
    extended_updates_notice_->SetNarrowLayout(two_columns);
  }
}

// static
void QuickSettingsHeader::ShowEnterpriseInfo(
    UnifiedSystemTrayController* controller,
    bool showManagementDisclosureDialog) {
  quick_settings_metrics_util::RecordQsButtonActivated(
      QsButtonCatalogName::kManagedButton);
  // Show the new disclosure when on the login/lock screen and feature is
  // enabled.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked() &&
      showManagementDisclosureDialog) {
    LockScreen::Get()->ShowManagementDisclosureDialog();
  } else {
    controller->HandleEnterpriseInfoAction();
  }
}

BEGIN_METADATA(QuickSettingsHeader)
END_METADATA

}  // namespace ash
