// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/icon_decoder_impl.h"

namespace ash {
namespace phonehub {

IconDecoder::DecodingData::DecodingData(unsigned long id,
                                        const std::string& input_data)
    : id(id), input_data(input_data) {}

}  // namespace phonehub
}  // namespace ash
