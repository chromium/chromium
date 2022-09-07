// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/connection_notifier.h"

#include "ash/components/arc/session/connection_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

struct FakeInstance {};

struct Observer : public ConnectionObserver<FakeInstance> {
  void OnConnectionReady() override { ++connection_ready_count_; }
  void OnConnectionClosed() override { ++connection_closed_count_; }

  size_t connection_ready_count_ = 0;
  size_t connection_closed_count_ = 0;
};

// Tests that NotifyConnectionReady works as intended.
TEST(ConnectionNotifier, NotifyConnectionReady) {
  Observer observer_1;
  Observer observer_2;
  internal::ConnectionNotifier notifier;

  notifier.AddObserver(&observer_1);
  // Notify the observer and check the results.
  notifier.NotifyConnectionReady();
  EXPECT_EQ(1u, observer_1.connection_ready_count_);
  EXPECT_EQ(0u, observer_2.connection_ready_count_);

  notifier.AddObserver(&observer_2);
  // Notify the observers and check the results.
  notifier.NotifyConnectionReady();
  EXPECT_EQ(2u, observer_1.connection_ready_count_);
  EXPECT_EQ(1u, observer_2.connection_ready_count_);

  notifier.RemoveObserver(&observer_1);
  // Notify the observer and check the results.
  notifier.NotifyConnectionReady();
  EXPECT_EQ(2u, observer_1.connection_ready_count_);
  EXPECT_EQ(2u, observer_2.connection_ready_count_);

  notifier.RemoveObserver(&observer_2);
  // Notify and check that this is no-op.
  notifier.NotifyConnectionReady();
  EXPECT_EQ(2u, observer_1.connection_ready_count_);
  EXPECT_EQ(2u, observer_2.connection_ready_count_);
}

// Tests NotifyConnectionClosed in the same way.
TEST(ConnectionNotifier, NotifyConnectionClosed) {
  Observer observer_1;
  Observer observer_2;
  internal::ConnectionNotifier notifier;

  notifier.AddObserver(&observer_1);
  // Notify the observer and check the results.
  notifier.NotifyConnectionClosed();
  EXPECT_EQ(1u, observer_1.connection_closed_count_);
  EXPECT_EQ(0u, observer_2.connection_closed_count_);

  notifier.AddObserver(&observer_2);
  // Notify the observers and check the results.
  notifier.NotifyConnectionClosed();
  EXPECT_EQ(2u, observer_1.connection_closed_count_);
  EXPECT_EQ(1u, observer_2.connection_closed_count_);

  notifier.RemoveObserver(&observer_1);
  // Notify the observer and check the results.
  notifier.NotifyConnectionClosed();
  EXPECT_EQ(2u, observer_1.connection_closed_count_);
  EXPECT_EQ(2u, observer_2.connection_closed_count_);

  notifier.RemoveObserver(&observer_2);
  // Notify and check that this is no-op.
  notifier.NotifyConnectionClosed();
  EXPECT_EQ(2u, observer_1.connection_closed_count_);
  EXPECT_EQ(2u, observer_2.connection_closed_count_);
}

}  // namespace
}  // namespace arc
