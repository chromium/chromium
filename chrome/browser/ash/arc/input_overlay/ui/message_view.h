// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MESSAGE_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MESSAGE_VIEW_H_

#include "ash/public/cpp/view_shadow.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/views/controls/label.h"

namespace arc::input_overlay {
class DisplayOverlayController;

// MessageView shows info or error message on the top center of the window.
class MessageView : public views::LabelButton {
 public:
  static MessageView* Show(DisplayOverlayController* controller,
                           views::View* parent,
                           const base::StringPiece& message,
                           MessageType message_type);

  MessageView(DisplayOverlayController* controller,
              const gfx::Size& parent_size,
              const base::StringPiece& message,
              MessageType message_type);
  MessageView(const MessageView&) = delete;
  MessageView& operator=(const MessageView&) = delete;
  ~MessageView() override;

 private:
  void AddShadow();

  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  // View shadow for this view.
  std::unique_ptr<ash::ViewShadow> view_shadow_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MESSAGE_VIEW_H_
