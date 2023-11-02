// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_EMOJI_PICKER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_EMOJI_PICKER_ASH_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/emoji_picker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the EmojiPicker crosapi interface.
// This class must only be used from the main thread.
class EmojiPickerAsh : public mojom::EmojiPicker {
 public:
  EmojiPickerAsh();
  EmojiPickerAsh(const EmojiPickerAsh&) = delete;
  EmojiPickerAsh& operator=(const EmojiPickerAsh&) = delete;
  ~EmojiPickerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::EmojiPicker> receiver);
  // mojom::EmojiPicker
  void ShowEmojiPicker() override;

 private:
  mojo::ReceiverSet<mojom::EmojiPicker> receivers_;

  base::WeakPtrFactory<EmojiPickerAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_EMOJI_PICKER_ASH_H_
