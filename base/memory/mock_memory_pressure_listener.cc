// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/mock_memory_pressure_listener.h"

namespace base {

MockMemoryPressureListener::MockMemoryPressureListener() = default;

MockMemoryPressureListener::~MockMemoryPressureListener() = default;

RegisteredMockMemoryPressureListener::RegisteredMockMemoryPressureListener()
    : registration_(MemoryPressureListenerTag::kTest, this) {}

RegisteredMockMemoryPressureListener::~RegisteredMockMemoryPressureListener() =
    default;

RegisteredMockAsyncMemoryPressureListener::
    RegisteredMockAsyncMemoryPressureListener()
    : registration_(FROM_HERE, MemoryPressureListenerTag::kTest, this) {}

RegisteredMockAsyncMemoryPressureListener::
    ~RegisteredMockAsyncMemoryPressureListener() = default;

}  // namespace base
