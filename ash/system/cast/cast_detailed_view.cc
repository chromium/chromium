// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_detailed_view.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/system/cast/cast_zero_state_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Extra spacing to add between cast stop buttons and the edge of the qs tray.
constexpr int kStopButtonExtraMargin = 4;

// Returns the correct vector icon for |icon_type|. Some types may be different
// for branded builds.
const gfx::VectorIcon& SinkIconTypeToIcon(SinkIconType icon_type) {
  switch (icon_type) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case SinkIconType::kCast:
      return kSystemMenuCastDeviceIcon;
#else
    case SinkIconType::kCast:
      return kSystemMenuCastGenericIcon;
#endif
    case SinkIconType::kGeneric:
      return kSystemMenuCastGenericIcon;
    case SinkIconType::kCastAudioGroup:
      return kSystemMenuCastAudioGroupIcon;
    case SinkIconType::kCastAudio:
      return kSystemMenuCastAudioIcon;
    case SinkIconType::kWiredDisplay:
      return kSystemMenuCastGenericIcon;
  }

  NOTREACHED();
}

std::unique_ptr<views::View> MakeButtonContainer() {
  std::unique_ptr<views::View> button_container =
      std::make_unique<views::View>();
  views::BoxLayout* manager =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  manager->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  manager->set_between_child_spacing(kTrayPopupLabelRightPadding);
  button_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0, 0,
          kStopButtonExtraMargin + kWideMenuExtraMarginsFromRightEdge));
  return button_container;
}

}  // namespace

CastDetailedView::CastDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();
  OnDevicesUpdated(CastConfigController::Get()->GetSinksAndRoutes());
  CastConfigController::Get()->AddObserver(this);
}

CastDetailedView::~CastDetailedView() {
  CastConfigController::Get()->RemoveObserver(this);
}

void CastDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_CAST);
}

void CastDetailedView::OnDevicesUpdated(
    const std::vector<SinkAndRoute>& sinks_routes) {
  sinks_and_routes_.clear();
  for (const auto& sink_and_route : sinks_routes) {
    sinks_and_routes_.push_back(sink_and_route);
  }
  // Update UI.
  UpdateReceiverListFromCachedData();
  DeprecatedLayoutImmediately();
}

void CastDetailedView::UpdateReceiverListFromCachedData() {
  RemoveAllViews();

  views::View* item_container =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>());

  // Per product requirement, access code receiver should be shown before other
  // receivers.
  if (CastConfigController::Get()->AccessCodeCastingEnabled() &&
      (Shell::Get()->session_controller()->GetSessionState() !=
       session_manager::SessionState::LOCKED)) {
    AddAccessCodeCastButton(item_container);
  }

  // Add a view for each receiver.
  for (auto& it : sinks_and_routes_) {
    const CastSink& sink = it.sink;
    const CastRoute& route = it.route;
    HoverHighlightView* container = AddScrollListItem(
        item_container, SinkIconTypeToIcon(sink.sink_icon_type),
        base::UTF8ToUTF16(sink.name));
    view_to_sink_map_[container] = sink.id;

    // Add receiver action buttons if this machine ("local source") is casting
    // to the device. See also CastNotificationController::OnDevicesUpdated().
    if (!route.id.empty() && route.is_local_source) {
      AddReceiverActionButtons(sink, route, container, item_container);
    }
  }

  // If there are no receiver views, show the zero state view.
  if (!add_access_code_device_ && view_to_sink_map_.empty()) {
    AddZeroStateView();
    scroller()->SetVisible(false);
  } else {
    scroller()->SetVisible(true);
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->DeprecatedLayoutImmediately();
}

void CastDetailedView::AddZeroStateView() {
  DCHECK(!zero_state_view_);
  DCHECK(scroller());
  zero_state_view_ = AddChildViewAt(std::make_unique<CastZeroStateView>(),
                                    GetIndexOf(scroller()).value());
  // Make the view fill the entire space below the title row.
  box_layout()->SetFlexForView(zero_state_view_, 1);
}

void CastDetailedView::HandleViewClicked(views::View* view) {
  // Find the receiver we are going to cast to.
  auto it = view_to_sink_map_.find(view);
  if (it != view_to_sink_map_.end()) {
    // `CastToSink()` sometimes opens a screen picker, which causes a window
    // activation, which closes the quick settings bubble and deletes `this`.
    auto weak_this = weak_factory_.GetWeakPtr();
    CastConfigController::Get()->CastToSink(it->second);
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Cast_Detailed_Launch_Cast"));
    // Close the system tray to emphasize the pinned Cast notification.
    if (weak_this) {
      weak_this->CloseBubble();  // Deletes `this`.
    }
  } else if (view == add_access_code_device_) {
    base::RecordAction(base::UserMetricsAction(
        "StatusArea_Cast_Detailed_Launch_AccesCastDialog"));
    Shell::Get()->system_tray_model()->client()->ShowAccessCodeCastingDialog(
        AccessCodeCastDialogOpenLocation::kSystemTrayCastMenu);
    // NOTE: System tray is closed by focus change and `this` is deleted.
  }
}

