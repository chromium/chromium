// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list_internal.h"

namespace base::internal {

RuntimeMutableFeatureState::RuntimeMutableFeatureState(
    const Feature& feature,
    FeatureList::OnRuntimeMutableFeatureStateChangedCallback
        pre_mutation_callback,
    FeatureList::OnRuntimeMutableFeatureStateChangedCallback
        post_mutation_callback)
    : feature(feature),
      pre_mutation_callback(std::move(pre_mutation_callback)),
      post_mutation_callback(std::move(post_mutation_callback)) {}

RuntimeMutableFeatureState::~RuntimeMutableFeatureState() = default;

RuntimeMutableFeatureState::RuntimeMutableFeatureState(
    const RuntimeMutableFeatureState&) = default;

RuntimeMutableFeatureState::RuntimeMutableFeatureState(
    RuntimeMutableFeatureState&&) = default;

RuntimeMutableFeatureState& RuntimeMutableFeatureState::operator=(
    const RuntimeMutableFeatureState&) = default;

RuntimeMutableFeatureState& RuntimeMutableFeatureState::operator=(
    RuntimeMutableFeatureState&&) = default;

}  // namespace base::internal
