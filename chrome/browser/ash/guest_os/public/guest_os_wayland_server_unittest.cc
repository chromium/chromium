// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/vm_wl/wl.pb.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/server/wayland_server_controller.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wayland/test/wayland_server_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using borealis::NiceCallbackFactory;
using testing::_;
using testing::Invoke;
using testing::IsFalse;

namespace guest_os {

namespace {

class GuestOsWaylandServerTest : public ChromeAshTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(root_dir.CreateUniqueTempDir());
    setenv("XDG_RUNTIME_DIR",
           root_dir.GetPath().Append("xdg").MaybeAsASCII().c_str(),
           /*overwrite=*/1);
    ChromeAshTestBase::SetUp();
  }

 protected:
  TestingProfile profile_;

 private:
  base::ScopedTempDir root_dir;
};

}  // namespace

TEST_F(GuestOsWaylandServerTest, BadSocketCausesFailure) {
  auto wsc = std::make_unique<exo::WaylandServerController>(
      nullptr, nullptr, nullptr, nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);

  base::test::TestFuture<std::optional<std::string>> result_future;
  gows.Listen({}, vm_tools::apps::UNKNOWN, "test", result_future.GetCallback());
  EXPECT_TRUE(result_future.Get().has_value());
}

TEST_F(GuestOsWaylandServerTest, NullDelegateCausesFailure) {
  auto wsc = std::make_unique<exo::WaylandServerController>(
      nullptr, nullptr, nullptr, nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);
  exo::wayland::test::WaylandServerTestBase::ScopedTempSocket socket;

  // This test relies on the borealis security delegate giving us nullptr
  // when borealis is disallowed.
  base::test::TestFuture<borealis::BorealisFeatures::AllowStatus>
      allowedness_future;
  borealis::BorealisServiceFactory::GetForProfile(&profile_)
      ->Features()
      .IsAllowed(allowedness_future.GetCallback());
  ASSERT_NE(allowedness_future.Get(),
            borealis::BorealisFeatures::AllowStatus::kAllowed);

  base::test::TestFuture<std::optional<std::string>> result_future;
  gows.Listen(socket.TakeFd(), vm_tools::apps::BOREALIS, "borealis",
              result_future.GetCallback());
  EXPECT_TRUE(result_future.Get().has_value());
}

TEST_F(GuestOsWaylandServerTest, DelegateLifetimeManagedCorrectly) {
  auto wsc = std::make_unique<exo::WaylandServerController>(
      nullptr, nullptr, nullptr, nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);
  exo::wayland::test::WaylandServerTestBase::ScopedTempSocket socket;

  // Initially the server doesn't exist.
  EXPECT_EQ(gows.GetDelegate(vm_tools::apps::UNKNOWN, "test"), nullptr);

  base::test::TestFuture<std::optional<std::string>> listen_future;
  gows.Listen(socket.TakeFd(), vm_tools::apps::UNKNOWN, "test",
              listen_future.GetCallback());
  EXPECT_FALSE(listen_future.Get().has_value());

  // Now the server is valid.
  base::WeakPtr<GuestOsSecurityDelegate> delegate =
      gows.GetDelegate(vm_tools::apps::UNKNOWN, "test");
  EXPECT_TRUE(delegate.MaybeValid());
  EXPECT_NE(delegate.get(), nullptr);

  base::test::TestFuture<std::optional<std::string>> close_future;
  gows.Close(vm_tools::apps::UNKNOWN, "test", close_future.GetCallback());
  EXPECT_FALSE(close_future.Get().has_value());

  EXPECT_EQ(gows.GetDelegate(vm_tools::apps::UNKNOWN, "test"), nullptr);
  EXPECT_TRUE(delegate.WasInvalidated());
}

TEST_F(GuestOsWaylandServerTest, EvictServersOnConciergeCrash) {
  ash::ConciergeClient::InitializeFake();
  auto* concierge_client = ash::FakeConciergeClient::Get();
  auto wsc = std::make_unique<exo::WaylandServerController>(
      nullptr, nullptr, nullptr, nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);

  exo::wayland::test::WaylandServerTestBase::ScopedTempSocket socket;
  base::test::TestFuture<std::optional<std::string>> listen_future;
  gows.Listen(socket.TakeFd(), vm_tools::apps::UNKNOWN, "test",
              listen_future.GetCallback());
  ASSERT_FALSE(listen_future.Get().has_value());

  EXPECT_NE(gows.GetDelegate(vm_tools::apps::UNKNOWN, "test"), nullptr);
  concierge_client->NotifyConciergeStopped();
  EXPECT_EQ(gows.GetDelegate(vm_tools::apps::UNKNOWN, "test"), nullptr);
}

}  // namespace guest_os
