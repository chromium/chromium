// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_SET_H_
#define CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_SET_H_

#include <variant>

#include "base/containers/unique_ptr_adapters.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

// The data structure that supports adding and removing `AIContextBoundObject`.
class AIContextBoundObjectSet {
 public:
  explicit AIContextBoundObjectSet(on_device_model::mojom::Priority priority);
  AIContextBoundObjectSet(const AIContextBoundObjectSet&) = delete;
  AIContextBoundObjectSet& operator=(const AIContextBoundObjectSet&) = delete;
  ~AIContextBoundObjectSet();

  // Add an `AIContextBoundObject` into the set.
  void AddContextBoundObject(std::unique_ptr<AIContextBoundObject> object);
  // Returns the size of set.
  size_t GetSize() const;

  // Remove the `AIContextBoundObject` from the set.
  void RemoveContextBoundObject(AIContextBoundObject* object);

  // Sets the priority for all objects owned by this.
  void SetPriority(on_device_model::mojom::Priority priority);

  on_device_model::mojom::Priority priority() const { return priority_; }

 protected:
  on_device_model::mojom::Priority priority_;
  base::flat_set<std::unique_ptr<AIContextBoundObject>,
                 base::UniquePtrComparator>
      context_bound_object_set_;
};

#endif  // CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_SET_H_
