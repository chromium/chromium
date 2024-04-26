// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"
#include "url/gurl.h"

using testing::NiceMock;

namespace {
const char kDefaultTestUrl[] = "https://www.google.com/";
}  //  namespace

class PrivateNetworkDeviceChooserControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PrivateNetworkDeviceChooserControllerTest() = default;
  PrivateNetworkDeviceChooserControllerTest(
      const PrivateNetworkDeviceChooserControllerTest&) = delete;
  PrivateNetworkDeviceChooserControllerTest& operator=(
      const PrivateNetworkDeviceChooserControllerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

    private_network_device_chooser_controller_ =
        std::make_unique<PrivateNetworkDeviceChooserController>(
            main_rfh(), blink::mojom::PrivateNetworkDevice::New(),
            PrivateNetworkDeviceChooserController::DoneCallback());
    mock_chooser_view_ =
        std::make_unique<NiceMock<permissions::MockChooserControllerView>>();
    private_network_device_chooser_controller_->set_view(
        mock_chooser_view_.get());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void CreateAndAddFakePrivateNetworkDevice(const std::string& id,
                                            const std::string& name,
                                            const net::IPAddress& ip_address) {
    private_network_device_chooser_controller_->ReplaceDeviceForTesting(
        blink::mojom::PrivateNetworkDevice::New(id, name, ip_address));
  }

  std::unique_ptr<PrivateNetworkDeviceChooserController>
      private_network_device_chooser_controller_;
  std::unique_ptr<permissions::MockChooserControllerView> mock_chooser_view_;
};

// The new added device will overwrite the device list because PNA chooser only
// have one device at a time.
TEST_F(PrivateNetworkDeviceChooserControllerTest, AddDevice) {
  EXPECT_CALL(*mock_chooser_view_, OnOptionAdded(0)).Times(1);
  CreateAndAddFakePrivateNetworkDevice("a", "001",
                                       net::IPAddress(192, 168, 1, 1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, private_network_device_chooser_controller_->NumOptions());
  EXPECT_EQ(u"001 (a)",
            private_network_device_chooser_controller_->GetOption(0));

  EXPECT_CALL(*mock_chooser_view_, OnOptionAdded(0)).Times(1);
  CreateAndAddFakePrivateNetworkDevice("b", "002",
                                       net::IPAddress(192, 168, 0, 1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, private_network_device_chooser_controller_->NumOptions());
  EXPECT_EQ(u"002 (b)",
            private_network_device_chooser_controller_->GetOption(0));

  EXPECT_CALL(*mock_chooser_view_, OnOptionAdded(0)).Times(1);
  CreateAndAddFakePrivateNetworkDevice("c", "003",
                                       net::IPAddress(127, 0, 0, 1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, private_network_device_chooser_controller_->NumOptions());
  EXPECT_EQ(u"003 (c)",
            private_network_device_chooser_controller_->GetOption(0));
}

// TODO(crbug.com/40272624): add test for Select(), Close() and Cancel().
