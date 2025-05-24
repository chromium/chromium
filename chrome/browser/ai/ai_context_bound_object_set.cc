// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_context_bound_object_set.h"

#include <memory>

AIContextBoundObjectSet::AIContextBoundObjectSet(
    on_device_model::mojom::Priority priority)
    : priority_(priority) {}
AIContextBoundObjectSet::~AIContextBoundObjectSet() = default;

void AIContextBoundObjectSet::AddContextBoundObject(
    std::unique_ptr<AIContextBoundObject> object) {
  object->SetPriority(priority_);
  context_bound_object_set_.insert(std::move(object));
}

void AIContextBoundObjectSet::RemoveContextBoundObject(
    AIContextBoundObject* object) {
  context_bound_object_set_.erase(object);
}

void AIContextBoundObjectSet::SetPriority(
    on_device_model::mojom::Priority priority) {
  priority_ = priority;
  for (auto& object : context_bound_object_set_) {
    object->SetPriority(priority);
  }
}

size_t AIContextBoundObjectSet::GetSizeForTesting() {
  return context_bound_object_set_.size();
}
