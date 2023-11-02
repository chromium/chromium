// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_ANALYSIS_STATE_H_
#define CC_PAINT_IMAGE_ANALYSIS_STATE_H_

namespace cc {

enum class ImageAnalysisState {
  kNoAnalysis,
  kAnimatedImages,
  kNoAnimatedImages,
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_ANALYSIS_STATE_H_
