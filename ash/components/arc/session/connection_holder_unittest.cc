// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/connection_holder.h"

#include "ash/components/arc/session/connection_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

struct FakeInstance {
  static constexpr uint32_t Version_ = 1;
  static constexpr char Name_[] = "FakeInstance";
};

struct Observer : public ConnectionObserver<FakeInstance> {
  void OnConnectionReady() override { ++connection_ready_count_; }
  void OnConnectionClosed() override { ++connection_closed_count_; }

  size_t connection_ready_count_ = 0;
  size_t connection_closed_count_ = 0;
};

// Tests that ConnectionHolder<>::AddObserver() works as intended.
TEST(ConnectionHolder, AddObserver) {
  Observer observer_1;
  Observer observer_2;

  ConnectionHolder<FakeInstance> holder;
  holder.AddObserver(&observer_1);

  // Since the holder doesn't have the instance yet, OnConnectionReady()
  // shouldn't be called back.
  EXPECT_EQ(0u, observer_1.connection_ready_count_);

  FakeInstance instance;
  holder.SetInstance(&instance);
  EXPECT_TRUE(holder.IsConnected());

  // Since the holder got the instance, OnConnectionReady() should have been
  // called.
  EXPECT_EQ(1u, observer_1.connection_ready_count_);

  // Add the second observer for the same holder. OnConnectionReady() should be
  // called back immediately for that observer.
  holder.AddObserver(&observer_2);
  EXPECT_EQ(1u, observer_2.connection_ready_count_);

  // But the AddObserver call shouldn't affect other observer(s).
  EXPECT_EQ(1u, observer_1.connection_ready_count_);

  // Both observers should be notified when the instance is closed.
  holder.CloseInstance(&instance);
  EXPECT_EQ(1u, observer_1.connection_closed_count_);
  EXPECT_EQ(1u, observer_2.connection_closed_count_);

  holder.RemoveObserver(&observer_1);
  holder.RemoveObserver(&observer_2);
}

// Tests that the holder recognizes the instance's version.
TEST(ConnectionHolder, Version) {
  ConnectionHolder<FakeInstance> holder;
  FakeInstance instance;
  holder.SetInstance(&instance);
  EXPECT_EQ(FakeInstance::Version_, holder.instance_version());
}

// Tests that the GetInstance method works as intended.
TEST(ConnectionHolder, GetInstance) {
  ConnectionHolder<FakeInstance> holder;
  FakeInstance instance;
  holder.SetInstance(&instance);
  // Version 1 of the instance exists.
  EXPECT_EQ(&instance, holder.GetInstanceForVersionDoNotCallDirectly(
                           FakeInstance::Version_, "MethodName"));
  // Version 2 of the instance doesn't.
  EXPECT_EQ(nullptr, holder.GetInstanceForVersionDoNotCallDirectly(
                         FakeInstance::Version_ + 1, "MethodName"));
}

// TODO(khmel|team): Test SetHost() method too.

}  // namespace
}  // namespace arc
