// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/ui_default_factory.h"

#include <utility>

#include "chrome/browser/vr/ui.h"

#include "chrome/browser/vr/audio_delegate.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/text_input_delegate.h"

namespace vr {

UiDefaultFactory::~UiDefaultFactory() {}

std::unique_ptr<UiInterface> UiDefaultFactory::Create(
    UiBrowserInterface* browser,
    PlatformInputHandler* content_input_forwarder,
    std::unique_ptr<KeyboardDelegate> keyboard_delegate,
    std::unique_ptr<TextInputDelegate> text_input_delegate,
    std::unique_ptr<AudioDelegate> audio_delegate,
    const UiInitialState& ui_initial_state) {
  return std::make_unique<Ui>(browser, content_input_forwarder,
                              std::move(keyboard_delegate),
                              std::move(text_input_delegate),
                              std::move(audio_delegate), ui_initial_state);
}

std::unique_ptr<UiFactory> CreateUiFactory() {
  return std::make_unique<UiDefaultFactory>();
}

}  // namespace vr
