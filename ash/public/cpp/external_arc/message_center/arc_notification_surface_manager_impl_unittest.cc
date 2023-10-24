// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager_impl.h"

#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kNotificationKey[] = "unit.test.notification";

class ArcNotificationSurfaceManagerImplTest : public exo::test::ExoTestBase {
 public:
  ArcNotificationSurfaceManagerImpl* manager() { return &manager_; }

 private:
  ArcNotificationSurfaceManagerImpl manager_;
};

class FakeObserver : public ArcNotificationSurfaceManager::Observer {
 public:
  void OnNotificationSurfaceAdded(ArcNotificationSurface* surface) override {
    added_call_count_++;
  }

  void OnNotificationSurfaceRemoved(ArcNotificationSurface* surface) override {
    removed_call_count_++;
  }

  int added_call_count_ = 0;
  int removed_call_count_ = 0;
};

}  // anonymous namespace

TEST_F(ArcNotificationSurfaceManagerImplTest, GetSurface) {
  auto surface = std::make_unique<exo::Surface>();
  auto notification_surface = std::make_unique<exo::NotificationSurface>(
      manager(), surface.get(), kNotificationKey);

  EXPECT_EQ(nullptr, manager()->GetSurface(kNotificationKey));

  manager()->AddSurface(notification_surface.get());
  EXPECT_EQ(notification_surface.get(),
            manager()->GetSurface(kNotificationKey));

  manager()->RemoveSurface(notification_surface.get());
  EXPECT_EQ(nullptr, manager()->GetSurface(kNotificationKey));
}

TEST_F(ArcNotificationSurfaceManagerImplTest, Observer) {
  FakeObserver observer;

  manager()->AddObserver(&observer);

  auto surface = std::make_unique<exo::Surface>();
  auto notification_surface = std::make_unique<exo::NotificationSurface>(
      manager(), surface.get(), kNotificationKey);
  manager()->AddSurface(notification_surface.get());
  EXPECT_EQ(1, observer.added_call_count_);
  EXPECT_EQ(0, observer.removed_call_count_);

  manager()->RemoveSurface(notification_surface.get());
  EXPECT_EQ(1, observer.added_call_count_);
  EXPECT_EQ(1, observer.removed_call_count_);
}

TEST_F(ArcNotificationSurfaceManagerImplTest,
       Observer_AddSurfaceWithTheSameKey) {
  FakeObserver observer;

  manager()->AddObserver(&observer);

  auto surface = std::make_unique<exo::Surface>();
  auto notification_surface = std::make_unique<exo::NotificationSurface>(
      manager(), surface.get(), kNotificationKey);
  manager()->AddSurface(notification_surface.get());
  EXPECT_EQ(1, observer.added_call_count_);
  EXPECT_EQ(0, observer.removed_call_count_);

  auto surface2 = std::make_unique<exo::Surface>();
  auto notification_surface2 = std::make_unique<exo::NotificationSurface>(
      manager(), surface2.get(), kNotificationKey);
  manager()->AddSurface(notification_surface2.get());

  EXPECT_EQ(2, observer.added_call_count_);
  EXPECT_EQ(1, observer.removed_call_count_);

  manager()->RemoveSurface(notification_surface2.get());
}

}  // namespace ash
