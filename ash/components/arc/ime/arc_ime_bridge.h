// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_
#define ASH_COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_

#include <string>

#include "ash/components/arc/mojom/ime.mojom-forward.h"
#include "base/functional/callback.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event.h"

namespace gfx {
class Range;
class Rect;
}  // namespace gfx

namespace ui {
struct CompositionText;
}  // namespace ui

namespace arc {

// This interface class encapsulates the detail of IME related IPC between
// Chromium and the ARC container.
class ArcImeBridge {
 public:
  ArcImeBridge(const ArcImeBridge&) = delete;
  ArcImeBridge& operator=(const ArcImeBridge&) = delete;

  virtual ~ArcImeBridge() {}

  // Received IPCs are deserialized and passed to this delegate.
  class Delegate {
   public:
    using KeyEventDoneCallback = base::OnceCallback<void(bool)>;

    virtual void OnTextInputTypeChanged(ui::TextInputType type,
                                        bool is_personalized_learning_allowed,
                                        int flags) = 0;
    virtual void OnCursorRectChanged(
        const gfx::Rect& rect,
        mojom::CursorCoordinateSpace coordinate_space) = 0;
    virtual void OnCancelComposition() = 0;
    virtual void ShowVirtualKeyboardIfEnabled() = 0;
    virtual void OnCursorRectChangedWithSurroundingText(
        const gfx::Rect& rect,
        const gfx::Range& text_range,
        const std::u16string& text_in_range,
        const gfx::Range& selection_range,
        mojom::CursorCoordinateSpace coordinate_space) = 0;
    virtual void SendKeyEvent(std::unique_ptr<ui::KeyEvent> key_event,
                              KeyEventDoneCallback callback) = 0;
  };

  // Serializes and sends IME related requests through IPCs.
  virtual void SendSetCompositionText(
      const ui::CompositionText& composition) = 0;
  virtual void SendConfirmCompositionText() = 0;
  virtual void SendSelectionRange(const gfx::Range& selection_range) = 0;
  virtual void SendInsertText(const std::u16string& text,
                              int new_cursor_position) = 0;
  virtual void SendExtendSelectionAndDelete(size_t before, size_t after) = 0;
  virtual void SendOnKeyboardAppearanceChanging(const gfx::Rect& new_bounds,
                                                bool is_available) = 0;
  virtual void SendSetComposingRegion(const gfx::Range& composing_range) = 0;

 protected:
  ArcImeBridge() {}
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_
