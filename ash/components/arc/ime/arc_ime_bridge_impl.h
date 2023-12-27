// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_IME_ARC_IME_BRIDGE_IMPL_H_
#define ASH_COMPONENTS_ARC_IME_ARC_IME_BRIDGE_IMPL_H_

#include <string>

#include "ash/components/arc/ime/arc_ime_bridge.h"
#include "ash/components/arc/mojom/ime.mojom.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
struct CompositionText;
}  // namespace ui

namespace arc {

class ArcBridgeService;

// This class encapsulates the detail of IME related IPC between
// Chromium and the ARC container.
class ArcImeBridgeImpl : public ArcImeBridge, public mojom::ImeHost {
 public:
  ArcImeBridgeImpl(Delegate* delegate, ArcBridgeService* bridge_service);

  ArcImeBridgeImpl(const ArcImeBridgeImpl&) = delete;
  ArcImeBridgeImpl& operator=(const ArcImeBridgeImpl&) = delete;

  ~ArcImeBridgeImpl() override;

  // ArcImeBridge overrides:
  void SendSetCompositionText(const ui::CompositionText& composition) override;
  void SendConfirmCompositionText() override;
  void SendInsertText(const std::u16string& text,
                      int new_cursor_position) override;
  void SendExtendSelectionAndDelete(size_t before, size_t after) override;
  void SendOnKeyboardAppearanceChanging(const gfx::Rect& new_bounds,
                                        bool is_available) override;
  void SendSelectionRange(const gfx::Range& selection_range) override;
  void SendSetComposingRegion(const gfx::Range& composing_range) override;

  // mojom::ImeHost overrides:
  void OnTextInputTypeChanged(ui::TextInputType type,
                              bool is_personalized_learning_allowed,
                              int32_t flags) override;
  void OnCursorRectChanged(
      const gfx::Rect& rect,
      mojom::CursorCoordinateSpace coordinate_space) override;
  void OnCancelComposition() override;
  void ShowVirtualKeyboardIfEnabled() override;
  void OnCursorRectChangedWithSurroundingText(
      const gfx::Rect& rect,
      const gfx::Range& text_range,
      const std::string& text_in_range,
      const gfx::Range& selection_range,
      mojom::CursorCoordinateSpace coordinate_space) override;
  void SendKeyEvent(std::unique_ptr<ui::KeyEvent> key_event,
                    SendKeyEventCallback callback) override;

 private:
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<ArcBridgeService> bridge_service_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_IME_ARC_IME_BRIDGE_IMPL_H_
