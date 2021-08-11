// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"

#include <memory>

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/mock_http_fetcher.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kValidResponse[] = R"(
    {
    "device": {
      "id": "123",
      "notificationType": "FAST_PAIR",
      "imageUrl": "https://lh3.googleusercontent.com/wK3v3qR5wzG2NImpnKU2bZ_nQiv8xzRhT1ZudaOCaK9NW4UKyY5kobSkHyqyBYO5N3XwRo8_4DFGFpq-R3Vmng",
      "name": "Headphone Buds",
      "intentUri": "intent:#Intent;action=com.google.android.gms.nearby.discovery%3AACTION_MAGIC_PAIR;package=com.google.android.gms;component=com.google.android.gms/.nearby.discovery.service.DiscoveryService;S.com.google.android.gms.nearby.discovery%3AEXTRA_COMPANION_APP=com.google.android.apps.wearables.maestro.companion;end",
      "triggerDistance": 0.6,
      "antiSpoofingKeyPair": {
        "publicKey": "KmmpnHD/WpMm4wFc87zHkYVQCYxzqyp8inrdfy/R9yZUEHaRQ9sNZXJT8eQfSNvpo71PSB7Ftl5bJ+s9Ph4HUw=="
      },
      "status": {
        "statusType": "PUBLISHED"
      },
      "lastUpdateTimestamp": "2019-11-25T22:02:04.476Z",
      "deviceType": "TRUE_WIRELESS_HEADPHONES",
      "trueWirelessImages": {
        "leftBudUrl": "https://lh3.googleusercontent.com/O9J8WGRC89CO58ASjaFmlcmS55f-1bIf2oQaWW1rKdD06mFZSKQ7EiMOrJIjcEKQGDS-IpRcvNwaPhoYp5aQng",
        "rightBudUrl": "https://lh3.googleusercontent.com/YXyi8W-UBDJbIrIN4rxTAycGgRoiWMcnPZlVrQ1MLLyuZ2DmPHtbsD23bmey7ImyGejRmdvafHKKsmuzfnfT",
        "caseUrl": "https://lh3.googleusercontent.com/hWygI3ibN4HWD-FjwvSnWPHnGs1AM15KrXvUlzNj6JRC2jM26nLVccKX41QdKf8q7hSvyRLb6LqRBD2VU9PZCPc"
      },
      "assistantSupported": true,
      "companyName": "Google",
      "displayName": "Black Headphone Buds",
      "interactionType": "NOTIFICATION",
      "companionDetail": {}
    },
    "image": "iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAABHNCSVQICAgIfAhkiAAAAPhJREFUGJVlkKFuhUAURGdJg6irQrUKx0dgWoepQPQ7MFgcClPxJB/QpKGpa1JTR/UT1QUSsgoIiM2yOzWP5pVOcjPintyZXIEzkbzOsuxjmqYrkqYoijchxAP2qqrqBQCDIODmJB8B4GKDkiR5zfM8iuMYruvC930YY36PiFPkfZqmz8YYkAQAhGGIKIreHce5Pe92sNZSa822bVnXNfu+J8m7jdmirVIK67pCKQWtNZRSAHC5gc7JV5LYxhgDrTUA6D34BQDWWizLgmVZMM8zABz/gEKIwzAMkFKi6zqM4wjP845CiO9/PyR5I6X8LMtyaprmab//AaFEknGJ38vvAAAAAElFTkSuQmCC",
    "strings": {
      "initialNotificationDescription": "Tap to pair. Earbuds will be tied to %s",
      "initialNotificationDescriptionNoAccount": "Tap to pair with this device",
      "openCompanionAppDescription": "Tap to finish setup",
      "updateCompanionAppDescription": "Tap to update device settings and finish setup",
      "downloadCompanionAppDescription": "Tap to download device app on Google Play and see all features",
      "unableToConnectTitle": "Unable to connect",
      "unableToConnectDescription": "Try manually pairing to the device",
      "initialPairingDescription": "%s will appear on devices linked with %s",
      "connectSuccessCompanionAppInstalled": "Your device is ready to be set up",
      "connectSuccessCompanionAppNotInstalled": "Download the device app on Google Play to see all available features",
      "subsequentPairingDescription": "Connect %s to this phone",
      "retroactivePairingDescription": "Save device to %s for faster pairing to your other devices",
      "waitLaunchCompanionAppDescription": "This will take a few moments",
      "failConnectGoToSettingsDescription": "Try manually pairing to the device by going to Settings",
      "assistantSetupHalfSheet": "Get the hands-free help on the go from Google Assistant",
      "assistantSetupNotification": "Tap to set up your Google Assistant"
    }
  }
)";
constexpr char kEmptyResponse[] = "{}";
constexpr char kInvalidResponse[] = "<html>404 error</html>";
constexpr char kHexDeviceId[] = "07B";
const int kDeviceId = 123;

}  // namespace

namespace ash {
namespace quick_pair {

class DeviceMetadataFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    auto mock_http_fetcher = std::make_unique<MockHttpFetcher>();
    mock_http_fetcher_ = mock_http_fetcher.get();
    device_metadata_fetcher_ =
        std::make_unique<DeviceMetadataFetcher>(std::move(mock_http_fetcher));
  }

 protected:
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<DeviceMetadataFetcher> device_metadata_fetcher_;
  MockHttpFetcher* mock_http_fetcher_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(DeviceMetadataFetcherTest, ValidResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::move(callback).Run(std::make_unique<std::string>(kValidResponse));
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce([](absl::optional<nearby::fastpair::GetObservedDeviceResponse>
                       response) {
        ASSERT_EQ("Headphone Buds", response->device().name());
        ASSERT_EQ(
            "https://lh3.googleusercontent.com/"
            "wK3v3qR5wzG2NImpnKU2bZ_"
            "nQiv8xzRhT1ZudaOCaK9NW4UKyY5kobSkHyqyBYO5N3XwRo8_4DFGFpq-R3Vmng",
            response->device().image_url());
        ASSERT_EQ(0.6f, response->device().trigger_distance());
        ASSERT_FALSE(
            response->device().anti_spoofing_key_pair().public_key().empty());
        ASSERT_FALSE(
            response->device().true_wireless_images().left_bud_url().empty());
        ASSERT_FALSE(
            response->device().true_wireless_images().right_bud_url().empty());
        ASSERT_FALSE(
            response->device().true_wireless_images().case_url().empty());
      });

  device_metadata_fetcher_->LookupHexDeviceId(kHexDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(DeviceMetadataFetcherTest, InvalidResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::move(callback).Run(
            std::make_unique<std::string>(kInvalidResponse));
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce([](absl::optional<nearby::fastpair::GetObservedDeviceResponse>
                       response) { ASSERT_EQ(absl::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(DeviceMetadataFetcherTest, EmptyResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::move(callback).Run(std::make_unique<std::string>(kEmptyResponse));
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce([](absl::optional<nearby::fastpair::GetObservedDeviceResponse>
                       response) { ASSERT_EQ(absl::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(DeviceMetadataFetcherTest, NoResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::move(callback).Run(nullptr);
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce([](absl::optional<nearby::fastpair::GetObservedDeviceResponse>
                       response) { ASSERT_EQ(absl::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
