// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_HIT_TEST_DATA_BUILDER_H_
#define CC_TREES_HIT_TEST_DATA_BUILDER_H_

#include <optional>

#include "base/memory/raw_ref.h"

namespace viz {
struct HitTestRegionList;
}

namespace cc {

class LayerTreeImpl;

class HitTestDataBuilder {
 public:
  explicit HitTestDataBuilder(const LayerTreeImpl& active_tree);

  std::optional<viz::HitTestRegionList> Build() &&;

 private:
  const raw_ref<const LayerTreeImpl> active_tree_;
};

}  // namespace cc

#endif  // CC_TREES_HIT_TEST_DATA_BUILDER_H_
