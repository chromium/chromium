// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_INPUT_METHOD_TEST_INTERFACE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_INPUT_METHOD_TEST_INTERFACE_ASH_H_

#include <queue>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "extensions/common/extension_id.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/ash/text_input_method.h"

namespace crosapi {

class FakeTextInputMethod : public ash::TextInputMethod {
 public:
  class Observer {
   public:
    virtual void OnFocus() = 0;
    virtual void OnSurroundingTextChanged(
        const std::u16string& text,
        const gfx::Range& selection_range) = 0;
  };

  FakeTextInputMethod();
  ~FakeTextInputMethod() override;

  void Focus(const InputContext& input_context) override;
  void Blur() override {}
  void Enable(const std::string& component_id) override {}
  void Disable() override {}
  void Reset() override {}
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback callback) override;
  void SetSurroundingText(const std::u16string& text,
                          const gfx::Range selection_range,
                          uint32_t offset_pos) override;
  void SetCaretBounds(const gfx::Rect& caret_bounds) override {}
  ui::VirtualKeyboardController* GetVirtualKeyboardController() const override;
  void PropertyActivate(const std::string& property_name) override {}
  void CandidateClicked(uint32_t index) override {}
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) override {}
  void AssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) override {}
  bool IsReadyForTesting() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  uint64_t GetCurrentKeyEventId() const;
  void KeyEventHandled(uint64_t key_event_id, bool handled);

 private:
  uint64_t current_key_event_id_ = 0;
  std::map<uint64_t, KeyEventDoneCallback> pending_key_event_callbacks_;

  std::u16string previous_surrounding_text_;
  gfx::Range previous_selection_range_;

  base::ObserverList<Observer>::Unchecked observers_;
};

class InputMethodTestInterfaceAsh : public mojom::InputMethodTestInterface,
                                    public FakeTextInputMethod::Observer {
 public:
  InputMethodTestInterfaceAsh();
  ~InputMethodTestInterfaceAsh() override;
  InputMethodTestInterfaceAsh(const InputMethodTestInterfaceAsh&) = delete;
  InputMethodTestInterfaceAsh& operator=(const InputMethodTestInterfaceAsh&) =
      delete;

  // mojom::InputMethodTestInterface:
  void WaitForFocus(WaitForFocusCallback callback) override;
  void CommitText(const std::string& text,
                  CommitTextCallback callback) override;
  void SetComposition(const std::string& text,
                      uint32_t index,
                      SetCompositionCallback callback) override;
  void SendKeyEvent(mojom::KeyEventPtr event,
                    SendKeyEventCallback callback) override;
  void KeyEventHandled(uint64_t key_event_id,
                       bool handled,
                       KeyEventHandledCallback callback) override;
  void WaitForNextSurroundingTextChange(
      WaitForNextSurroundingTextChangeCallback callback) override;
  void HasCapabilities(const std::vector<std::string>& capabilities,
                       HasCapabilitiesCallback callback) override;
  void ConfirmComposition(ConfirmCompositionCallback callback) override;
  void DeleteSurroundingText(uint32_t length_before_selection,
                             uint32_t length_after_selection,
                             DeleteSurroundingTextCallback callback) override;
  void InstallAndSwitchToInputMethod(
      mojom::InputMethodPtr input_method,
      InstallAndSwitchToInputMethodCallback callback) override;

  // FakeTextInputMethod::Observer:
  void OnFocus() override;
  void OnSurroundingTextChanged(const std::u16string& text,
                                const gfx::Range& selection_range) override;

 private:
  struct SurroundingText {
    std::string text;
    gfx::Range selection_range;
  };

  // This installs a new input method upon instantiation and uninstalls it on
  // destruction.
  class ScopedInputMethodInstall {
   public:
    explicit ScopedInputMethodInstall(const mojom::InputMethod& input_method,
                                      ash::TextInputMethod* text_input_method);
    ~ScopedInputMethodInstall();

    const std::string& extension_id() const;
    std::string GetInputMethodId() const;

   private:
    extensions::ExtensionId extension_id_;
  };

  raw_ptr<ash::InputMethodAsh> text_input_target_;
  FakeTextInputMethod fake_text_input_method_;
  // For testing, only allow one input method to be installed.
  std::unique_ptr<ScopedInputMethodInstall> installed_input_method_;
  base::ScopedObservation<FakeTextInputMethod, FakeTextInputMethod::Observer>
      text_input_method_observation_{this};
  base::OnceClosureList focus_callbacks_;
  std::queue<SurroundingText> surrounding_text_changes_;
  WaitForNextSurroundingTextChangeCallback surrounding_text_change_callback_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_INPUT_METHOD_TEST_INTERFACE_ASH_H_
