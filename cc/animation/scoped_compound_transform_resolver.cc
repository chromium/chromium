// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scoped_compound_transform_resolver.h"

#include "base/notreached.h"

namespace cc {

ScopedCompoundTransformResolver::ScopedCompoundTransformResolver(
    AnimationHost* animation_host)
    : animation_host_(animation_host) {
  DCHECK(animation_host_);
  animation_host_->set_compound_transform_resolver(this);
}

ScopedCompoundTransformResolver::~ScopedCompoundTransformResolver() {
  for (auto& it : transform_map_) {
    const ElementId element_id = it.first;
    const auto& compound_transform = it.second;
    ApplyCompoundTransforms(element_id, ElementListType::ACTIVE,
                            compound_transform.active_transforms);
    ApplyCompoundTransforms(element_id, ElementListType::PENDING,
                            compound_transform.pending_transforms);
  }
  animation_host_->set_compound_transform_resolver(nullptr);
}

void ScopedCompoundTransformResolver::ApplyCompoundTransforms(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform* transforms) {
  DCHECK(animation_host_);
  DCHECK(animation_host_->mutator_host_client());

  gfx::Transform result;
  for (size_t i = 0; i < kNumTransforms; i++) {
    const gfx::Transform& transform = transforms[i];
    if (!transform.IsIdentity())
      result.PreconcatTransform(transform);
  }
  if (!result.IsIdentity()) {
    animation_host_->mutator_host_client()->SetElementTransformMutated(
        element_id, list_type, result);
  }
}

void ScopedCompoundTransformResolver::AddTransform(
    ElementId element_id,
    const gfx::Transform& transform,
    TargetProperty::Type target_property_type,
    bool apply_active,
    bool apply_pending) {
  DCHECK(target_property_type >=
         TargetProperty::Type::FIRST_TRANSFORM_PROPERTY);
  DCHECK(target_property_type <= TargetProperty::Type::LAST_TRANSFORM_PROPERTY);
  size_t index =
      target_property_type - TargetProperty::Type::FIRST_TRANSFORM_PROPERTY;
  CompoundTransform& compound_transform = transform_map_[element_id];
  if (apply_active)
    compound_transform.active_transforms[index] = transform;
  if (apply_pending)
    compound_transform.pending_transforms[index] = transform;
}

}  // namespace cc
