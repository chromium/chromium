// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_settings.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_enums.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom-test-utils.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDefaultDeviceName[] = "Josh's Chromebook";

}  // namespace

using NearbyShareSettingsAsyncWaiter =
    nearby_share::mojom::NearbyShareSettingsAsyncWaiter;

class FakeNearbyShareSettingsObserver
    : public nearby_share::mojom::NearbyShareSettingsObserver {
 public:
  void OnEnabledChanged(bool enabled) override { this->enabled = enabled; }
  void OnDeviceNameChanged(const std::string& device_name) override {
    this->device_name = device_name;
  }
  void OnDataUsageChanged(nearby_share::mojom::DataUsage data_usage) override {
    this->data_usage = data_usage;
  }
  void OnVisibilityChanged(
      nearby_share::mojom::Visibility visibility) override {
    this->visibility = visibility;
  }
  void OnAllowedContactsChanged(
      const std::vector<std::string>& allowed_contacts) override {
    this->allowed_contacts = allowed_contacts;
  }

  bool enabled = false;
  std::string device_name = "uncalled";
  nearby_share::mojom::DataUsage data_usage =
      nearby_share::mojom::DataUsage::kUnknown;
  nearby_share::mojom::Visibility visibility =
      nearby_share::mojom::Visibility::kUnknown;
  std::vector<std::string> allowed_contacts;
  mojo::Receiver<nearby_share::mojom::NearbyShareSettingsObserver> receiver_{
      this};
};

class NearbyShareSettingsTest : public ::testing::Test {
 public:
  NearbyShareSettingsTest() : local_device_data_manager_(kDefaultDeviceName) {
    scoped_feature_list_.InitAndEnableFeature(features::kNearbySharing);

    RegisterNearbySharingPrefs(pref_service_.registry());

    nearby_share_settings_.AddSettingsObserver(
        observer_.receiver_.BindNewPipeAndPassRemote());
  }
  ~NearbyShareSettingsTest() override = default;

