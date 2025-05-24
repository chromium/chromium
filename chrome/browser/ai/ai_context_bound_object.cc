// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_context_bound_object.h"

#include "chrome/browser/ai/ai_context_bound_object_set.h"

AIContextBoundObject::AIContextBoundObject(
    AIContextBoundObjectSet& context_bound_object_set)
    : context_bound_object_set_(context_bound_object_set) {}

AIContextBoundObject::~AIContextBoundObject() = default;

void AIContextBoundObject::RemoveFromSet() {
  context_bound_object_set_->RemoveContextBoundObject(this);
  // NOTE: do not write any logic after this, since the AIContextBoundObject is
  // removed.
}
