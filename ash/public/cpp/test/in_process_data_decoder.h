// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_IN_PROCESS_DATA_DECODER_H_
#define ASH_PUBLIC_CPP_TEST_IN_PROCESS_DATA_DECODER_H_

#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

namespace ash {

// Exactly the same as `data_decoder::test::InProcessDataDecoder`, except
// substitutes a custom implementation of image decoding that avoids the
// dependency on blink that `data_decoder::ImageDecoderImpl` has. Blink is an
// undesirable dependency in ash.
class InProcessDataDecoder : public data_decoder::test::InProcessDataDecoder {
 public:
  InProcessDataDecoder();
  InProcessDataDecoder(const InProcessDataDecoder&) = delete;
  InProcessDataDecoder& operator=(const InProcessDataDecoder&) = delete;
  ~InProcessDataDecoder() override;

 private:
  std::unique_ptr<data_decoder::mojom::ImageDecoder> CreateCustomImageDecoder()
      override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_IN_PROCESS_DATA_DECODER_H_
