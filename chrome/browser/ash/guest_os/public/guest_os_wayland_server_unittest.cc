// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"
#include <memory>

#include "chrome/browser/ash/borealis/borealis_security_delegate.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/server/wayland_server_controller.h"
#include "components/exo/toast_surface_manager.h"
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
          [this](base::OnceCallback<void(
                     std::unique_ptr<GuestOsSecurityDelegate>)> callback) {
            std::move(callback).Run(
                std::make_unique<borealis::BorealisSecurityDelegate>(
                    &profile_));
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
          [this](base::OnceCallback<void(
                     std::unique_ptr<GuestOsSecurityDelegate>)> callback) {
            std::move(callback).Run(
                std::make_unique<borealis::BorealisSecurityDelegate>(
                    &profile_));
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

}  // namespace guest_os
