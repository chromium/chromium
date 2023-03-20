// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_info_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/clock_observer.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/update_model.h"
#include "ash/system/power/adaptive_charging_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/buttons.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/update/eol_notice_quick_settings_view.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

namespace {

std::u16string FormatDate(const base::Time& time) {
  // Use 'short' month format (e.g., "Oct") followed by non-padded day of
  // month (e.g., "2", "10").
  return base::TimeFormatWithPattern(time, "LLLd");
}

std::u16string FormatDayOfWeek(const base::Time& time) {
  // Use 'short' day of week format (e.g., "Wed").
  return base::TimeFormatWithPattern(time, "EEE");
}

// Returns whether SmartChargingUI should be used.
bool UseSmartChargingUI() {
  return ash::features::IsAdaptiveChargingEnabled() &&
         Shell::Get()
             ->adaptive_charging_controller()
             ->is_adaptive_delaying_charge();
}

// A view that shows current date in short format e.g. "Mon, Mar 12". It updates
// by observing ClockObserver.
class DateView : public views::Button, public ClockObserver {
 public:
  explicit DateView(UnifiedSystemTrayController* controller);

  DateView(const DateView&) = delete;
  DateView& operator=(const DateView&) = delete;

  ~DateView() override;

  // views::Button:
  const char* GetClassName() const override { return "DateView"; }
  void OnThemeChanged() override;

 private:
  friend class ash::UnifiedSystemInfoView;

  // Callback called when this is pressed.
  void OnButtonPressed(const ui::Event& event);

  void Update();

  // views::Button:
  gfx::Insets GetInsets() const override;

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

  // Owned by the views hierarchy.
  views::Label* label_;

  // Unowned.
  UnifiedSystemTrayController* const controller_;
};

DateView::DateView(UnifiedSystemTrayController* controller)
    : Button(base::BindRepeating(&DateView::OnButtonPressed,
                                 base::Unretained(this))),
      label_(AddChildView(std::make_unique<views::Label>())),
      controller_(controller) {
  SetID(VIEW_ID_QS_DATE_VIEW_BUTTON);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  Update();

  Shell::Get()->system_tray_model()->clock()->AddObserver(this);
  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
}

DateView::~DateView() {
  Shell::Get()->system_tray_model()->clock()->RemoveObserver(this);
}

void DateView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  label_->SetEnabledColor(color_provider->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary));
}

void DateView::OnButtonPressed(const ui::Event& event) {
  quick_settings_metrics_util::RecordQsButtonActivated(
      QsButtonCatalogName::kDateViewButton);

  if (controller_->IsExpanded()) {
    controller_->ShowCalendarView(
        calendar_metrics::CalendarViewShowSource::kDateView,
        calendar_metrics::GetEventType(event));
    return;
  }

  controller_->HandleOpenDateTimeSettingsAction();
}

void DateView::Update() {
  base::Time now = base::Time::Now();
  label_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DATE, FormatDayOfWeek(now), FormatDate(now)));

  SetAccessibleName(
      l10n_util::GetStringFUTF16(IDS_ASH_CALENDAR_ENTRY_ACCESSIBLE_DESCRIPTION,
                                 TimeFormatFriendlyDateAndTime(now)));
}

gfx::Insets DateView::GetInsets() const {
  // This padding provides room to render the focus ring around this button.
  return kUnifiedSystemInfoDateViewPadding;
}

void DateView::OnDateFormatChanged() {}

void DateView::OnSystemClockTimeUpdated() {
  Update();
}

void DateView::OnSystemClockCanSetTimeChanged(bool can_set_time) {}

void DateView::Refresh() {
  Update();
}

}  // namespace

