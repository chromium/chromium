// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace guest_os {

namespace {
class GuestOsDlcInstallationTest : public ::testing::Test,
                                   public FakeDlcserviceHelper {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};
}  // namespace

TEST_F(GuestOsDlcInstallationTest, Success) {
  base::test::TestFuture<GuestOsDlcInstallation::Result> result;

  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);
  GuestOsDlcInstallation installation("test-dlc", /*retry=*/false,
                                      result.GetCallback(), base::DoNothing());

  EXPECT_TRUE(result.Get().has_value());
}

TEST_F(GuestOsDlcInstallationTest, RetryOnBusyInternal) {
  base::test::TestFuture<GuestOsDlcInstallation::Result> result;

  FakeDlcserviceClient()->set_install_errors({dlcservice::kErrorInternal,
                                              dlcservice::kErrorBusy,
                                              dlcservice::kErrorNone});
  GuestOsDlcInstallation installation("test-dlc", /*retry=*/true,
                                      result.GetCallback(), base::DoNothing());

  // After some time we're still retrying.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(result.IsReady());

  // Eventually it succeeds
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(result.Get().has_value());
}

TEST_F(GuestOsDlcInstallationTest, GivesUpAfterMaxRetries) {
  base::test::TestFuture<GuestOsDlcInstallation::Result> result;

  FakeDlcserviceClient()->set_install_errors(
      {dlcservice::kErrorBusy, dlcservice::kErrorBusy, dlcservice::kErrorBusy,
       dlcservice::kErrorBusy, dlcservice::kErrorBusy, dlcservice::kErrorBusy,
       dlcservice::kErrorBusy, dlcservice::kErrorNone});
  GuestOsDlcInstallation installation("test-dlc", /*retry=*/true,
                                      result.GetCallback(), base::DoNothing());

  task_environment_.FastForwardBy(base::Seconds(99));
  EXPECT_FALSE(result.Get().has_value());
}

TEST_F(GuestOsDlcInstallationTest, NoRetryAllowed) {
  base::test::TestFuture<GuestOsDlcInstallation::Result> result;

  FakeDlcserviceClient()->set_install_errors({dlcservice::kErrorInternal,
                                              dlcservice::kErrorBusy,
                                              dlcservice::kErrorNone});
  GuestOsDlcInstallation installation("test-dlc", /*retry=*/false,
                                      result.GetCallback(), base::DoNothing());

  EXPECT_FALSE(result.Get().has_value());
  EXPECT_EQ(result.Get().error(), GuestOsDlcInstallation::Error::Internal);
}

TEST_F(GuestOsDlcInstallationTest, Cancel) {
  base::test::TestFuture<GuestOsDlcInstallation::Result> result;

  auto installation = std::make_unique<GuestOsDlcInstallation>(
      "test-dlc", /*retry=*/false, result.GetCallback(), base::DoNothing());
  EXPECT_FALSE(result.IsReady());

  installation.reset();

  // Cancelling is synchronous.
  EXPECT_TRUE(result.IsReady());
  EXPECT_FALSE(result.Get().has_value());
  EXPECT_EQ(result.Get().error(), GuestOsDlcInstallation::Error::Cancelled);
}

TEST_F(GuestOsDlcInstallationTest, Offline) {
  ASSERT_TRUE(network::TestNetworkConnectionTracker::HasInstance());
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  base::test::TestFuture<GuestOsDlcInstallation::Result> result;

  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorInternal);
  GuestOsDlcInstallation installation("test-dlc", /*retry=*/false,
                                      result.GetCallback(), base::DoNothing());

  EXPECT_FALSE(result.Get().has_value());
  EXPECT_EQ(result.Get().error(), GuestOsDlcInstallation::Error::Offline);
}

}  // namespace guest_os
