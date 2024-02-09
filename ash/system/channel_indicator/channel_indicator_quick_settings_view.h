// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_QUICK_SETTINGS_VIEW_H_
#define ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_QUICK_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "components/version_info/channel.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// ChannelIndicatorQuickSettingsView contains all of the views included in the
// channel indicator UI that resides in `QuickSettingsHeader`.
class ASH_EXPORT ChannelIndicatorQuickSettingsView : public views::View {
  METADATA_HEADER(ChannelIndicatorQuickSettingsView, views::View)

 public:
  ChannelIndicatorQuickSettingsView(version_info::Channel channel,
                                    bool allow_user_feedback);
  ChannelIndicatorQuickSettingsView(const ChannelIndicatorQuickSettingsView&) =
      delete;
  ChannelIndicatorQuickSettingsView& operator=(
      const ChannelIndicatorQuickSettingsView&) = delete;
  ~ChannelIndicatorQuickSettingsView() override = default;

  // Sets a special "narrow" layout. If `narrow` is true, centers the version
  // string in the left button. If `narrow` is false, centers the version
  // string with respect to the combined width of the version and feedback
  // buttons.
  void SetNarrowLayout(bool narrow);

  views::View* version_button_for_test() { return version_button_; }
  views::View* feedback_button_for_test() { return feedback_button_; }

 private:
  // Refs maintained for unit test introspection methods.
  raw_ptr<views::View> version_button_ = nullptr;
  raw_ptr<views::View> feedback_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_QUICK_SETTINGS_VIEW_H_
