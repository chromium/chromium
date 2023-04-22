// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_VIEWS_BUBBLE_VIEW_H_
#define ASH_SHORTCUT_VIEWER_VIEWS_BUBBLE_VIEW_H_

#include <string>
#include <vector>

#include "ash/public/cpp/style/color_provider.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace gfx {
class ShadowValue;
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace keyboard_shortcut_viewer {

// Displays a keyboard shortcut component (such as a modifier or a key) as a
// rounded-corner bubble which can contain either text, icon, or both.
class BubbleView : public views::View {
 public:
  BubbleView();

  BubbleView(const BubbleView&) = delete;
  BubbleView& operator=(const BubbleView&) = delete;

  ~BubbleView() override;

  void SetIcon(const gfx::VectorIcon& icon);

  void SetText(const std::u16string& text);

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  raw_ptr<views::ImageView, ExperimentalAsh> icon_ = nullptr;

  raw_ptr<views::Label, ExperimentalAsh> text_ = nullptr;

  std::vector<gfx::ShadowValue> shadows_;

  raw_ptr<ash::ColorProvider, ExperimentalAsh> color_provider_;  // Not owned.
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_VIEWS_BUBBLE_VIEW_H_
