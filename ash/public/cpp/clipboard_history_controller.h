// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_
#define ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
enum class MenuAnchorPosition;
}  // namespace views

namespace ash {

class ScopedClipboardHistoryPause;

// An interface implemented in Ash to enable the Chrome side to show the
// clipboard history menu.
class ASH_PUBLIC_EXPORT ClipboardHistoryController {
 public:
  // Returns the singleton instance.
  static ClipboardHistoryController* Get();

  // Returns whether the clipboard history menu is able to show.
  virtual bool CanShowMenu() const = 0;

  // Shows the clipboard history menu triggered by `source_type` at the
  // specified position.
  virtual void ShowMenu(const gfx::Rect& anchor_rect,
                        views::MenuAnchorPosition menu_anchor_position,
                        ui::MenuSourceType source_type) = 0;

  // Creates a ScopedClipboardHistoryPause, which pauses ClipboardHistory for
  // its lifetime.
  virtual std::unique_ptr<ScopedClipboardHistoryPause> CreateScopedPause() = 0;

 protected:
  ClipboardHistoryController();
  virtual ~ClipboardHistoryController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_
