// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_MOCK_FAST_PAIR_IMAGE_DECODER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_MOCK_FAST_PAIR_IMAGE_DECODER_H_

#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class MockFastPairImageDecoder : public FastPairImageDecoder {
 public:
  MockFastPairImageDecoder();
  MockFastPairImageDecoder(const MockFastPairImageDecoder&) = delete;
  MockFastPairImageDecoder& operator=(const MockFastPairImageDecoder&) = delete;
  ~MockFastPairImageDecoder() override;

  MOCK_METHOD(void,
              DecodeImageFromUrl,
              (const GURL& image_url,
               bool resize_to_notification_size,
               DecodeImageCallback on_image_decoded_callback),
              (override));

  MOCK_METHOD(void,
              DecodeImage,
              (const std::vector<uint8_t>& encoded_image_bytes,
               bool resize_to_notification_size,
               DecodeImageCallback on_image_decoded_callback),
              (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_MOCK_FAST_PAIR_IMAGE_DECODER_H_
