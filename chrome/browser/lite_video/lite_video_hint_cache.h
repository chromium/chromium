// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_CACHE_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_CACHE_H_

#include <stdint.h>

#include "base/sequence_checker.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace lite_video {

class LiteVideoHint;

// The LiteVideoHintCache holds the necessary information, keyed by origin,
// needed to configure throttling performed by the LiteVideo optimization.
class LiteVideoHintCache {
 public:
  LiteVideoHintCache();
  virtual ~LiteVideoHintCache();

  // Returns a LiteVideoHint if one exists for the navigation URL.
  // Virtual for testing.
  virtual absl::optional<LiteVideoHint> GetHintForNavigationURL(
      const GURL& url) const;

 private:
  // The set of hints, keyed by origin, available to the hint cache.
  const absl::optional<base::Value> origin_hints_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_CACHE_H_
