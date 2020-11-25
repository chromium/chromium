// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_info_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/clock_observer.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/power/power_status.h"
#include "ash/system/supervised/supervised_icon_string.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

namespace {

base::string16 FormatDate(const base::Time& time) {
  // Use 'short' month format (e.g., "Oct") followed by non-padded day of
  // month (e.g., "2", "10").
  return base::TimeFormatWithPattern(time, "LLLd");
}

base::string16 FormatDayOfWeek(const base::Time& time) {
  // Use 'short' day of week format (e.g., "Wed").
  return base::TimeFormatWithPattern(time, "EEE");
}

// A view that shows current date in short format e.g. "Mon, Mar 12". It updates
// by observing ClockObserver.
class DateView : public views::Button, public ClockObserver {
 public:
  explicit DateView(UnifiedSystemTrayController* controller);
  ~DateView() override;

  // views::Button:
  const char* GetClassName() const override { return "DateView"; }
  void OnThemeChanged() override;

 private:
  void Update();

  // views::Button:
  gfx::Insets GetInsets() const override;

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DateView);
};

DateView::DateView(UnifiedSystemTrayController* controller)
    : Button(base::BindRepeating(
          &UnifiedSystemTrayController::HandleOpenDateTimeSettingsAction,
          base::Unretained(controller))) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  Update();

  Shell::Get()->system_tray_model()->clock()->AddObserver(this);
  SetEnabled(Shell::Get()->system_tray_model()->clock()->IsSettingsAvailable());
  SetInstallFocusRingOnFocus(true);
  SetInkDropMode(views::InkDropHostView::InkDropMode::OFF);
}

DateView::~DateView() {
  Shell::Get()->system_tray_model()->clock()->RemoveObserver(this);
}

void DateView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  label_->SetEnabledColor(color_provider->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary));
  focus_ring()->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

void DateView::Update() {
  base::Time now = base::Time::Now();
  label_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DATE, FormatDayOfWeek(now), FormatDate(now)));
  SetAccessibleName(TimeFormatFriendlyDateAndTime(now));
  label_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
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

// A view that shows battery status. It updates by observing PowerStatus.
class BatteryView : public views::View, public PowerStatus::Observer {
 public:
  BatteryView();
  ~BatteryView() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  const char* GetClassName() const override { return "BatteryView"; }
  void OnThemeChanged() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

 private:
  void Update();

  void ConfigureLabel(views::Label* label);

  views::Label* percentage_;
  views::Label* separator_;
  views::Label* status_;

  DISALLOW_COPY_AND_ASSIGN(BatteryView);
};

BatteryView::BatteryView() {
  PowerStatus::Get()->AddObserver(this);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  percentage_ = AddChildView(std::make_unique<views::Label>());
  separator_ = AddChildView(std::make_unique<views::Label>());
  status_ = AddChildView(std::make_unique<views::Label>());
  separator_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_STATUS_SEPARATOR));
  Update();
}

BatteryView::~BatteryView() {
  PowerStatus::Get()->RemoveObserver(this);
}

void BatteryView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kLabelText;
  node_data->SetName(PowerStatus::Get()->GetAccessibleNameString(true));
}

void BatteryView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void BatteryView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void BatteryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  ConfigureLabel(percentage_);
  ConfigureLabel(separator_);
  ConfigureLabel(status_);
}

void BatteryView::OnPowerStatusChanged() {
  Update();
}

void BatteryView::Update() {
  base::string16 percentage_text;
  base::string16 status_text;
  std::tie(percentage_text, status_text) =
      PowerStatus::Get()->GetStatusStrings();

  percentage_->SetText(percentage_text);
  status_->SetText(status_text);

  percentage_->SetVisible(!percentage_text.empty());
  separator_->SetVisible(!percentage_text.empty() && !status_text.empty());
  status_->SetVisible(!status_text.empty());

  percentage_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  status_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

void BatteryView::ConfigureLabel(views::Label* label) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorSecondary));
  label->GetViewAccessibility().OverrideIsIgnored(true);
}

// A base class of the views showing device management state.
class ManagedStateView : public views::Button {
 public:
  ~ManagedStateView() override = default;

  // views::Button:
  const char* GetClassName() const override { return "ManagedStateView"; }
  void OnThemeChanged() override;

 protected:
  ManagedStateView(PressedCallback callback,
                   int label_id,
                   const gfx::VectorIcon& icon);

 private:
  views::Label* label_ = nullptr;
  views::ImageView* image_ = nullptr;
  const gfx::VectorIcon& icon_;

  DISALLOW_COPY_AND_ASSIGN(ManagedStateView);
};

void ManagedStateView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  label_->SetEnabledColor(color_provider->GetContentLayerColor(
      ContentLayerType::kTextColorSecondary));
  image_->SetImage(
      gfx::CreateVectorIcon(icon_, color_provider->GetContentLayerColor(
                                       ContentLayerType::kIconColorSecondary)));
  focus_ring()->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

