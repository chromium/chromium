// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_DESTRUCTION_OBSERVER_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_DESTRUCTION_OBSERVER_H_

#include "base/observer_list_types.h"

namespace base {

class MemoryConsumerRegistryDestructionObserver : public CheckedObserver {
 public:
  virtual void OnBeforeMemoryConsumerRegistryDestroyed() = 0;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_DESTRUCTION_OBSERVER_H_
