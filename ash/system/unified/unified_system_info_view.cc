// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_info_view.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/power/power_status.h"
#include "ash/system/supervised/supervised_icon_string.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
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
class DateView : public views::Button,
                 public views::ButtonListener,
                 public ClockObserver {
 public:
  explicit DateView(UnifiedSystemTrayController* controller);
  ~DateView() override;

 private:
  void Update();

  // views::Button:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

  UnifiedSystemTrayController* const controller_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DateView);
};

DateView::DateView(UnifiedSystemTrayController* controller)
    : Button(this), controller_(controller), label_(new views::Label) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(label_);

  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  label_->SetEnabledColor(kUnifiedMenuTextColor);
  Update();

  Shell::Get()->system_tray_model()->clock()->AddObserver(this);
  TrayPopupUtils::ConfigureTrayPopupButton(this);

  SetEnabled(Shell::Get()->system_tray_model()->clock()->IsSettingsAvailable());
}

DateView::~DateView() {
  Shell::Get()->system_tray_model()->clock()->RemoveObserver(this);
}

void DateView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  controller_->HandleOpenDateTimeSettingsAction();
}

void DateView::Update() {
  base::Time now = base::Time::Now();
  label_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DATE, FormatDayOfWeek(now), FormatDate(now)));
  SetAccessibleName(TimeFormatFriendlyDateAndTime(now));
  label_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

std::unique_ptr<views::InkDrop> DateView::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> DateView::CreateInkDropRipple() const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight> DateView::CreateInkDropHighlight()
    const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this);
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
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

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

BatteryView::BatteryView()
    : percentage_(new views::Label),
      separator_(new views::Label),
      status_(new views::Label) {
  PowerStatus::Get()->AddObserver(this);

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));

  separator_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_STATUS_SEPARATOR));

  ConfigureLabel(percentage_);
  ConfigureLabel(separator_);
  ConfigureLabel(status_);

  AddChildView(percentage_);
  AddChildView(separator_);
  AddChildView(status_);

  Update();
}

BatteryView::~BatteryView() {
  PowerStatus::Get()->RemoveObserver(this);
}

void BatteryView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void BatteryView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
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
  label->SetEnabledColor(kUnifiedMenuSecondaryTextColor);
}

// A base class of the views showing device management state.
class ManagedStateView : public views::Button {
 public:
  ~ManagedStateView() override = default;

  // views::Button:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

 protected:
  ManagedStateView(views::ButtonListener* listener,
                   int label_id,
                   const gfx::VectorIcon& icon);

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedStateView);
};

ManagedStateView::ManagedStateView(views::ButtonListener* listener,
                                   int label_id,
                                   const gfx::VectorIcon& icon)
    : Button(listener) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(), kUnifiedSystemInfoSpacing));

  auto* label = new views::Label;
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetEnabledColor(kUnifiedMenuSecondaryTextColor);
  label->SetText(l10n_util::GetStringUTF16(label_id));
  AddChildView(label);

  auto* image = new views::ImageView;
  image->SetImage(gfx::CreateVectorIcon(icon, kUnifiedMenuSecondaryTextColor));
  image->SetPreferredSize(
      gfx::Size(kUnifiedSystemInfoHeight, kUnifiedSystemInfoHeight));
  AddChildView(image);

  TrayPopupUtils::ConfigureTrayPopupButton(this);
}

std::unique_ptr<views::InkDrop> ManagedStateView::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> ManagedStateView::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
ManagedStateView::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this);
}

// A view that shows whether the device is enterprise managed or not. It updates
// by observing EnterpriseDomainModel.
class EnterpriseManagedView : public ManagedStateView,
                              public views::ButtonListener,
                              public EnterpriseDomainObserver {
 public:
  explicit EnterpriseManagedView(UnifiedSystemTrayController* controller);
  ~EnterpriseManagedView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // EnterpriseDomainObserver:
  void OnEnterpriseDomainChanged() override;

 private:
  void Update();

  UnifiedSystemTrayController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseManagedView);
};

EnterpriseManagedView::EnterpriseManagedView(
    UnifiedSystemTrayController* controller)
    : ManagedStateView(this,
                       IDS_ASH_ENTERPRISE_DEVICE_MANAGED_SHORT,
                       kUnifiedMenuManagedIcon),
      controller_(controller) {
  DCHECK(Shell::Get());
  set_id(VIEW_ID_TRAY_ENTERPRISE);
  Shell::Get()->system_tray_model()->enterprise_domain()->AddObserver(this);
  Update();
}

EnterpriseManagedView::~EnterpriseManagedView() {
  Shell::Get()->system_tray_model()->enterprise_domain()->RemoveObserver(this);
}

void EnterpriseManagedView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  controller_->HandleEnterpriseInfoAction();
}

void EnterpriseManagedView::OnEnterpriseDomainChanged() {
  Update();
}

void EnterpriseManagedView::Update() {
  EnterpriseDomainModel* model =
      Shell::Get()->system_tray_model()->enterprise_domain();
  SetVisible(model->active_directory_managed() ||
             !model->enterprise_display_domain().empty());

  if (model->active_directory_managed()) {
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED));
  } else if (!model->enterprise_display_domain().empty()) {
    SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY,
        base::UTF8ToUTF16(model->enterprise_display_domain())));
  }
}

// A view that shows whether the user is supervised or a child.
class SupervisedUserView : public ManagedStateView {
 public:
  SupervisedUserView();
  ~SupervisedUserView() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserView);
};

SupervisedUserView::SupervisedUserView()
    : ManagedStateView(nullptr,
                       IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL,
                       GetSupervisedUserIcon()) {
  SetVisible(Shell::Get()->session_controller()->IsUserSupervised());
  if (Shell::Get()->session_controller()->IsUserSupervised())
    SetTooltipText(GetSupervisedUserMessage());
}

}  // namespace

UnifiedSystemInfoView::UnifiedSystemInfoView(
    UnifiedSystemTrayController* controller)
    : enterprise_managed_(new EnterpriseManagedView(controller)),
      supervised_(new SupervisedUserView()) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedSystemInfoSpacing));

  AddChildView(new DateView(controller));

  if (PowerStatus::Get()->IsBatteryPresent()) {
    auto* separator = new views::Separator();
    separator->SetColor(kUnifiedMenuSecondaryTextColor);
    separator->SetPreferredHeight(kUnifiedSystemInfoHeight);
    AddChildView(separator);

    AddChildView(new BatteryView());
  }

  auto* spacing = new views::View;
  AddChildView(spacing);
  layout->SetFlexForView(spacing, 1);

  AddChildView(enterprise_managed_);
  AddChildView(supervised_);
}

UnifiedSystemInfoView::~UnifiedSystemInfoView() = default;

void UnifiedSystemInfoView::ChildVisibilityChanged(views::View* child) {
  Layout();
}

void UnifiedSystemInfoView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

}  // namespace ash
