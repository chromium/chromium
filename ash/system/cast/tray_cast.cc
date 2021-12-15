// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/tray_cast.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"

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

namespace tray {

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

const char* CastDetailedView::GetClassName() const {
  return "CastDetailedView";
}

void CastDetailedView::UpdateReceiverListFromCachedData() {
  // Remove all of the existing views.
  view_to_sink_map_.clear();
  scroll_content()->RemoveAllChildViews();

  // Add a view for each receiver.
  for (auto& it : sinks_and_routes_) {
    const CastSink& sink = it.second.sink;
    views::View* container = AddScrollListItem(
        SinkIconTypeToIcon(sink.sink_icon_type), base::UTF8ToUTF16(sink.name));
    view_to_sink_map_[container] = sink.id;
  }

  if (CastConfigController::Get()->AccessCodeCastingEnabled()) {
    EnterpriseDomainModel* enterprise_domain =
        Shell::Get()->system_tray_model()->enterprise_domain();
    const std::string& org_name = enterprise_domain->account_domain_manager();
    DCHECK(!org_name.empty())
        << "account_domain_manager should not be empty when user is managed!";
    add_access_code_device_ = AddScrollListItem(
        // TODO(b/209720161): replace with plus button icon when UI is final.
        SinkIconTypeToIcon(SinkIconType::kGeneric),
        l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_CAST_ACCESS_CAST_ADD,
                                   base::UTF8ToUTF16(org_name)));
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

void CastDetailedView::HandleViewClicked(views::View* view) {
  // Find the receiver we are going to cast to.
  auto it = view_to_sink_map_.find(view);
  if (it != view_to_sink_map_.end()) {
    CastConfigController::Get()->CastToSink(it->second);
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_DETAILED_CAST_VIEW_LAUNCH_CAST);
  } else if (view == add_access_code_device_) {
    base::RecordAction(base::UserMetricsAction(
        "StatusArea_Cast_Detailed_Launch_AccesCastDialog"));
    Shell::Get()->system_tray_model()->client()->ShowAccessCodeCastingDialog();
  }
}

}  // namespace tray
}  // namespace ash