// A view that contains date, battery status, and whether the device
// is enterprise managed.
class ManagementPowerDateComboView : public views::View {
 public:
  explicit ManagementPowerDateComboView(
      UnifiedSystemTrayController* controller) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kUnifiedSystemInfoSpacing));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    date_view_ = AddChildView(std::make_unique<DateView>(controller));

    if (PowerStatus::Get()->IsBatteryPresent()) {
      separator_view_ = AddChildView(std::make_unique<views::Separator>());
      separator_view_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
      separator_view_->SetPreferredLength(kUnifiedSystemInfoHeight);

      const bool use_smart_charging_ui = UseSmartChargingUI();
      if (use_smart_charging_ui) {
        AddChildView(std::make_unique<BatteryIconView>(controller));
      }
      AddChildView(std::make_unique<BatteryLabelView>(controller,
                                                      use_smart_charging_ui));
    }

    auto* spacing = AddChildView(std::make_unique<views::View>());
    layout->SetFlexForView(spacing, 1);

    enterprise_managed_view_ =
        AddChildView(std::make_unique<EnterpriseManagedView>(controller));
    supervised_view_ = AddChildView(std::make_unique<SupervisedUserView>());
  }
  ManagementPowerDateComboView(const ManagementPowerDateComboView&) = delete;
  ManagementPowerDateComboView& operator=(const ManagementPowerDateComboView&) =
      delete;
  ~ManagementPowerDateComboView() override = default;

  bool IsSupervisedVisibleForTesting() {
    return supervised_view_->GetVisible();
  }

 private:
  friend class UnifiedSystemInfoView;

  // Pointer to the actual child view is maintained for unit testing, owned by
  // `ManagementPowerDateComboView`.
  EnterpriseManagedView* enterprise_managed_view_ = nullptr;

  // Pointer to the actual child view is maintained for unit testing, owned by
  // `ManagementPowerDateComboView`.
  SupervisedUserView* supervised_view_ = nullptr;

  // Separator between date and battery views, owned by
  // `ManagementPowerDateComboView`.
  views::Separator* separator_view_ = nullptr;

  // Pointer to the actual child view is maintained for unit testing, owned by
  // `ManagementPowerDateComboView`.
  DateView* date_view_ = nullptr;
};

UnifiedSystemInfoView::UnifiedSystemInfoView(
    UnifiedSystemTrayController* controller) {
  // Layout for the overall UnifiedSystemInfoView.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kUnifiedSystemInfoViewPadding,
      kUnifiedSystemInfoSpacing));
  // Allow children to stretch to fill the whole width of the parent. Some
  // direct children are kStart aligned, others are kCenter aligned.
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // Construct a ManagementPowerDateComboView and save off a raw pointer, to
  // facilitate introspection needed for unit tests.
  combo_view_ =
      AddChildView(std::make_unique<ManagementPowerDateComboView>(controller));
  layout->SetFlexForView(combo_view_, 1);

  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE) {
    if (Shell::Get()->system_tray_model()->update_model()->show_eol_notice()) {
      auto* eol_notice_wrapper = AddChildView(std::make_unique<views::View>());
      auto* eol_notice_layout = eol_notice_wrapper->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
              kUnifiedSystemInfoSpacing));
      eol_notice_layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      eol_notice_layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);

      eol_notice_ = eol_notice_wrapper->AddChildView(
          std::make_unique<EolNoticeQuickSettingsView>());
    }

    // If the release track is not "stable" then channel indicator UI for quick
    // settings is put up.
    auto channel = Shell::Get()->shell_delegate()->GetChannel();
    if (features::IsReleaseTrackUiEnabled() && !eol_notice_ &&
        channel_indicator_utils::IsDisplayableChannel(channel)) {
      channel_view_ =
          AddChildView(std::make_unique<ChannelIndicatorQuickSettingsView>(
              channel, Shell::Get()
                           ->system_tray_model()
                           ->client()
                           ->IsUserFeedbackEnabled()));
    }
  }
}

UnifiedSystemInfoView::~UnifiedSystemInfoView() = default;

void UnifiedSystemInfoView::ChildVisibilityChanged(views::View* child) {
  Layout();
}

void UnifiedSystemInfoView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

bool UnifiedSystemInfoView::IsSupervisedVisibleForTesting() {
  return combo_view_->IsSupervisedVisibleForTesting();  // IN-TEST
}

views::View* UnifiedSystemInfoView::GetDateViewForTesting() {
  return combo_view_->date_view_;
}

views::View* UnifiedSystemInfoView::GetDateViewLabelForTesting() {
  DCHECK(combo_view_->date_view_);
  return combo_view_->date_view_->label_;
}

void UnifiedSystemInfoView::UpdateDateViewForTesting() {
  DCHECK(combo_view_->date_view_);
  combo_view_->date_view_->Update();
}

BEGIN_METADATA(UnifiedSystemInfoView, views::View)
END_METADATA

}  // namespace ash
