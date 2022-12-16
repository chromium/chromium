// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/tray_cast.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/system/cast/cast_zero_state_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

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
  return kSystemMenuCastGenericIcon;
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
  // Add/update existing.
  for (const auto& device : sinks_routes)
    sinks_and_routes_.insert(std::make_pair(device.sink.id, device));

  // Remove non-existent sinks. Removing an element invalidates all existing
  // iterators.
  auto iter = sinks_and_routes_.begin();
  while (iter != sinks_and_routes_.end()) {
    bool has_receiver = false;
    for (auto& receiver : sinks_routes) {
      if (iter->first == receiver.sink.id)
        has_receiver = true;
    }

    if (has_receiver)
      ++iter;
    else
      iter = sinks_and_routes_.erase(iter);
  }

  // Update UI.
  UpdateReceiverListFromCachedData();
  Layout();
}

void CastDetailedView::UpdateReceiverListFromCachedData() {
  // Remove all of the existing views.
  view_to_sink_map_.clear();
  scroll_content()->RemoveAllChildViews();
  add_access_code_device_ = nullptr;
  if (zero_state_view_) {
    RemoveChildViewT(zero_state_view_);
    zero_state_view_ = nullptr;
  }

  // QsRevamp places items in a rounded container.
  views::View* item_container =
      features::IsQsRevampEnabled()
          ? scroll_content()->AddChildView(std::make_unique<RoundedContainer>())
          : scroll_content();

  // Per product requirement, access code receiver should be shown before other
  // receivers.
  if (CastConfigController::Get()->AccessCodeCastingEnabled()) {
    add_access_code_device_ = AddScrollListItem(
        item_container, vector_icons::kKeyboardIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_CAST_ACCESS_CODE_CAST_CONNECT));
  }

  // Add a view for each receiver.
  for (auto& it : sinks_and_routes_) {
    const CastSink& sink = it.second.sink;
    const CastRoute& route = it.second.route;
    HoverHighlightView* container = AddScrollListItem(
        item_container, SinkIconTypeToIcon(sink.sink_icon_type),
        base::UTF8ToUTF16(sink.name));
    view_to_sink_map_[container] = sink.id;

    // Add a stop casting button if this machine ("local source") is casting to
    // the device. See also CastNotificationController::OnDevicesUpdated().
    if (features::IsQsRevampEnabled() && !route.id.empty() &&
        route.is_local_source) {
      auto button = std::make_unique<PillButton>(
          base::BindRepeating(&CastDetailedView::StopCasting,
                              base::Unretained(this), route.id),
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_STOP_CASTING),
          PillButton::kPrimaryWithoutIcon);
      container->AddRightView(
          button.release(),
          views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, 8)));
    }
  }

  // If there are no receiver views, show the zero state view.
  if (features::IsQsRevampEnabled() && !add_access_code_device_ &&
      view_to_sink_map_.empty()) {
    AddZeroStateView();
    scroller()->SetVisible(false);
  } else {
    scroller()->SetVisible(true);
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
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
    CastConfigController::Get()->CastToSink(it->second);
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Cast_Detailed_Launch_Cast"));
    // Close the system tray to emphasize the pinned Cast notification.
    if (features::IsQsRevampEnabled()) {
      CloseBubble();  // Deletes `this`.
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
  DCHECK(features::IsQsRevampEnabled());
  CastConfigController::Get()->StopCasting(route_id);
  CloseBubble();  // Deletes `this`.
}

BEGIN_METADATA(CastDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash
