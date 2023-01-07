// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_QUICK_START_DECODER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_QUICK_START_DECODER_H_

#include <vector>

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::quick_start {

// QuickStartDecoder is a class on the utility process that will
// accept incoming raw bytes from an Android device, decode the
// bytes and parse them into secure structs that can be consumed
// by the browser process.
class QuickStartDecoder : public mojom::QuickStartDecoder {
 public:
  explicit QuickStartDecoder(
      mojo::PendingReceiver<mojom::QuickStartDecoder> receiver);
  QuickStartDecoder(const QuickStartDecoder&) = delete;
  QuickStartDecoder& operator=(const QuickStartDecoder&) = delete;
  ~QuickStartDecoder() override;

  // mojom::QuickStartDecoder;
  void DecodeGetAssertionResponse(
      const std::vector<uint8_t>& data,
      DecodeGetAssertionResponseCallback callback) override;

 private:
  friend class QuickStartDecoderTest;
  mojom::GetAssertionResponsePtr DoDecodeGetAssertionResponse(
      const std::vector<uint8_t>& data);
  mojo::Receiver<mojom::QuickStartDecoder> receiver_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_QUICK_START_DECODER_H_
