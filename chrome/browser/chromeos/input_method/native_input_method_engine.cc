// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"
#include "base/strings/string_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

namespace {

bool ShouldEngineUseMojo(const std::string& engine_id) {
  return base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE);
}

}  // namespace

NativeInputMethodEngine::NativeInputMethodEngine() = default;

NativeInputMethodEngine::~NativeInputMethodEngine() = default;

void NativeInputMethodEngine::Initialize(
    std::unique_ptr<InputMethodEngineBase::Observer> observer,
    const char* extension_id,
    Profile* profile) {
  // Wrap the given observer in our observer that will decide whether to call
  // Mojo directly or forward to the extension.
  auto native_observer =
      std::make_unique<chromeos::NativeInputMethodEngine::ImeObserver>(
          std::move(observer));
  InputMethodEngine::Initialize(std::move(native_observer), extension_id,
                                profile);
}

void NativeInputMethodEngine::FlushForTesting() {
  GetNativeObserver()->FlushForTesting();
}

bool NativeInputMethodEngine::IsConnectedForTesting() const {
  return GetNativeObserver()->IsConnectedForTesting();
}

NativeInputMethodEngine::ImeObserver*
NativeInputMethodEngine::GetNativeObserver() const {
  return static_cast<ImeObserver*>(observer_.get());
}

NativeInputMethodEngine::ImeObserver::ImeObserver(
    std::unique_ptr<InputMethodEngineBase::Observer> base_observer)
    : base_observer_(std::move(base_observer)), receiver_from_engine_(this) {
  input_method::InputMethodManager::Get()->ConnectInputEngineManager(
      remote_manager_.BindNewPipeAndPassReceiver());
}

NativeInputMethodEngine::ImeObserver::~ImeObserver() = default;

void NativeInputMethodEngine::ImeObserver::OnActivate(
    const std::string& engine_id) {
  if (ShouldEngineUseMojo(engine_id)) {
    // For legacy reasons, |engine_id| starts with "vkd_" in the input method
    // manifest, but the InputEngineManager expects the prefix "m17n:".
    // TODO(https://crbug.com/1012490): Migrate to m17n prefix and remove this.
    const auto new_engine_id = "m17n:" + engine_id.substr(4);
    remote_manager_->ConnectToImeEngine(
        new_engine_id, remote_to_engine_.BindNewPipeAndPassReceiver(),
        receiver_from_engine_.BindNewPipeAndPassRemote(), {},
        base::BindOnce(&ImeObserver::OnConnected, base::Unretained(this)));
  }
  base_observer_->OnActivate(engine_id);
}

void NativeInputMethodEngine::ImeObserver::OnFocus(
    const IMEEngineHandlerInterface::InputContext& context) {
  base_observer_->OnFocus(context);
}

void NativeInputMethodEngine::ImeObserver::OnBlur(int context_id) {
  base_observer_->OnBlur(context_id);
}

void NativeInputMethodEngine::ImeObserver::OnKeyEvent(
    const std::string& engine_id,
    const InputMethodEngineBase::KeyboardEvent& event,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) {
  base_observer_->OnKeyEvent(engine_id, event, std::move(callback));
}

void NativeInputMethodEngine::ImeObserver::OnReset(
    const std::string& engine_id) {
  base_observer_->OnReset(engine_id);
}

void NativeInputMethodEngine::ImeObserver::OnDeactivated(
    const std::string& engine_id) {
  base_observer_->OnDeactivated(engine_id);
}

void NativeInputMethodEngine::ImeObserver::OnCompositionBoundsChanged(
    const std::vector<gfx::Rect>& bounds) {
  base_observer_->OnCompositionBoundsChanged(bounds);
}

void NativeInputMethodEngine::ImeObserver::OnSurroundingTextChanged(
    const std::string& engine_id,
    const std::string& text,
    int cursor_pos,
    int anchor_pos,
    int offset_pos) {
  base_observer_->OnSurroundingTextChanged(engine_id, text, cursor_pos,
                                           anchor_pos, offset_pos);
}

void NativeInputMethodEngine::ImeObserver::OnInputContextUpdate(
    const IMEEngineHandlerInterface::InputContext& context) {
  base_observer_->OnInputContextUpdate(context);
}

void NativeInputMethodEngine::ImeObserver::OnCandidateClicked(
    const std::string& component_id,
    int candidate_id,
    InputMethodEngineBase::MouseButtonEvent button) {
  base_observer_->OnCandidateClicked(component_id, candidate_id, button);
}

void NativeInputMethodEngine::ImeObserver::OnMenuItemActivated(
    const std::string& component_id,
    const std::string& menu_id) {
  base_observer_->OnMenuItemActivated(component_id, menu_id);
}

void NativeInputMethodEngine::ImeObserver::OnScreenProjectionChanged(
    bool is_projected) {
  base_observer_->OnScreenProjectionChanged(is_projected);
}

void NativeInputMethodEngine::ImeObserver::FlushForTesting() {
  remote_manager_.FlushForTesting();
  receiver_from_engine_.FlushForTesting();
  remote_to_engine_.FlushForTesting();
}

void NativeInputMethodEngine::ImeObserver::OnConnected(bool bound) {
  connected_to_engine_ = bound;
}

}  // namespace chromeos
