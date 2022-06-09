// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_info_view.h"

#include "ash/constants/ash_features.h"
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
#include "ash/system/power/adaptive_charging_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/system/supervised/supervised_icon_string.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
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

// Helper function for getting ContentLayerColor.
inline SkColor GetContentLayerColor(ContentLayerType type) {
  return ash::AshColorProvider::Get()->GetContentLayerColor(type);
}

// Helper function for configuring label in BatteryInfoView.
void ConfigureLabel(views::Label* label, SkColor color) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetEnabledColor(color);
  label->GetViewAccessibility().OverrideIsIgnored(true);
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
  SetLayoutManager(std::make_unique<views::FillLayout>());
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  Update();

  Shell::Get()->system_tray_model()->clock()->AddObserver(this);
  SetEnabled(Shell::Get()->system_tray_model()->clock()->IsSettingsAvailable());
  SetInstallFocusRingOnFocus(true);
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
  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

void DateView::OnButtonPressed(const ui::Event& event) {
  if (features::IsCalendarViewEnabled() && controller_->IsExpanded()) {
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
  if (features::IsCalendarViewEnabled()) {
    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_CALENDAR_ENTRY_ACCESSIBLE_DESCRIPTION,
        TimeFormatFriendlyDateAndTime(now)));
  } else {
    SetAccessibleName(TimeFormatFriendlyDateAndTime(now));
  }
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

// A base class for both BatteryLabelView and BatteryIconView. It updates by
// observing PowerStatus.
class BatteryInfoViewBase : public views::Button, public PowerStatus::Observer {
 public:
  METADATA_HEADER(BatteryInfoViewBase);
  explicit BatteryInfoViewBase(UnifiedSystemTrayController* controller)
      : Button(base::BindRepeating(&BatteryInfoViewBase::OnButtonPressed,
                                   base::Unretained(this))),
        controller_(controller) {
    power_status_observation_.Observe(PowerStatus::Get());
  }

  BatteryInfoViewBase(const BatteryInfoViewBase&) = delete;
  BatteryInfoViewBase& operator=(const BatteryInfoViewBase&) = delete;

  ~BatteryInfoViewBase() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kLabelText;
    node_data->SetName(PowerStatus::Get()->GetAccessibleNameString(true));
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override { Update(); }

  // Should be override by subclass.
  virtual void Update() = 0;

 private:
  // Callback called when this is pressed.
  void OnButtonPressed(const ui::Event& event) {
    controller_->HandleOpenPowerSettingsAction();
  }

  // Unowned.
  ash::UnifiedSystemTrayController* const controller_;

  base::ScopedObservation<PowerStatus, PowerStatus::Observer>
      power_status_observation_{this};
};
BEGIN_METADATA(BatteryInfoViewBase, views::Button)
END_METADATA

// A view that shows battery status.
class BatteryLabelView : public BatteryInfoViewBase {
 public:
  METADATA_HEADER(BatteryLabelView);
  BatteryLabelView(UnifiedSystemTrayController* controller,
                   bool use_smart_charging_ui)
      : BatteryInfoViewBase(controller),
        use_smart_charging_ui_(use_smart_charging_ui) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    percentage_ = AddChildView(std::make_unique<views::Label>());
    auto seperator = std::make_unique<views::Label>();
    seperator->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_STATUS_SEPARATOR));
    separator_ = AddChildView(std::move(seperator));
    status_ = AddChildView(std::make_unique<views::Label>());
    Update();
  }

  BatteryLabelView(const BatteryLabelView&) = delete;
  BatteryLabelView& operator=(const BatteryLabelView&) = delete;
  ~BatteryLabelView() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const auto color =
        GetContentLayerColor(ContentLayerType::kTextColorSecondary);
    ConfigureLabel(percentage_, color);
    ConfigureLabel(separator_, color);
    ConfigureLabel(status_, color);
  }

 private:
  void Update() override {
    std::u16string percentage_text;
    std::u16string status_text;
    std::tie(percentage_text, status_text) =
        PowerStatus::Get()->GetStatusStrings();

    percentage_->SetText(percentage_text);
    status_->SetText(status_text);

    percentage_->SetVisible(!percentage_text.empty() &&
                            !use_smart_charging_ui_);
    separator_->SetVisible(!percentage_text.empty() &&
                           !use_smart_charging_ui_ && !status_text.empty());
    status_->SetVisible(!status_text.empty());

    percentage_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
    status_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  }

  views::Label* percentage_ = nullptr;
  views::Label* separator_ = nullptr;
  views::Label* status_ = nullptr;

  const bool use_smart_charging_ui_;
};
BEGIN_METADATA(BatteryLabelView, BatteryInfoViewBase)
END_METADATA

// A view that shows battery icon and charging state when smart charging is
// enabled.
class BatteryIconView : public BatteryInfoViewBase {
 public:
  METADATA_HEADER(BatteryIconView);
  explicit BatteryIconView(UnifiedSystemTrayController* controller)
      : BatteryInfoViewBase(controller) {
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal);
    layout->set_inside_border_insets(kUnifiedSystemInfoBatteryIconPadding);
    SetLayoutManager(std::move(layout));

    battery_image_ = AddChildView(std::make_unique<views::ImageView>());
    if (features::IsDarkLightModeEnabled()) {
      // The battery icon requires its own layer to properly render the masked
      // outline of the badge within the battery icon.
      battery_image_->SetPaintToLayer();
      battery_image_->layer()->SetFillsBoundsOpaquely(false);
    }
    ConfigureIcon();