void CastDetailedView::StopCasting(const std::string& route_id) {
  CastConfigController::Get()->StopCasting(route_id);
  CloseBubble();  // Deletes `this`.
}

void CastDetailedView::FreezePressed(const std::string& route_id,
                                     bool is_frozen) {
  if (is_frozen) {
    CastConfigController::Get()->UnfreezeRoute(route_id);
  } else {
    CastConfigController::Get()->FreezeRoute(route_id);
    CloseBubble();
  }
}

void CastDetailedView::RemoveAllViews() {
  view_to_sink_map_.clear();
  sink_extra_views_map_.clear();
  scroll_content()->RemoveAllChildViews();
  add_access_code_device_ = nullptr;
  if (zero_state_view_) {
    RemoveChildViewT(zero_state_view_.get());
    zero_state_view_ = nullptr;
  }
}

void CastDetailedView::AddAccessCodeCastButton(
    views::View* receiver_list_view) {
  add_access_code_device_ =
      AddScrollListItem(receiver_list_view, vector_icons::kKeyboardIcon,
                        l10n_util::GetStringUTF16(
                            IDS_ASH_STATUS_TRAY_CAST_ACCESS_CODE_CAST_CONNECT));
  // `views::ImageView` does not support changing the color, so set the
  // image with an updated `ui::ImageModel`.
  add_access_code_device_->icon()->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kKeyboardIcon, cros_tokens::kCrosSysPrimary));
  add_access_code_device_->text_label()->SetEnabledColorId(
      cros_tokens::kCrosSysPrimary);
}

void CastDetailedView::AddReceiverActionButtons(
    const CastSink& sink,
    const CastRoute& route,
    HoverHighlightView* receiver_view,
    views::View* receiver_list_view) {
  std::unique_ptr<PillButton> stop_button = CreateStopButton(route);

  // In the case that we want to show a pause/resume button, then we must
  // put both buttons on a row below the cast sink.
  if (route.freeze_info.can_freeze) {
    std::unique_ptr<PillButton> freeze_button = CreateFreezeButton(route);
    std::unique_ptr<views::View> button_container = MakeButtonContainer();
    std::vector<raw_ptr<views::View, VectorExperimental>> extra_views;
    extra_views.emplace_back(
        button_container->AddChildView(std::move(freeze_button)));
    extra_views.emplace_back(
        button_container->AddChildView(std::move(stop_button)));
    sink_extra_views_map_[sink.id] = extra_views;

    // Add the button container directly as a new row in the list of cast
    // devices. Since the associated device was just added, the buttons will
    // show up correctly below their associated device.
    receiver_list_view->AddChildView(std::move(button_container));
  } else {
    receiver_view->AddRightView(stop_button.release(),
                                views::CreateEmptyBorder(gfx::Insets::TLBR(
                                    0, 0, 0, kStopButtonExtraMargin)));
  }
}

std::unique_ptr<PillButton> CastDetailedView::CreateStopButton(
    const CastRoute& route) {
  std::unique_ptr<PillButton> stop_button = std::make_unique<PillButton>(
      base::BindRepeating(&CastDetailedView::StopCasting,
                          base::Unretained(this), route.id),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_STOP_CASTING),
      PillButton::kDefaultWithIconLeading, &kQuickSettingsCircleStopIcon);
  stop_button->SetBackgroundColorId(cros_tokens::kCrosSysErrorContainer);
  stop_button->SetIconColorId(cros_tokens::kCrosSysError);
  stop_button->SetButtonTextColorId(cros_tokens::kCrosSysError);
  return stop_button;
}

std::unique_ptr<PillButton> CastDetailedView::CreateFreezeButton(
    const CastRoute& route) {
  std::unique_ptr<PillButton> freeze_button = std::make_unique<PillButton>(
      base::BindRepeating(&CastDetailedView::FreezePressed,
                          base::Unretained(this), route.id,
                          route.freeze_info.is_frozen),
      route.freeze_info.is_frozen
          ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_RESUME_CASTING)
          : l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_PAUSE_CASTING),
      PillButton::kSecondaryWithIconLeading,
      route.freeze_info.is_frozen ? &kQuickSettingsCirclePlayIcon
                                  : &kQuickSettingsCirclePauseIcon);
  return freeze_button;
}

BEGIN_METADATA(CastDetailedView)
END_METADATA

}  // namespace ash
