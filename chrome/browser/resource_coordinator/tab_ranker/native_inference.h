// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated using tf.native from a neural network trained by
// TensorFlow, then cleaned up by hand. Please do not edit except to update
// the constants for a new model. See native_inference.md for details.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_NATIVE_INFERENCE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_NATIVE_INFERENCE_H_

#include <cstdint>

namespace tab_ranker {
namespace tfnative_model {

constexpr int DNN_RANK = 2;
constexpr int FEATURES_SIZE = 275;
constexpr int DNN_BIASES_SIZE = 40;
constexpr int DNN_WEIGHTS_SIZE = FEATURES_SIZE * DNN_BIASES_SIZE;

struct alignas(16) FixedAllocations {
  float alloc0[DNN_BIASES_SIZE];
  int32_t alloc0_shape[DNN_RANK];
  float alloc1[DNN_BIASES_SIZE];
  int32_t alloc1_shape[DNN_RANK];
};

void Inference(
    /* size: FEATURES_SIZE */
    const float* __restrict features,
    /* size: 1 */
    float* __restrict prediction,
    FixedAllocations* __restrict fixed);

}  // namespace tfnative_model
}  // namespace tab_ranker

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_NATIVE_INFERENCE_H_
