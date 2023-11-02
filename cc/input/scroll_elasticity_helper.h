// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_ELASTICITY_HELPER_H_
#define CC_INPUT_SCROLL_ELASTICITY_HELPER_H_

#include "cc/cc_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class LayerTreeHostImpl;

// ScrollElasticityHelper is based on
// WebKit/Source/platform/mac/ScrollElasticityController.h
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Interface between a LayerTreeHostImpl and the ScrollElasticityController. It
// would be possible, in principle, for LayerTreeHostImpl to implement this
// interface itself. This artificial boundary is introduced to reduce the amount
// of logic and state held directly inside LayerTreeHostImpl.
class CC_EXPORT ScrollElasticityHelper {
 public:
  static ScrollElasticityHelper* CreateForLayerTreeHostImpl(
      LayerTreeHostImpl* host_impl);

  virtual ~ScrollElasticityHelper() {}

  virtual bool IsUserScrollableHorizontal() const = 0;
  virtual bool IsUserScrollableVertical() const = 0;

  // The bounds of the root scroller.
  virtual gfx::Size ScrollBounds() const = 0;

  // The amount that the view is stretched past the normal allowable bounds.
  virtual gfx::Vector2dF StretchAmount() const = 0;
  virtual void SetStretchAmount(const gfx::Vector2dF& stretch_amount) = 0;

  // Functions for the scrolling of the root scroll layer.
  virtual gfx::PointF ScrollOffset() const = 0;
  virtual gfx::PointF MaxScrollOffset() const = 0;
  virtual void ScrollBy(const gfx::Vector2dF& delta) = 0;

  // Requests that another frame happens for the controller to continue ticking
  // animations.
  virtual void RequestOneBeginFrame() = 0;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_ELASTICITY_HELPER_H_
