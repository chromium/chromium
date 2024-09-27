// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chromeos/ash/components/standalone_browser/channel_util.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using component_updater::ComponentInfo;
using component_updater::MockComponentUpdateService;
using testing::_;
using ::testing::Return;

namespace crosapi {

class MockBrowserVersionObserver
    : public crosapi::mojom::BrowserVersionObserver {
 public:
  MockBrowserVersionObserver() = default;
  ~MockBrowserVersionObserver() override = default;

  mojo::PendingRemote<crosapi::mojom::BrowserVersionObserver>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<crosapi::mojom::BrowserVersionObserver> receiver_{this};

  MOCK_METHOD1(OnBrowserVersionInstalled, void(const std::string& version));
};

class MockVersionServiceDelegate : public BrowserVersionServiceAsh::Delegate {
 public:
  MockVersionServiceDelegate() = default;
  MockVersionServiceDelegate(const MockVersionServiceDelegate&) = delete;
  MockVersionServiceDelegate& operator=(const MockVersionServiceDelegate&) =
      delete;
  ~MockVersionServiceDelegate() override = default;

  // BrowserVersionServiceAsh::Delegate:
  base::Version GetLatestLaunchableBrowserVersion() const override {
    return base::Version("95.0.0.0");
  }

  bool IsNewerBrowserAvailable() const override { return true; }
};

class BrowserVersionServiceAshTest : public testing::Test {
 public:
  BrowserVersionServiceAshTest() = default;
  ~BrowserVersionServiceAshTest() override = default;

 protected:
  testing::StrictMock<MockBrowserVersionObserver> browser_version_observer_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(BrowserVersionServiceAshTest,
       NotifiesObserverOnInstalledBrowserVersionUpdates) {
  ::testing::NiceMock<MockComponentUpdateService> mock_component_update_service;
  EXPECT_CALL(mock_component_update_service, AddObserver(_)).Times(1);

  std::string sample_browser_component_id =
      ash::standalone_browser::kLacrosDogfoodDevInfo.crx_id;
  std::string sample_browser_version_str = "95.0.0.0";
  std::vector<ComponentInfo> sample_components;
  sample_components.emplace_back(
      sample_browser_component_id, "",
      base::UTF8ToUTF16(ash::standalone_browser::kLacrosDogfoodDevInfo.name),
      base::Version(sample_browser_version_str), "");
  ON_CALL(mock_component_update_service, GetComponents())
      .WillByDefault(Return(sample_components));

  EXPECT_CALL(browser_version_observer_,
              OnBrowserVersionInstalled(sample_browser_version_str))
      .Times(2);

  base::RunLoop run_loop;
  MockVersionServiceDelegate delegate;
  BrowserVersionServiceAsh browser_version_service(
      &mock_component_update_service);
  browser_version_service.set_delegate_for_testing(&delegate);
  browser_version_service.AddBrowserVersionObserver(
      browser_version_observer_.BindAndGetRemote());

  update_client::CrxUpdateItem item;
  item.id = sample_browser_component_id;
  item.state = update_client::ComponentState::kUpdated;
  static_cast<component_updater::ComponentUpdateService::Observer*>(
      &browser_version_service)
      ->OnEvent(item);
  run_loop.RunUntilIdle();
}

TEST_F(BrowserVersionServiceAshTest, GetInstalledBrowserVersion) {
  ::testing::NiceMock<MockComponentUpdateService> mock_component_update_service;
  std::string sample_browser_component_id =
      ash::standalone_browser::kLacrosDogfoodDevInfo.crx_id;
  std::string sample_browser_version_str = "95.0.0.0";
  std::vector<ComponentInfo> sample_components;
  sample_components.emplace_back(
      sample_browser_component_id, "",
      base::UTF8ToUTF16(ash::standalone_browser::kLacrosDogfoodDevInfo.name),
      base::Version(sample_browser_version_str), "");
  ON_CALL(mock_component_update_service, GetComponents())
      .WillByDefault(Return(sample_components));

  MockVersionServiceDelegate delegate;
  BrowserVersionServiceAsh browser_version_service(
      &mock_component_update_service);
  browser_version_service.set_delegate_for_testing(&delegate);

  base::MockCallback<
      mojom::BrowserVersionService::GetInstalledBrowserVersionCallback>
      callback;
  EXPECT_CALL(callback, Run(sample_browser_version_str));
  browser_version_service.GetInstalledBrowserVersion(callback.Get());
}

}  // namespace crosapi
