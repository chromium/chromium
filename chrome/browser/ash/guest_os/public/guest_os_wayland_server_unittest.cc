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
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
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

TEST_F(GuestOsWaylandServerTest, UnknownVmTypeNotSupported) {
  NiceCallbackFactory<void(GuestOsWaylandServer::Result)> result_factory;

  GuestOsWaylandServer gows(&profile_);

  EXPECT_CALL(result_factory, Call(IsFalse()));

  gows.Get(vm_tools::launch::UNKNOWN, result_factory.BindOnce());
}

TEST_F(GuestOsWaylandServerTest, NullSecurityDelegatePreventsBuild) {
  NiceCallbackFactory<void(
      base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>)>
      capability_factory;
  NiceCallbackFactory<void(GuestOsWaylandServer::Result)> result_factory;

  GuestOsWaylandServer gows(&profile_);
  gows.SetCapabilityFactoryForTesting(vm_tools::launch::UNKNOWN,
                                      capability_factory.BindRepeating());

  EXPECT_CALL(capability_factory, Call(_))
      .WillOnce(Invoke(
          [](base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>
                 callback) { std::move(callback).Run(nullptr); }));
  EXPECT_CALL(result_factory, Call(IsFalse()));

  gows.Get(vm_tools::launch::UNKNOWN, result_factory.BindOnce());
}

TEST_F(GuestOsWaylandServerTest, SuccessfulResultIsReused) {
  NiceCallbackFactory<void(
      base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>)>
      capability_factory;
  NiceCallbackFactory<void(GuestOsWaylandServer::Result)> result_factory;

  exo::WaylandServerController wsc(nullptr, nullptr, nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);
  gows.SetCapabilityFactoryForTesting(vm_tools::launch::UNKNOWN,
                                      capability_factory.BindRepeating());

  EXPECT_CALL(capability_factory, Call(_))
      .Times(1)
      .WillOnce(Invoke(
          [](base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>
                 callback) {
            std::move(callback).Run(
                std::make_unique<GuestOsSecurityDelegate>());
          }));
  base::RunLoop loop;
  EXPECT_CALL(result_factory, Call(_))
      .Times(2)
      .WillRepeatedly(Invoke([&loop](GuestOsWaylandServer::Result result) {
        EXPECT_TRUE(result);
        EXPECT_NE(result.Value()->server_path(), base::FilePath{});
        loop.Quit();
      }));

  gows.Get(vm_tools::launch::UNKNOWN, result_factory.BindOnce());
  loop.Run();

  gows.Get(vm_tools::launch::UNKNOWN, result_factory.BindOnce());
}

TEST_F(GuestOsWaylandServerTest, InvalidatedOnceServerDestroyed) {
  NiceCallbackFactory<void(
      base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>)>
      capability_factory;
  NiceCallbackFactory<void(GuestOsWaylandServer::Result)> result_factory;

  auto wsc = std::make_unique<exo::WaylandServerController>(nullptr, nullptr,
                                                            nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);
  gows.SetCapabilityFactoryForTesting(vm_tools::launch::UNKNOWN,
                                      capability_factory.BindRepeating());
  GuestOsWaylandServer::ServerDetails* details;

  EXPECT_CALL(capability_factory, Call(_))
      .WillOnce(Invoke(
          [](base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>
                 callback) {
            std::move(callback).Run(
                std::make_unique<GuestOsSecurityDelegate>());
          }));
  base::RunLoop loop;
  EXPECT_CALL(result_factory, Call(_))
      .WillRepeatedly(
          Invoke([&loop, &details](GuestOsWaylandServer::Result result) {
            details = result.Value();
            loop.Quit();
          }));

  gows.Get(vm_tools::launch::UNKNOWN, result_factory.BindOnce());
  loop.Run();

  EXPECT_NE(details->security_delegate(), nullptr);
  wsc.reset();
  this->task_environment()->RunUntilIdle();
  EXPECT_EQ(details->security_delegate(), nullptr);
}

TEST_F(GuestOsWaylandServerTest, BadSocketCausesFailure) {
  auto wsc = std::make_unique<exo::WaylandServerController>(nullptr, nullptr,
                                                            nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);

  base::test::TestFuture<absl::optional<std::string>> result_future;
  gows.Listen({}, vm_tools::apps::UNKNOWN, "test", result_future.GetCallback());
  EXPECT_TRUE(result_future.Get().has_value());
}

TEST_F(GuestOsWaylandServerTest, NullDelegateCausesFailure) {
  auto wsc = std::make_unique<exo::WaylandServerController>(nullptr, nullptr,
                                                            nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);
  exo::wayland::test::WaylandServerTestBase::ScopedTempSocket socket;

  // This test relies on the borealis security delegate giving us nullptr
  // when borealis is disallowed.
  ASSERT_NE(borealis::BorealisService::GetForProfile(&profile_)
                ->Features()
                .MightBeAllowed(),
            borealis::BorealisFeatures::AllowStatus::kAllowed);

  base::test::TestFuture<absl::optional<std::string>> result_future;
  gows.Listen(socket.TakeFd(), vm_tools::apps::BOREALIS, "borealis",
              result_future.GetCallback());
  EXPECT_TRUE(result_future.Get().has_value());
}

TEST_F(GuestOsWaylandServerTest, DelegateLifetimeManagedCorrectly) {
  auto wsc = std::make_unique<exo::WaylandServerController>(nullptr, nullptr,
                                                            nullptr, nullptr);
  GuestOsWaylandServer gows(&profile_);
  exo::wayland::test::WaylandServerTestBase::ScopedTempSocket socket;

  // Initially the server doesn't exist.
  EXPECT_EQ(gows.GetDelegate(vm_tools::apps::UNKNOWN, "test"), nullptr);

  base::test::TestFuture<absl::optional<std::string>> listen_future;
  gows.Listen(socket.TakeFd(), vm_tools::apps::UNKNOWN, "test",
              listen_future.GetCallback());
  EXPECT_FALSE(listen_future.Get().has_value());

  // Now the server is valid.
  base::WeakPtr<GuestOsSecurityDelegate> delegate =
      gows.GetDelegate(vm_tools::apps::UNKNOWN, "test");
  EXPECT_TRUE(delegate.MaybeValid());
  EXPECT_NE(delegate.get(), nullptr);

  base::test::TestFuture<absl::optional<std::string>> close_future;
  gows.Close(vm_tools::apps::UNKNOWN, "test", close_future.GetCallback());
  EXPECT_FALSE(close_future.Get().has_value());

  EXPECT_EQ(gows.GetDelegate(vm_tools::apps::UNKNOWN, "test"), nullptr);
  EXPECT_TRUE(delegate.WasInvalidated());
}

}  // namespace guest_os
