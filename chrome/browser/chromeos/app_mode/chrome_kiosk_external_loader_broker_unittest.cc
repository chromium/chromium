// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"

#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using Observer = base::test::RepeatingTestFuture<base::Value::Dict>;
using crosapi::mojom::AppInstallParams;

base::Value::Dict SecondaryAppData() {
  return base::Value::Dict()
      .Set("external_update_url", extension_urls::GetWebstoreUpdateUrl().spec())
      .Set("is_from_webstore", true);
}

}  // namespace

class ChromeKioskExternalLoaderBrokerTest : public ::testing::Test {
 public:
  ChromeKioskExternalLoaderBrokerTest() = default;
  ChromeKioskExternalLoaderBrokerTest(
      const ChromeKioskExternalLoaderBrokerTest&) = delete;
  ChromeKioskExternalLoaderBrokerTest& operator=(
      const ChromeKioskExternalLoaderBrokerTest&) = delete;
  ~ChromeKioskExternalLoaderBrokerTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment environment_;
};

TEST_F(ChromeKioskExternalLoaderBrokerTest,
       ShouldNotInvokePrimaryObserverBeforeAppIsInstalled) {
  ChromeKioskExternalLoaderBroker broker;

  Observer primary_observer;
  broker.RegisterPrimaryAppInstallDataObserver(primary_observer.GetCallback());

  EXPECT_TRUE(primary_observer.IsEmpty());
}

TEST_F(ChromeKioskExternalLoaderBrokerTest,
       ShouldInvokePrimaryObserverWhenAppIsInstalled) {
  ChromeKioskExternalLoaderBroker broker;

  Observer primary_observer;
  broker.RegisterPrimaryAppInstallDataObserver(primary_observer.GetCallback());

  broker.TriggerPrimaryAppInstall(AppInstallParams(
      /*id=*/"the-app-id",
      /*crx_file_location=*/"the-app-location",
      /*version=*/"the-app-version",
      /*is_store_app=*/false));

  EXPECT_EQ(
      primary_observer.Take(),
      base::Value::Dict()  //
          .Set("the-app-id", base::Value::Dict()
                                 .Set("external_crx", "the-app-location")
                                 .Set("external_version", "the-app-version")
                                 .Set("is_from_webstore", false)));
}

TEST_F(ChromeKioskExternalLoaderBrokerTest,
       ShouldMarkPrimaryStoreAppAsFromWebstore) {
  const bool kIsStoreApp = true;

  ChromeKioskExternalLoaderBroker broker;

  Observer primary_observer;
  broker.RegisterPrimaryAppInstallDataObserver(primary_observer.GetCallback());

  broker.TriggerPrimaryAppInstall(AppInstallParams(
      /*id=*/"app-id",
      /*crx_file_location=*/"location",
      /*version=*/"version",
      /*is_store_app=*/kIsStoreApp));

  EXPECT_EQ(primary_observer.Take(),
            base::Value::Dict()  //
                .Set("app-id", base::Value::Dict()
                                   .Set("external_crx", "location")
                                   .Set("external_version", "version")
                                   .Set("is_from_webstore", true)));
}

TEST_F(ChromeKioskExternalLoaderBrokerTest,
       ShouldInvokePrimaryObserverEvenWhenAppWasAlreadyInstalled) {
  ChromeKioskExternalLoaderBroker broker;

  broker.TriggerPrimaryAppInstall(AppInstallParams(
      /*id=*/"a-very-long-app-id-to-cause-crashes-if-used-after-delete",
      /*crx_file_location=*/"the-app-location",
      /*version=*/"the-app-version",
      /*is_store_app=*/false));

  // Install the observer after the primary app was installed.
  Observer primary_observer;
  broker.RegisterPrimaryAppInstallDataObserver(primary_observer.GetCallback());

  EXPECT_EQ(primary_observer.Take(),
            base::Value::Dict()  //
                .Set("a-very-long-app-id-to-cause-crashes-if-used-after-delete",
                     base::Value::Dict()
                         .Set("external_crx", "the-app-location")
                         .Set("external_version", "the-app-version")
                         .Set("is_from_webstore", false)));
}

TEST_F(ChromeKioskExternalLoaderBrokerTest,
       ShouldNotInvokeSecondaryObserverBeforeAppIsInstalled) {
  ChromeKioskExternalLoaderBroker broker;

  Observer secondary_observer;
  broker.RegisterSecondaryAppInstallDataObserver(
      secondary_observer.GetCallback());

  EXPECT_TRUE(secondary_observer.IsEmpty());
}

TEST_F(ChromeKioskExternalLoaderBrokerTest,
       ShouldInvokeSecondaryObserverWhenAppsAreIsInstalled) {
  ChromeKioskExternalLoaderBroker broker;

  Observer secondary_observer;
  broker.RegisterSecondaryAppInstallDataObserver(
      secondary_observer.GetCallback());

  broker.TriggerSecondaryAppInstall({"secondary-app", "other-secondary-app"});

  EXPECT_EQ(secondary_observer.Take(),
            base::Value::Dict()  //
                .Set("secondary-app", SecondaryAppData())
                .Set("other-secondary-app", SecondaryAppData()));
}

TEST_F(ChromeKioskExternalLoaderBrokerTest,
       ShouldInvokeSecondaryObserverWhenAppsWereAlreadyInstalled) {
  ChromeKioskExternalLoaderBroker broker;

  broker.TriggerSecondaryAppInstall({"secondary-app"});

  // Install the observer after the secondary app was installed.
  Observer secondary_observer;
  broker.RegisterSecondaryAppInstallDataObserver(
      secondary_observer.GetCallback());

  EXPECT_EQ(secondary_observer.Take(),
            base::Value::Dict().Set("secondary-app", SecondaryAppData()));
}

}  // namespace chromeos