    percentage_ = AddChildView(std::make_unique<views::Label>());

    SetBackground(views::CreateRoundedRectBackground(
        GetContentLayerColor(AshColorProvider::ContentLayerType::
                                 kBatterySystemInfoBackgroundColor),
        GetPreferredSize().height() / 2));

    Update();
  }

  BatteryIconView(const BatteryIconView&) = delete;
  BatteryIconView& operator=(const BatteryIconView&) = delete;
  ~BatteryIconView() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const auto color =
        GetContentLayerColor(ContentLayerType::kButtonLabelColorPrimary);
    ConfigureLabel(percentage_, color);
    ConfigureIcon();
  }

 private:
  void Update() override {
    const std::u16string percentage_text =
        PowerStatus::Get()->GetStatusStrings().first;

    percentage_->SetText(percentage_text);
    percentage_->SetVisible(!percentage_text.empty());
    percentage_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);

    ConfigureIcon();
  }

  void ConfigureIcon() {
    const SkColor battery_icon_color = GetContentLayerColor(
        AshColorProvider::ContentLayerType::kBatterySystemInfoIconColor);

    const SkColor badge_color = GetContentLayerColor(
        AshColorProvider::ContentLayerType::kBatterySystemInfoBackgroundColor);

    PowerStatus::BatteryImageInfo info =
        PowerStatus::Get()->GetBatteryImageInfo();
    info.alert_if_low = false;
    battery_image_->SetImage(PowerStatus::GetBatteryImage(
        info, kUnifiedTrayBatteryIconSize, battery_icon_color,
        battery_icon_color, badge_color));
  }

  views::Label* percentage_ = nullptr;
  views::ImageView* battery_image_ = nullptr;
};
BEGIN_METADATA(BatteryIconView, BatteryInfoViewBase)
END_METADATA

// A base class of the views showing device management state.
class ManagedStateView : public views::Button {
 public:
  ManagedStateView(const ManagedStateView&) = delete;
  ManagedStateView& operator=(const ManagedStateView&) = delete;

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
};

void ManagedStateView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  label_->SetEnabledColor(color_provider->GetContentLayerColor(
      ContentLayerType::kTextColorSecondary));
  image_->SetImage(
      gfx::CreateVectorIcon(icon_, color_provider->GetContentLayerColor(
                                       ContentLayerType::kIconColorSecondary)));
  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
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
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
}

// A view that shows whether the device is enterprise managed or not. It updates
// by observing EnterpriseDomainModel.
class EnterpriseManagedView : public ManagedStateView,
                              public EnterpriseDomainObserver,
                              public SessionObserver {
 public:
  explicit EnterpriseManagedView(UnifiedSystemTrayController* controller);

  EnterpriseManagedView(const EnterpriseManagedView&) = delete;
  EnterpriseManagedView& operator=(const EnterpriseManagedView&) = delete;

  ~EnterpriseManagedView() override;

  // EnterpriseDomainObserver:
  void OnDeviceEnterpriseInfoChanged() override;
  void OnEnterpriseAccountDomainChanged() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override;

  // views::Button:
  const char* GetClassName() const override { return "EnterpriseManagedView"; }

 private:
  void Update();
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
  std::string account_domain_manager =
      features::IsManagedDeviceUIRedesignEnabled()
          ? model->account_domain_manager()
          : std::string();

  bool visible = session_controller->ShouldDisplayManagedUI() ||
                 model->active_directory_managed() ||
                 !enterprise_domain_manager.empty() ||
                 !account_domain_manager.empty();
  SetVisible(visible);

  if (!visible)
    return;

  if (!features::IsManagedDeviceUIRedesignEnabled()) {
    if (model->active_directory_managed()) {
      SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED, ui::GetChromeOSDeviceName()));
    } else if (!model->enterprise_domain_manager().empty()) {
      SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(model->enterprise_domain_manager())));
    }
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
  }
  SetTooltipText(managed_string);
}

// A view that shows whether the user is supervised or a child.
class SupervisedUserView : public ManagedStateView {
 public:
  SupervisedUserView();

  SupervisedUserView(const SupervisedUserView&) = delete;
  SupervisedUserView& operator=(const SupervisedUserView&) = delete;

  ~SupervisedUserView() override = default;

  // views::Button:
  const char* GetClassName() const override { return "SupervisedUserView"; }
};

SupervisedUserView::SupervisedUserView()
    : ManagedStateView(PressedCallback(),
                       IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL,
                       GetSupervisedUserIcon()) {
  bool visible = Shell::Get()->session_controller()->IsUserChild();
  SetVisible(visible);
  if (visible)
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
    separator_->SetPreferredLength(kUnifiedSystemInfoHeight);

    const bool use_smart_charging_ui = UseSmartChargingUI();
    if (use_smart_charging_ui)
      AddChildView(std::make_unique<BatteryIconView>(controller));
    AddChildView(
        std::make_unique<BatteryLabelView>(controller, use_smart_charging_ui));
  }

  auto* spacing = AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(spacing, 1);

  enterprise_managed_ =
      AddChildView(std::make_unique<EnterpriseManagedView>(controller));
  supervised_ = AddChildView(std::make_unique<SupervisedUserView>());
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
    separator_->SetColor(
        GetContentLayerColor(ContentLayerType::kSeparatorColor));
  }
}

}  // namespace ash
