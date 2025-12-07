// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_BUBBLE_VIEW_H_
#define ASH_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
namespace ash {

inline constexpr int kBubbleBorderRadius = 26;
inline constexpr int kBubbleVerticalPadding = 10;
inline constexpr int kBubbleHorizontalPadding = 12;
inline constexpr int kBubbleElementSpace = 8;
inline constexpr int kIconDip = 20;

class ASH_EXPORT SpotlightNotificationBubbleView : public views::BoxLayoutView {
  METADATA_HEADER(SpotlightNotificationBubbleView, views::BoxLayoutView)
 public:
  explicit SpotlightNotificationBubbleView(const std::string& teacher_name);
  SpotlightNotificationBubbleView(const SpotlightNotificationBubbleView&) =
      delete;
  SpotlightNotificationBubbleView& operator=(
      const SpotlightNotificationBubbleView) = delete;
  ~SpotlightNotificationBubbleView() override;

  // Test element accessors:
  views::ImageView* get_visibility_icon() { return visibility_icon_; }
  views::Label* get_notification_label() { return notification_label_; }

  // Shows and announces the bubble with the name of teacher who owns the
  // active Spotlight session.
  void ShowInactive();

 private:
  void Init(const std::string& teacher_name);

  raw_ptr<views::ImageView> visibility_icon_;
  raw_ptr<views::Label> notification_label_;
};
}  // namespace ash

#endif  // ASH_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_BUBBLE_VIEW_H_
