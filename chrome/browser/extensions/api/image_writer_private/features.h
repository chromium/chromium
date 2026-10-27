// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_FEATURES_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace extensions::image_writer {

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kImageWriterPrivateAndroid);
#endif

}  // namespace extensions::image_writer

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_FEATURES_H_
