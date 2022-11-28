// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"

#include <vector>

#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/guest_os/guest_os_test_helpers.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

using Id = GuestOsMountProviderRegistry::Id;

class MockMountObserver : public GuestOsMountProviderRegistry::Observer {
  using Id = GuestOsMountProviderRegistry::Id;

 public:
  std::vector<Id> ids_;
  void OnRegistered(Id id, GuestOsMountProvider* provider) override {
    DCHECK(provider);
    ids_.push_back(id);
  }
  void OnUnregistered(Id id) override {
    auto pos = base::ranges::find(ids_, id);
    if (pos != ids_.end()) {
      ids_.erase(pos);
    }
  }
};

class GuestOsMountProviderRegistryTest : public testing::Test {};

// Test that we can register, list and get providers
TEST_F(GuestOsMountProviderRegistryTest, TestGetting) {
  auto provider1 = std::make_unique<MockMountProvider>();
  auto* p1 = provider1.get();
  auto provider2 = std::make_unique<MockMountProvider>();
  auto* p2 = provider2.get();
  GuestOsMountProviderRegistry registry;
  auto id1 = registry.Register(std::move(provider1));
  auto id2 = registry.Register(std::move(provider2));

  std::vector<Id> expected = std::vector<Id>{0, 1};
  ASSERT_EQ(registry.List(), expected);

  ASSERT_EQ(p1, registry.Get(id1));
  ASSERT_EQ(p2, registry.Get(id2));
}

// Test that we can register, list and get providers
TEST_F(GuestOsMountProviderRegistryTest, TestGetNullPtrIfMissing) {
  GuestOsMountProviderRegistry registry;
  ASSERT_EQ(registry.Get(0), nullptr);
}

// Test register/unregister and observing/unobserving
TEST_F(GuestOsMountProviderRegistryTest, TestObservation) {
  auto provider1 = std::make_unique<MockMountProvider>();
  auto provider2 = std::make_unique<MockMountProvider>();
  GuestOsMountProviderRegistry registry;
  MockMountObserver obs;
  registry.AddObserver(&obs);
  registry.Register(std::move(provider1));
  registry.Register(std::move(provider2));
  std::vector<Id> expected = std::vector<Id>{0, 1};
  ASSERT_EQ(obs.ids_, expected);
  registry.Unregister(0);
  ASSERT_EQ(obs.ids_.size(), 1u);
  registry.Unregister(1);
  ASSERT_EQ(obs.ids_.size(), 0u);
}

}  // namespace guest_os
