// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCOPED_COMPOUND_TRANSFORM_RESOLVER_H_
#define CC_ANIMATION_SCOPED_COMPOUND_TRANSFORM_RESOLVER_H_

#include <map>

#include "cc/animation/animation_host.h"
#include "cc/trees/target_property.h"
#include "ui/gfx/transform.h"

namespace cc {

class CompoundTransform;

class CC_ANIMATION_EXPORT ScopedCompoundTransformResolver {
 public:
  explicit ScopedCompoundTransformResolver(AnimationHost* host);
  ~ScopedCompoundTransformResolver();
  ScopedCompoundTransformResolver(const ScopedCompoundTransformResolver&) =
      delete;
  ScopedCompoundTransformResolver& operator=(
      const ScopedCompoundTransformResolver&) = delete;

  void AddTransform(ElementId element_id,
                    const gfx::Transform& transform,
                    TargetProperty::Type target_property_type,
                    bool apply_active,
                    bool apply_pending);

 private:
  void ApplyCompoundTransforms(ElementId element_id,
                               ElementListType list_type,
                               const gfx::Transform* transforms);

  static constexpr size_t kNumTransforms =
      TargetProperty::LAST_TRANSFORM_PROPERTY -
      TargetProperty::FIRST_TRANSFORM_PROPERTY + 1;

  struct CompoundTransform {
    gfx::Transform active_transforms[kNumTransforms];
    gfx::Transform pending_transforms[kNumTransforms];
  };

  AnimationHost* animation_host_;
  std::map<ElementId, CompoundTransform> transform_map_;
};

}  // namespace cc

#endif  //  CC_ANIMATION_SCOPED_COMPOUND_TRANSFORM_RESOLVER_H_
