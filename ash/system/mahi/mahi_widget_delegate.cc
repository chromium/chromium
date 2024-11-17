// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_widget_delegate.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

namespace {

class MahiFrameView : public NonClientFrameViewAsh {
 public:
  explicit MahiFrameView(views::Widget* frame) : NonClientFrameViewAsh(frame) {}
  ~MahiFrameView() override = default;
  MahiFrameView(const MahiFrameView&) = delete;
  MahiFrameView& operator=(const MahiFrameView&) = delete;

  // views::NonClientFrameView:
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(mahi_constants::kPanelDefaultWidth,
                     mahi_constants::kPanelDefaultHeight);
  }

  // views::NonClientFrameView:
  gfx::Size GetMaximumSize() const override {
    return gfx::Size(mahi_constants::kPanelMaximumWidth,
                     mahi_constants::kPanelMaximumHeight);
  }
};

}  // namespace

MahiWidgetDelegate::MahiWidgetDelegate() = default;
MahiWidgetDelegate::~MahiWidgetDelegate() = default;

std::unique_ptr<views::NonClientFrameView>
MahiWidgetDelegate::CreateNonClientFrameView(views::Widget* widget) {
  auto frame = std::make_unique<MahiFrameView>(widget);
  frame->SetFrameEnabled(false);
  frame->SetShouldPaintHeader(false);
  return frame;
}

}  // namespace ash
