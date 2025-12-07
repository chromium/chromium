// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_MOCK_MEMORY_PRESSURE_LISTENER_H_
#define BASE_MEMORY_MOCK_MEMORY_PRESSURE_LISTENER_H_

#include "base/memory/memory_pressure_listener.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

class MockMemoryPressureListener : public MemoryPressureListener {
 public:
  MockMemoryPressureListener();
  ~MockMemoryPressureListener() override;

  MOCK_METHOD(void, OnMemoryPressure, (base::MemoryPressureLevel), (override));
};

// Same as MockMemoryPressureListener, but automatically registers with the
// global registry.
class RegisteredMockMemoryPressureListener : public MockMemoryPressureListener {
 public:
  RegisteredMockMemoryPressureListener();
  ~RegisteredMockMemoryPressureListener() override;

 private:
  SyncMemoryPressureListenerRegistration registration_;
};

// Async version of RegisteredMockMemoryPressureListener.
class RegisteredMockAsyncMemoryPressureListener
    : public MockMemoryPressureListener {
 public:
  RegisteredMockAsyncMemoryPressureListener();
  ~RegisteredMockAsyncMemoryPressureListener() override;

 private:
  AsyncMemoryPressureListenerRegistration registration_;
};

}  // namespace base

#endif  // BASE_MEMORY_MOCK_MEMORY_PRESSURE_LISTENER_H_
