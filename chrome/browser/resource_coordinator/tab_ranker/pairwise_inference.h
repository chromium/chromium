// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated using tf.native from a neural network trained by
// TensorFlow, then cleaned up by hand. Please do not edit except to update
// the constants for a new model. See native_inference.md for details.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_PAIRWISE_INFERENCE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_PAIRWISE_INFERENCE_H_

#include <cstdint>

namespace tab_ranker {
namespace pairwise_model {

constexpr int DNN_WEIGHTS_SIZE = 21200;
constexpr int DNN_RANK = 2;
constexpr int FEATURES_SIZE = 530;
constexpr int DNN_BIASES_SIZE = 40;

struct alignas(16) FixedAllocations {
  float alloc0[DNN_WEIGHTS_SIZE];
  float alloc1[DNN_BIASES_SIZE];
  int32_t shape0[DNN_RANK];
};

void Inference(
    /* size: FEATURES_SIZE */
    const float* __restrict features,
    /* size: 1 */
    float* __restrict prediction,
    FixedAllocations* __restrict fixed);

}  // namespace pairwise_model
}  // namespace tab_ranker

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_PAIRWISE_INFERENCE_H_