  void FlushMojoMessages() { observer_.receiver_.FlushForTesting(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager_;
  FakeNearbyShareSettingsObserver observer_;
  NearbyShareSettings nearby_share_settings_{&pref_service_,
                                             &local_device_data_manager_};
  NearbyShareSettingsAsyncWaiter nearby_share_settings_waiter_{
      &nearby_share_settings_};
};

TEST_F(NearbyShareSettingsTest, GetAndSetEnabled) {
  EXPECT_EQ(false, observer_.enabled);
  nearby_share_settings_.SetEnabled(true);
  EXPECT_EQ(true, nearby_share_settings_.GetEnabled());
  FlushMojoMessages();
  EXPECT_EQ(true, observer_.enabled);

  bool enabled = false;
  nearby_share_settings_waiter_.GetEnabled(&enabled);
  EXPECT_EQ(true, enabled);

  nearby_share_settings_.SetEnabled(false);
  EXPECT_EQ(false, nearby_share_settings_.GetEnabled());
  FlushMojoMessages();
  EXPECT_EQ(false, observer_.enabled);

  nearby_share_settings_waiter_.GetEnabled(&enabled);
  EXPECT_EQ(false, enabled);

  // Verify that setting the value to false again value doesn't trigger an
  // observer event.
  observer_.enabled = true;
  nearby_share_settings_.SetEnabled(false);
  EXPECT_EQ(false, nearby_share_settings_.GetEnabled());
  FlushMojoMessages();
  // the observers's value should not have been updated.
  EXPECT_EQ(true, observer_.enabled);
}

TEST_F(NearbyShareSettingsTest, ValidateDeviceName) {
  auto result = nearby_share::mojom::DeviceNameValidationResult::kValid;
  local_device_data_manager_.set_next_validation_result(
      nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty);
  nearby_share_settings_waiter_.ValidateDeviceName("", &result);
  EXPECT_EQ(result,
            nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty);

  local_device_data_manager_.set_next_validation_result(
      nearby_share::mojom::DeviceNameValidationResult::kValid);
  nearby_share_settings_waiter_.ValidateDeviceName(
      "this string is 32 bytes in UTF-8", &result);
  EXPECT_EQ(result, nearby_share::mojom::DeviceNameValidationResult::kValid);
}

TEST_F(NearbyShareSettingsTest, GetAndSetDeviceName) {
  std::string name = "not_the_default";
  nearby_share_settings_waiter_.GetDeviceName(&name);
  EXPECT_EQ(kDefaultDeviceName, name);

  // When we get a validation error, setting the name should not succeed.
  EXPECT_EQ("uncalled", observer_.device_name);
  auto result = nearby_share::mojom::DeviceNameValidationResult::kValid;
  local_device_data_manager_.set_next_validation_result(
      nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty);
  nearby_share_settings_waiter_.SetDeviceName("", &result);
  EXPECT_EQ(result,
            nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty);
  EXPECT_EQ(kDefaultDeviceName, nearby_share_settings_.GetDeviceName());

  // When the name is valid, setting should succeed.
  EXPECT_EQ("uncalled", observer_.device_name);
  result = nearby_share::mojom::DeviceNameValidationResult::kValid;
  local_device_data_manager_.set_next_validation_result(
      nearby_share::mojom::DeviceNameValidationResult::kValid);
  nearby_share_settings_waiter_.SetDeviceName("d", &result);
  EXPECT_EQ(result, nearby_share::mojom::DeviceNameValidationResult::kValid);
  EXPECT_EQ("d", nearby_share_settings_.GetDeviceName());

  EXPECT_EQ("uncalled", observer_.device_name);
  FlushMojoMessages();
  EXPECT_EQ("d", observer_.device_name);

  nearby_share_settings_waiter_.GetDeviceName(&name);
  EXPECT_EQ("d", name);
}

TEST_F(NearbyShareSettingsTest, GetAndSetDataUsage) {
  EXPECT_EQ(nearby_share::mojom::DataUsage::kUnknown, observer_.data_usage);
  nearby_share_settings_.SetDataUsage(DataUsage::kOffline);
  EXPECT_EQ(DataUsage::kOffline, nearby_share_settings_.GetDataUsage());
  FlushMojoMessages();
  EXPECT_EQ(nearby_share::mojom::DataUsage::kOffline, observer_.data_usage);

  nearby_share::mojom::DataUsage data_usage =
      nearby_share::mojom::DataUsage::kUnknown;
  nearby_share_settings_waiter_.GetDataUsage(&data_usage);
  EXPECT_EQ(nearby_share::mojom::DataUsage::kOffline, data_usage);
}

TEST_F(NearbyShareSettingsTest, GetAndSetVisibility) {
  EXPECT_EQ(nearby_share::mojom::Visibility::kUnknown, observer_.visibility);
  nearby_share_settings_.SetVisibility(Visibility::kNoOne);
  EXPECT_EQ(Visibility::kNoOne, nearby_share_settings_.GetVisibility());
  FlushMojoMessages();
  EXPECT_EQ(nearby_share::mojom::Visibility::kNoOne, observer_.visibility);

  nearby_share::mojom::Visibility visibility =
      nearby_share::mojom::Visibility::kUnknown;
  nearby_share_settings_waiter_.GetVisibility(&visibility);
  EXPECT_EQ(nearby_share::mojom::Visibility::kNoOne, visibility);
}

TEST_F(NearbyShareSettingsTest, GetAndSetAllowedContacts) {
  const std::string id1("1");

  std::vector<std::string> allowed_contacts;

  nearby_share_settings_waiter_.GetAllowedContacts(&allowed_contacts);
  EXPECT_EQ(0u, allowed_contacts.size());

  nearby_share_settings_.SetAllowedContacts({id1});
  FlushMojoMessages();
  EXPECT_EQ(1u, observer_.allowed_contacts.size());
  EXPECT_EQ(true, base::Contains(observer_.allowed_contacts, id1));

  nearby_share_settings_waiter_.GetAllowedContacts(&allowed_contacts);
  EXPECT_EQ(1u, allowed_contacts.size());
  EXPECT_EQ(true, base::Contains(allowed_contacts, id1));

  nearby_share_settings_.SetAllowedContacts({});
  FlushMojoMessages();
  EXPECT_EQ(0u, observer_.allowed_contacts.size());

  nearby_share_settings_waiter_.GetAllowedContacts(&allowed_contacts);
  EXPECT_EQ(0u, allowed_contacts.size());
}
