// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_ACTIVELY_SCROLLING_TYPE_H_
#define CC_INPUT_ACTIVELY_SCROLLING_TYPE_H_

namespace cc {

enum class ActivelyScrollingType {
  kNone,
  kPrecise,
  kAnimated,
};

}  // namespace cc

#endif  // CC_INPUT_ACTIVELY_SCROLLING_TYPE_H_
