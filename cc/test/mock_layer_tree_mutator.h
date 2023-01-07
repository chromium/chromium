// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_MOCK_LAYER_TREE_MUTATOR_H_
#define CC_TEST_MOCK_LAYER_TREE_MUTATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "cc/trees/layer_tree_mutator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cc {

class MockLayerTreeMutator : public LayerTreeMutator {
 public:
  MockLayerTreeMutator();
  ~MockLayerTreeMutator() override;
  // gmock cannot mock methods with move-only args so we forward it ourself.
  bool Mutate(std::unique_ptr<MutatorInputState> input_state,
              MutateQueuingStrategy queueing_strategy,
              DoneCallback done_callback) override {
    MutateRef(input_state.get());
    return true;
  }

  MOCK_METHOD1(MutateRef, void(MutatorInputState* input_state));
  MOCK_METHOD1(SetClient, void(LayerTreeMutatorClient* client));
  MOCK_METHOD0(HasMutators, bool());
};

}  // namespace cc

#endif  // CC_TEST_MOCK_LAYER_TREE_MUTATOR_H_
