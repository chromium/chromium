// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/generator/android/noop_thumbnail_media_parser.h"

#include "third_party/skia/include/core/SkBitmap.h"

void NoopThumbnailMediaParser::Start(ParseCompleteCB parse_complete_cb) {
  DCHECK(parse_complete_cb);
  std::move(parse_complete_cb)
      .Run(false /*success*/, chrome::mojom::MediaMetadata::New(), SkBitmap());
}
