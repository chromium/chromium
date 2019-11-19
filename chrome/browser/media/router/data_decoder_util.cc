// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/data_decoder_util.h"

#include "base/no_destructor.h"

namespace media_router {

data_decoder::DataDecoder& GetDataDecoder() {
  static base::NoDestructor<data_decoder::DataDecoder> decoder;
  return *decoder;
}

}  // namespace media_router
