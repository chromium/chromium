// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/tray_update.h"

#include "ash/metrics/user_metrics_action.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Returns the color to use for the update icon when the update severity is
// |severity|. If |for_menu| is true, the icon color for the system menu is
// given, otherwise the icon color for the system tray is given.
SkColor IconColorForUpdateSeverity(mojom::UpdateSeverity severity,
                                   bool for_menu) {
  const SkColor default_color = for_menu ? kMenuIconColor : kTrayIconColor;
  switch (severity) {
    case mojom::UpdateSeverity::NONE:
    case mojom::UpdateSeverity::VERY_LOW:  // Not used on Chrome OS.
      return default_color;
    case mojom::UpdateSeverity::LOW:
      return for_menu ? gfx::kGoogleGreen700 : kTrayIconColor;
    case mojom::UpdateSeverity::ELEVATED:
      return for_menu ? gfx::kGoogleYellow700 : gfx::kGoogleYellow300;
    case mojom::UpdateSeverity::HIGH:
    case mojom::UpdateSeverity::CRITICAL:
      return for_menu ? gfx::kGoogleRed700 : gfx::kGoogleRed300;
  }
  NOTREACHED() << severity;
  return default_color;
}

}  // namespace

// The "restart to update" item in the system tray menu.
class TrayUpdate::UpdateView : public ActionableView {
 public:
  explicit UpdateView(TrayUpdate* owner)
      : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS),
        model_(owner->model_),
        update_label_(nullptr) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);
    views::ImageView* image = TrayPopupUtils::CreateMainImageView();
    image->SetImage(gfx::CreateVectorIcon(
        kSystemMenuUpdateIcon,
        IconColorForUpdateSeverity(model_->GetSeverity(), true)));
    tri_view->AddView(TriView::Container::START, image);

    base::string16 label_text;
    update_label_ = TrayPopupUtils::CreateDefaultLabel();
    update_label_->set_id(VIEW_ID_TRAY_UPDATE_MENU_LABEL);
    update_label_->SetMultiLine(true);
    if (model_->factory_reset_required()) {
      label_text = bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_RESTART_AND_POWERWASH_TO_UPDATE);
    } else if (model_->update_type() == mojom::UpdateType::FLASH) {
      label_text = bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE_FLASH);
    } else if (!model_->update_required() &&
               model_->update_over_cellular_available()) {
      label_text = bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_UPDATE_OVER_CELLULAR_AVAILABLE);
      if (!Shell::Get()->session_controller()->ShouldEnableSettings()) {
        // Disables the view if settings page is not enabled.
        tri_view->SetEnabled(false);
        update_label_->SetEnabled(false);
      }
    } else {
      label_text = bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE);
    }

    SetAccessibleName(label_text);
    update_label_->SetText(label_text);

    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    style.SetupLabel(update_label_);
    tri_view->AddView(TriView::Container::CENTER, update_label_);

    SetInkDropMode(InkDropMode::ON);
  }

  ~UpdateView() override = default;

  UpdateModel* const model_;
  views::Label* update_label_;

 private:
  // Overridden from ActionableView.
  bool PerformAction(const ui::Event& /* event */) override {
    DCHECK(model_->update_required() ||
           model_->update_over_cellular_available());
    if (model_->update_required()) {
      Shell::Get()
          ->system_tray_model()
          ->client_ptr()
          ->RequestRestartForUpdate();
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_STATUS_AREA_OS_UPDATE_DEFAULT_SELECTED);
    } else {
      // Shows the about chrome OS page and checks for update after the page is
      // loaded.
      Shell::Get()->system_tray_model()->client_ptr()->ShowAboutChromeOS();
    }
    CloseSystemBubble();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

TrayUpdate::TrayUpdate(SystemTray* system_tray)
    : TrayImageItem(system_tray,
                    kSystemTrayUpdateIcon,
                    SystemTrayItemUmaType::UMA_UPDATE),
      model_(Shell::Get()->system_tray_model()->update_model()) {
  model_->AddObserver(this);
}

TrayUpdate::~TrayUpdate() {
  model_->RemoveObserver(this);
}

bool TrayUpdate::GetInitialVisibility() {
  return ShouldShowUpdate();
}

views::View* TrayUpdate::CreateTrayView(LoginStatus status) {
  views::View* view = TrayImageItem::CreateTrayView(status);
  view->set_id(VIEW_ID_TRAY_UPDATE_ICON);
  return view;
}

views::View* TrayUpdate::CreateDefaultView(LoginStatus status) {
  if (ShouldShowUpdate()) {
    update_view_ = new UpdateView(this);
    return update_view_;
  }
  return nullptr;
}

void TrayUpdate::OnDefaultViewDestroyed() {
  update_view_ = nullptr;
}

views::Label* TrayUpdate::GetLabelForTesting() {
  return update_view_ ? update_view_->update_label_ : nullptr;
}

void TrayUpdate::OnUpdateAvailable() {
  // Show the icon in the tray.
  SetIconColor(IconColorForUpdateSeverity(model_->GetSeverity(), false));
  tray_view()->SetVisible(ShouldShowUpdate());
}

bool TrayUpdate::ShouldShowUpdate() const {
  return model_->update_required() || model_->update_over_cellular_available();
}

}  // namespace ash
