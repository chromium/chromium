// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MOCK_MEMORY_CONSUMER_H_
#define BASE_MEMORY_COORDINATOR_MOCK_MEMORY_CONSUMER_H_

#include "base/memory_coordinator/memory_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

class MockMemoryConsumer : public MemoryConsumer {
 public:
  MockMemoryConsumer();
  ~MockMemoryConsumer() override;

  MOCK_METHOD(void, OnUpdateMemoryLimit, (), (override));
  MOCK_METHOD(void, OnReleaseMemory, (), (override));
};

// Same as MockMemoryConsumer, but automatically registers with the global
// registry.
class RegisteredMockMemoryConsumer : public MockMemoryConsumer {
 public:
  RegisteredMockMemoryConsumer(std::string_view consumer_id,
                               MemoryConsumerTraits traits);
  ~RegisteredMockMemoryConsumer() override;

 private:
  MemoryConsumerRegistration registration_;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MOCK_MEMORY_CONSUMER_H_
