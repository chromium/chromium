// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MESSAGE_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MESSAGE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/views/controls/label.h"

namespace arc {
namespace input_overlay {
class DisplayOverlayController;

// MessageView shows when the input for editing key binding is invalid. It shows
// near the focused ActionView.
class MessageView : public views::Label {
 public:
  MessageView(DisplayOverlayController* controller,
              ActionView* view,
              base::StringPiece text);
  MessageView(const MessageView&) = delete;
  MessageView& operator=(const MessageView&) = delete;
  ~MessageView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MESSAGE_VIEW_H_
