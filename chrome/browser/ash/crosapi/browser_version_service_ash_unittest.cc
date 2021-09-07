// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/component_updater/mock_component_updater_service.h"
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
  std::unique_ptr<MockComponentUpdateService> mock_component_update_service(
      new ::testing::NiceMock<MockComponentUpdateService>());
  EXPECT_CALL(*mock_component_update_service, AddObserver(_)).Times(1);

  std::string sample_browser_component_id =
      browser_util::kLacrosDogfoodDevInfo.crx_id;
  std::string sample_browser_version_str = "95.0.0.0";
  std::vector<ComponentInfo> sample_components;
  sample_components.push_back(
      ComponentInfo(sample_browser_component_id, "",
                    base::UTF8ToUTF16(browser_util::kLacrosDogfoodDevInfo.name),
                    base::Version(sample_browser_version_str)));
  ON_CALL(*mock_component_update_service, GetComponents())
      .WillByDefault(Return(sample_components));

  EXPECT_CALL(browser_version_observer_,
              OnBrowserVersionInstalled(sample_browser_version_str))
      .Times(2);

  std::unique_ptr<BrowserVersionServiceAsh> browser_version_service =
      std::make_unique<BrowserVersionServiceAsh>(
          mock_component_update_service.get());

  base::RunLoop run_loop;
  browser_version_service->AddBrowserVersionObserver(
      browser_version_observer_.BindAndGetRemote());

  static_cast<component_updater::ComponentUpdateService::Observer*>(
      browser_version_service.get())
      ->OnEvent(
          update_client::UpdateClient::Observer::Events::COMPONENT_UPDATED,
          sample_browser_component_id);
  run_loop.RunUntilIdle();
}

}  // namespace crosapi
