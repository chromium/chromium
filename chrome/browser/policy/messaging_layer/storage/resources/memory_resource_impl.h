// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_RESOURCES_MEMORY_RESOURCE_IMPL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_RESOURCES_MEMORY_RESOURCE_IMPL_H_

#include <atomic>
#include <cstdint>

#include "chrome/browser/policy/messaging_layer/storage/resources/resource_interface.h"

namespace reporting {

// Interface to resources management by Storage module.
// Must be implemented by the caller base on the platform limitations.
// All APIs are non-blocking.
class MemoryResourceImpl : public ResourceInterface {
 public:
  MemoryResourceImpl();
  ~MemoryResourceImpl() override;

  // Implementation of ResourceInterface methods.
  bool Reserve(int64_t size) override;
  void Discard(int64_t size) override;
  int64_t GetTotal() override;
  int64_t GetUsed() override;
  void Test_SetTotal(int64_t test_total) override;

 private:
  int64_t total_;
  std::atomic<int64_t> used_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_RESOURCES_MEMORY_RESOURCE_IMPL_H_
