// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/features.h"

namespace extensions::image_writer {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kImageWriterPrivateAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace extensions::image_writer
