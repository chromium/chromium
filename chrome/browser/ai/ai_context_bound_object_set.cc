// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_context_bound_object_set.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace {

const char kAIContextBoundObjectSetUserDataKey[] = "ai_context_bound_objects";

}  // namespace

AIContextBoundObjectSet::AIContextBoundObjectSet() = default;
AIContextBoundObjectSet::~AIContextBoundObjectSet() = default;

void AIContextBoundObjectSet::AddContextBoundObject(
    std::unique_ptr<AIContextBoundObject> object) {
  context_bound_object_set_.insert(std::move(object));
}

void AIContextBoundObjectSet::RemoveContextBoundObject(
    AIContextBoundObject* object) {
  context_bound_object_set_.erase(object);
}

AIContextBoundObjectSet* AIContextBoundObjectSet::GetFromContext(
    base::SupportsUserData& context_user_data) {
  if (!context_user_data.GetUserData(kAIContextBoundObjectSetUserDataKey)) {
    context_user_data.SetUserData(kAIContextBoundObjectSetUserDataKey,
                                  // Constructor is
                                  std::make_unique<AIContextBoundObjectSet>());
  }
  return static_cast<AIContextBoundObjectSet*>(
      context_user_data.GetUserData(kAIContextBoundObjectSetUserDataKey));
}

size_t AIContextBoundObjectSet::GetSizeForTesting() {
  return context_bound_object_set_.size();
}
