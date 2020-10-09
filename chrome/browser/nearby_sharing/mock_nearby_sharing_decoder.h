// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_SHARING_DECODER_H_
#define CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_SHARING_DECODER_H_

#include "chromeos/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockNearbySharingDecoder : public sharing::mojom::NearbySharingDecoder {
 public:
  MockNearbySharingDecoder();
  explicit MockNearbySharingDecoder(const MockNearbySharingDecoder&) = delete;
  MockNearbySharingDecoder& operator=(const MockNearbySharingDecoder&) = delete;
  ~MockNearbySharingDecoder() override;

  // sharing::mojom::NearbySharingDecoder:
  MOCK_METHOD(void,
              DecodeAdvertisement,
              (const std::vector<uint8_t>& data,
               DecodeAdvertisementCallback callback),
              (override));
  MOCK_METHOD(void,
              DecodeFrame,
              (const std::vector<uint8_t>& data, DecodeFrameCallback callback),
              (override));
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_SHARING_DECODER_H_
