// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_
#define ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_

#include "ash/system/tray/tray_item_view.h"
#include "components/version_info/channel.h"

namespace ash {

// A view that resides in the system tray, to make it obvious to the user when a
// device is running on a release track other than "stable."
class ChannelIndicatorView : public TrayItemView {
 public:
  ChannelIndicatorView(Shelf* shelf, version_info::Channel channel);
  ChannelIndicatorView(const ChannelIndicatorView&) = delete;
  ChannelIndicatorView& operator=(const ChannelIndicatorView&) = delete;

  ~ChannelIndicatorView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // TrayItemView:
  void HandleLocaleChange() override;

 private:
  // Functions called downstream from Update(), that make no assumptions about
  // the value of the `channel_` member variable.
  void Update(version_info::Channel channel);
  void SetImage(version_info::Channel channel);
  void SetAccessibleName(version_info::Channel channel);
  void SetTooltip(version_info::Channel channel);

  std::u16string accessible_name_;
  std::u16string tooltip_;
  version_info::Channel channel_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_
