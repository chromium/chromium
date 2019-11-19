// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DATA_DECODER_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DATA_DECODER_UTIL_H_

#include "services/data_decoder/public/cpp/data_decoder.h"

namespace media_router {

// Returns a shared DataDecoder instance used to handle all decoding operations
// related to Media Router.
data_decoder::DataDecoder& GetDataDecoder();

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DATA_DECODER_UTIL_H_
