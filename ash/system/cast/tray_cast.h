// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_TRAY_CAST_H_
#define ASH_SYSTEM_CAST_TRAY_CAST_H_

#include <string>
#include <vector>

#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/macros.h"

namespace ash {
namespace tray {

// This view displays a list of cast receivers that can be clicked on and casted
// to. It is activated by clicking on the chevron inside of
// |CastSelectDefaultView|.
class CastDetailedView : public TrayDetailedView,
                         public CastConfigController::Observer {
 public:
  explicit CastDetailedView(DetailedViewDelegate* delegate);
  ~CastDetailedView() override;

  // CastConfigController::Observer:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  void CreateItems();

  void UpdateReceiverListFromCachedData();

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // A mapping from the sink id to the receiver/activity data.
  std::map<std::string, SinkAndRoute> sinks_and_routes_;
  // A mapping from the view pointer to the associated activity sink id.
  std::map<views::View*, std::string> view_to_sink_map_;

  DISALLOW_COPY_AND_ASSIGN(CastDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_CAST_TRAY_CAST_H_
