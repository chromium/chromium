// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/emoji_picker_ash.h"

#include <utility>
#include "ui/base/emoji/emoji_panel_helper.h"

namespace crosapi {

EmojiPickerAsh::EmojiPickerAsh() = default;
EmojiPickerAsh::~EmojiPickerAsh() = default;

void EmojiPickerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::EmojiPicker> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void EmojiPickerAsh::ShowEmojiPicker() {
  ui::ShowEmojiPanel();
}
}  // namespace crosapi
