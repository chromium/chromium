// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_
#define ASH_COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_

#include <memory>
#include <optional>

#include "ash/components/arc/ime/arc_ime_bridge.h"
#include "ash/components/arc/ime/key_event_result_receiver.h"
#include "ash/components/arc/mojom/ime.mojom-forward.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
class InputMethod;
}  // namespace ui

namespace arc {

class ArcBridgeService;

// This class implements ui::TextInputClient and makes ARC windows behave
// as a text input target in Chrome OS environment.
class ArcImeService : public KeyedService,
                      public ArcImeBridge::Delegate,
                      public aura::EnvObserver,
                      public aura::WindowObserver,
                      public aura::client::FocusChangeObserver,
                      public ash::KeyboardControllerObserver,
                      public ui::TextInputClient {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcImeService* GetForBrowserContext(content::BrowserContext* context);

  class ArcWindowDelegate {
   public:
    virtual ~ArcWindowDelegate() = default;
    // Checks the |window| is a transient child of an ARC window.
    // This method assumes passed |window| is already attached to window
    // hierarchy.
    virtual bool IsInArcAppWindow(const aura::Window* window) const = 0;
    virtual void RegisterFocusObserver() = 0;
    virtual void UnregisterFocusObserver() = 0;
    virtual ui::InputMethod* GetInputMethodForWindow(
        aura::Window* window) const = 0;
  };

  ArcImeService(content::BrowserContext* context,
                ArcBridgeService* bridge_service);

  ArcImeService(const ArcImeService&) = delete;
  ArcImeService& operator=(const ArcImeService&) = delete;

  ~ArcImeService() override;

  // Injects the custom IPC bridge object for testing purpose only.
  void SetImeBridgeForTesting(std::unique_ptr<ArcImeBridge> test_ime_bridge);

  // Overridden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;
  void OnWindowRemoved(aura::Window* removed_window) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ArcImeBridge::Delegate:
  void OnTextInputTypeChanged(ui::TextInputType type,
                              bool is_personalized_learning_allowed,
                              int flags) override;
  void OnCursorRectChanged(
      const gfx::Rect& rect,
      mojom::CursorCoordinateSpace coordinate_space) override;
  void OnCancelComposition() override;
  void ShowVirtualKeyboardIfEnabled() override;
  void OnCursorRectChangedWithSurroundingText(
      const gfx::Rect& rect,
      const gfx::Range& text_range,
      const std::u16string& text_in_range,
      const gfx::Range& selection_range,
      mojom::CursorCoordinateSpace coordinate_space) override;
  void SendKeyEvent(std::unique_ptr<ui::KeyEvent> key_event,
                    KeyEventDoneCallback callback) override;

  // Overridden from ash::KeyboardControllerObserver.
  void OnKeyboardAppearanceChanged(
      const ash::KeyboardStateDescriptor& state) override;

  // Overridden from ui::TextInputClient:
  base::WeakPtr<ui::TextInputClient> AsWeakPtr() override;
  void SetCompositionText(const ui::CompositionText& composition) override;
  size_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const std::u16string& text,
                  InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  gfx::Rect GetCaretBounds() const override;
  gfx::Rect GetSelectionBoundingBox() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;

  // Overridden from ui::TextInputClient (with default implementation):
  // TODO(kinaba): Support each of these methods to the extent possible in
  // Android input method API.
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  bool GetCompositionCharacterBounds(size_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  FocusReason GetFocusReason() const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
  void OnInputMethodChanged() override {}
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override {
  }
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
  gfx::Range GetAutocorrectRange() const override;
  gfx::Rect GetAutocorrectCharacterBounds() const override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
  std::optional<ui::GrammarFragment> GetGrammarFragmentAtCursor()
      const override;
  bool ClearGrammarFragments(const gfx::Range& range) override;
  bool AddGrammarFragments(
      const std::vector<ui::GrammarFragment>& fragments) override;
  void OnDispatchingKeyEventPostIME(ui::KeyEvent* event) override;
  void GetActiveTextInputControlLayoutBounds(
      std::optional<gfx::Rect>* control_bounds,
      std::optional<gfx::Rect>* selection_bounds) override {}

  // Normally, the default device scale factor is used to convert from DPI to
  // physical pixels. This method provides a way to override it for testing.
  static void SetOverrideDefaultDeviceScaleFactorForTesting(
      std::optional<double> scale_factor);

  static void SetOverrideDisplayOriginForTesting(
      std::optional<gfx::Point> origin);

  static void EnsureFactoryBuilt();

 private:
  friend class ArcImeServiceTest;

  // Injects the custom delegate for ARC windows, for testing purpose only.
  ArcImeService(content::BrowserContext* context,
                ArcBridgeService* bridge_service,
                std::unique_ptr<ArcWindowDelegate> delegate);

  ui::InputMethod* GetInputMethod();

  // Detaches from the IME associated with the |old_window|, and attaches to the
  // IME associated with |new_window|. Called when the focus status of ARC
  // windows has changed, or when an ARC window moved to a different display.
  // Do nothing if both windows are associated with the same IME.
  void ReattachInputMethod(aura::Window* old_window, aura::Window* new_window);

  void InvalidateSurroundingTextAndSelectionRange();

  // Converts |rect| passed from the client to the host's cooridnates and
  // updates |cursor_rect_|. Returns whether or not the stored value changed.
  bool UpdateCursorRect(const gfx::Rect& rect,
                        mojom::CursorCoordinateSpace coordinate_space);

  // Returns true if this TextInputClient is active and incoming input state
  // from Android is valid.
  bool ShouldSendUpdateToInputMethod() const;

  double GetDeviceScaleFactorForKeyboard() const;
  double GetDeviceScaleFactorForFocusedWindow() const;
  double GetDefaultDeviceScaleFactor() const;

  gfx::Point GetDisplayOriginForFocusedWindow() const;

  std::unique_ptr<ArcImeBridge> ime_bridge_;
  std::unique_ptr<ArcWindowDelegate> arc_window_delegate_;
  ui::TextInputType ime_type_;
  // The flag is the bit map of ui::TextInputFlags.
  int ime_flags_;
  bool is_personalized_learning_allowed_;
  gfx::Rect cursor_rect_;
  bool has_composition_text_;
  gfx::Range text_range_;
  std::u16string text_in_range_;
  gfx::Range selection_range_;

  raw_ptr<aura::Window> focused_arc_window_ = nullptr;

  std::unique_ptr<KeyEventResultReceiver> receiver_;
  base::WeakPtrFactory<ArcImeService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_
