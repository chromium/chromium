// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_STYLUS_BATTERY_VIEW_H_
#define ASH_SYSTEM_PALETTE_STYLUS_BATTERY_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/palette/stylus_battery_delegate.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class ASH_EXPORT StylusBatteryView : public views::View {
  METADATA_HEADER(StylusBatteryView, views::View)

 public:
  StylusBatteryView();
  StylusBatteryView(const StylusBatteryView&) = delete;
  StylusBatteryView& operator=(const StylusBatteryView&) = delete;
  ~StylusBatteryView() override = default;

  // views::View:
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void OnBatteryLevelUpdated();

 private:
  StylusBatteryDelegate stylus_battery_delegate_;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_STYLUS_BATTERY_VIEW_H_