ManagedStateView::ManagedStateView(PressedCallback callback,
                                   int label_id,
                                   const gfx::VectorIcon& icon)
    : Button(std::move(callback)), icon_(icon) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kUnifiedSystemInfoSpacing));

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  label_->SetText(l10n_util::GetStringUTF16(label_id));

  image_ = AddChildView(std::make_unique<views::ImageView>());
  image_->SetPreferredSize(
      gfx::Size(kUnifiedSystemInfoHeight, kUnifiedSystemInfoHeight));

  SetInstallFocusRingOnFocus(true);
  SetInkDropMode(views::InkDropHostView::InkDropMode::OFF);
}

// A view that shows whether the device is enterprise managed or not. It updates
// by observing EnterpriseDomainModel.
class EnterpriseManagedView : public ManagedStateView,
                              public EnterpriseDomainObserver,
                              public SessionObserver {
 public:
  explicit EnterpriseManagedView(UnifiedSystemTrayController* controller);
  ~EnterpriseManagedView() override;

  // EnterpriseDomainObserver:
  void OnEnterpriseDomainChanged() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override;

  // views::Button:
  const char* GetClassName() const override { return "EnterpriseManagedView"; }

 private:
  void Update();

  DISALLOW_COPY_AND_ASSIGN(EnterpriseManagedView);
};

EnterpriseManagedView::EnterpriseManagedView(
    UnifiedSystemTrayController* controller)
    : ManagedStateView(
          base::BindRepeating(
              &UnifiedSystemTrayController::HandleEnterpriseInfoAction,
              base::Unretained(controller)),
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_SHORT,
          kUnifiedMenuManagedIcon) {
  DCHECK(Shell::Get());
  SetID(VIEW_ID_TRAY_ENTERPRISE);
  Shell::Get()->system_tray_model()->enterprise_domain()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  Update();
}

EnterpriseManagedView::~EnterpriseManagedView() {
  Shell::Get()->system_tray_model()->enterprise_domain()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void EnterpriseManagedView::OnEnterpriseDomainChanged() {
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
  SetVisible(session_controller->ShouldDisplayManagedUI() ||
             model->active_directory_managed() ||
             !model->enterprise_domain_manager().empty());

  if (model->active_directory_managed()) {
    SetTooltipText(l10n_util::GetStringFUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED,
                                              ui::GetChromeOSDeviceName()));
  } else if (!model->enterprise_domain_manager().empty()) {
    SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, ui::GetChromeOSDeviceName(),
        base::UTF8ToUTF16(model->enterprise_domain_manager())));
  }
}

// A view that shows whether the user is supervised or a child.
class SupervisedUserView : public ManagedStateView {
 public:
  SupervisedUserView();
  ~SupervisedUserView() override = default;

  // views::Button:
  const char* GetClassName() const override { return "SupervisedUserView"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserView);
};

SupervisedUserView::SupervisedUserView()
    : ManagedStateView(PressedCallback(),
                       IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL,
                       GetSupervisedUserIcon()) {
  SetVisible(Shell::Get()->session_controller()->IsUserSupervised());
  if (Shell::Get()->session_controller()->IsUserSupervised())
    SetTooltipText(GetSupervisedUserMessage());

  // TODO(crbug/1026821) Add SupervisedUserView::ButtonPress() overload
  // to show a similar ui to enterprise managed accounts. Disable button
  // state for now.
  SetState(ButtonState::STATE_DISABLED);
}

}  // namespace

UnifiedSystemInfoView::UnifiedSystemInfoView(
    UnifiedSystemTrayController* controller) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedSystemInfoViewPadding,
      kUnifiedSystemInfoSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(std::make_unique<DateView>(controller));

  if (PowerStatus::Get()->IsBatteryPresent()) {
    separator_ = AddChildView(std::make_unique<views::Separator>());
    separator_->SetPreferredHeight(kUnifiedSystemInfoHeight);
    AddChildView(std::make_unique<BatteryView>());
  }

  auto* spacing = AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(spacing, 1);

  if (!features::IsManagedDeviceUIRedesignEnabled()) {
    // UnifiedManagedDeviceView is shown instead.
    enterprise_managed_ =
        AddChildView(std::make_unique<EnterpriseManagedView>(controller));
    supervised_ = AddChildView(std::make_unique<SupervisedUserView>());
  }
}

UnifiedSystemInfoView::~UnifiedSystemInfoView() = default;

void UnifiedSystemInfoView::ChildVisibilityChanged(views::View* child) {
  Layout();
}

void UnifiedSystemInfoView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

const char* UnifiedSystemInfoView::GetClassName() const {
  return "UnifiedSystemInfoView";
}

void UnifiedSystemInfoView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (separator_) {
    separator_->SetColor(AshColorProvider::Get()->GetContentLayerColor(
        ContentLayerType::kSeparatorColor));
  }
}

}  // namespace ash
