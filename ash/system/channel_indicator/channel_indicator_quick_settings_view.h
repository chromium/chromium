// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_QUICK_SETTINGS_VIEW_H_
#define ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_QUICK_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "components/version_info/channel.h"
#include "ui/views/view.h"

namespace ash {

// ChannelIndicatorQuickSettingsView contains all of the views included in the
// channel indicator UI that resides in UnifiedSystemInfoView.
class ASH_EXPORT ChannelIndicatorQuickSettingsView : public views::View {
 public:
  ChannelIndicatorQuickSettingsView(version_info::Channel channel,
                                    bool allow_user_feedback);
  ChannelIndicatorQuickSettingsView(const ChannelIndicatorQuickSettingsView&) =
      delete;
  ChannelIndicatorQuickSettingsView& operator=(
      const ChannelIndicatorQuickSettingsView&) = delete;
  ~ChannelIndicatorQuickSettingsView() override = default;

  views::View* version_button_for_test() { return version_button_; }
  views::View* feedback_button_for_test() { return feedback_button_; }

 private:
  // Refs maintained for unit test introspection methods.
  views::View* version_button_ = nullptr;
  views::View* feedback_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_QUICK_SETTINGS_VIEW_H_
