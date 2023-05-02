// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_DETAILED_VIEW_H_
#define ASH_SYSTEM_CAST_CAST_DETAILED_VIEW_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class HoverHighlightView;

// This view displays a list of cast receivers that can be clicked on and casted
// to. It is activated by clicking on the chevron inside of
// |CastSelectDefaultView|.
class ASH_EXPORT CastDetailedView : public TrayDetailedView,
                                    public CastConfigController::Observer {
 public:
  METADATA_HEADER(CastDetailedView);

  explicit CastDetailedView(DetailedViewDelegate* delegate);

  CastDetailedView(const CastDetailedView&) = delete;
  CastDetailedView& operator=(const CastDetailedView&) = delete;

  ~CastDetailedView() override;

  // CastConfigController::Observer:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

  HoverHighlightView* get_add_access_code_device_for_testing() {
    return add_access_code_device_;
  }

 private:
  friend class CastDetailedViewTest;

  void CreateItems();

  void UpdateReceiverListFromCachedData();

  // Adds the view shown when no cast devices are available (with QsRevamp).
  void AddZeroStateView();

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // Stops casting the route identified by `route_id`.
  void StopCasting(const std::string& route_id);

  // A mapping from the sink id to the receiver/activity data.
  std::map<std::string, SinkAndRoute> sinks_and_routes_;

  // A mapping from the view pointer to the associated activity sink id.
  std::map<views::View*, std::string> view_to_sink_map_;

  // Special list item that, if clicked, launches the access code casting dialog
  raw_ptr<HoverHighlightView, ExperimentalAsh> add_access_code_device_ =
      nullptr;

  // View shown when no cast devices are available (with QsRevamp).
  raw_ptr<views::View, ExperimentalAsh> zero_state_view_ = nullptr;

  base::WeakPtrFactory<CastDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_DETAILED_VIEW_H_
