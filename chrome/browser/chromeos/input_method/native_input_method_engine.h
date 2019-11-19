// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_

#include "chrome/browser/chromeos/input_method/input_method_engine.h"

#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// An InputMethodEngine used for the official Chrome OS build. It's a bridge
// between the Chrome OS input framework and the IME service. Although it
// currently depends on the Chrome extension system, it will not need to in
// the future after all the extensions code are migrated to the IME service.
// The design of this class is not good, mainly because we are inheriting
// from InputMethodEngineBase, which was designed for extension-based engines.
//
// In the final design, there should be some common interface between
// NativeInputMethodEngine and "ExtensionInputMethodEngine" (which is
// InputMethodEngineBase in the current design). All extensions-related logic
// will reside in the ExtensionInputMethodEngine inheritance tree. There will
// be no "ImeObserver" for the native engine either, as it is only used as
// a way for ExtensionInputMethodEngine to delegate to the extensions code,
// which is not required for the native engine.
class NativeInputMethodEngine : public InputMethodEngine {
 public:
  NativeInputMethodEngine();
  ~NativeInputMethodEngine() override;

  // InputMethodEngine:
  void Initialize(std::unique_ptr<InputMethodEngineBase::Observer> observer,
                  const char* extension_id,
                  Profile* profile) override;

  // Flush all relevant Mojo pipes.
  void FlushForTesting();

  // Returns whether this is connected to the input engine.
  bool IsConnectedForTesting() const;

 private:
  class ImeObserver : public InputMethodEngineBase::Observer,
                      public ime::mojom::InputChannel {
   public:
    // |base_observer| is to forward events to extension during this migration.
    // It will be removed when the official extension is completely migrated.
    explicit ImeObserver(
        std::unique_ptr<InputMethodEngineBase::Observer> base_observer);
    ~ImeObserver() override;

    // InputMethodEngineBase::Observer:
    void OnActivate(const std::string& engine_id) override;
    void OnFocus(
        const IMEEngineHandlerInterface::InputContext& context) override;
    void OnBlur(int context_id) override;
    void OnKeyEvent(
        const std::string& engine_id,
        const InputMethodEngineBase::KeyboardEvent& event,
        ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) override;
    void OnReset(const std::string& engine_id) override;
    void OnDeactivated(const std::string& engine_id) override;
    void OnCompositionBoundsChanged(
        const std::vector<gfx::Rect>& bounds) override;
    void OnSurroundingTextChanged(const std::string& engine_id,
                                  const std::string& text,
                                  int cursor_pos,
                                  int anchor_pos,
                                  int offset_pos) override;
    void OnInputContextUpdate(
        const IMEEngineHandlerInterface::InputContext& context) override;
    void OnCandidateClicked(
        const std::string& component_id,
        int candidate_id,
        InputMethodEngineBase::MouseButtonEvent button) override;
    void OnMenuItemActivated(const std::string& component_id,
                             const std::string& menu_id) override;
    void OnScreenProjectionChanged(bool is_projected) override;

    // mojom::InputChannel:
    void ProcessMessage(const std::vector<uint8_t>& message,
                        ProcessMessageCallback callback) override {}
    void ProcessKeypressForRulebased(
        ime::mojom::KeypressInfoForRulebasedPtr message,
        ProcessKeypressForRulebasedCallback callback) override {}
    void ResetForRulebased() override {}
    void GetRulebasedKeypressCountForTesting(
        GetRulebasedKeypressCountForTestingCallback callback) override {}

    // Flush all relevant Mojo pipes.
    void FlushForTesting();

    // Returns whether this is connected to the input engine.
    bool IsConnectedForTesting() const { return connected_to_engine_; }

   private:
    // Called when this is connected to the input engine. |bound| indicates
    // the success of the connection.
    void OnConnected(bool bound);

    std::unique_ptr<InputMethodEngineBase::Observer> base_observer_;
    mojo::Remote<ime::mojom::InputEngineManager> remote_manager_;
    mojo::Receiver<ime::mojom::InputChannel> receiver_from_engine_;
    mojo::Remote<ime::mojom::InputChannel> remote_to_engine_;
    bool connected_to_engine_ = false;
  };

  ImeObserver* GetNativeObserver() const;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_NATIVE_INPUT_METHOD_ENGINE_H_
