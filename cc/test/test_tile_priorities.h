// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_TILE_PRIORITIES_H_
#define CC_TEST_TEST_TILE_PRIORITIES_H_

#include "cc/tiles/tile_priority.h"

namespace cc {

class TilePriorityForSoonBin : public TilePriority {
 public:
  TilePriorityForSoonBin();
};

class TilePriorityForEventualBin : public TilePriority {
 public:
  TilePriorityForEventualBin();
};

class TilePriorityForNowBin : public TilePriority {
 public:
  TilePriorityForNowBin();
};

class TilePriorityLowRes : public TilePriority {
 public:
  TilePriorityLowRes();
};

}  // namespace cc

#endif  // CC_TEST_TEST_TILE_PRIORITIES_H_
