// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SCROLL_SOURCE_TYPE_H_
#define CC_TREES_SCROLL_SOURCE_TYPE_H_

namespace cc {

// https://drafts.csswg.org/css-scroll-snap-1/#scroll-types
enum class ScrollSourceType {
  kNone,
  kAbsoluteScroll,
  kRelativeScroll,
  kStationaryScroll,
};

}  // namespace cc

#endif  // CC_TREES_SCROLL_SOURCE_TYPE_H_
