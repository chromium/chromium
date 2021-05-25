// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <locale>
#include <memory>
#include <string>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/fake_nearby_share_profile_info_provider.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "chrome/browser/nearby_sharing/proto/device_rpc.pb.h"
#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

const char kFakeDeviceName[] = "My Cool Chromebook";
const char kFakeEmptyDeviceName[] = "";
const char kFakeFullName[] = "Barack Obama";
const char kFakeGivenName[] = "Barack";
const char kFakeIconUrl[] = "https://www.google.com";
const char kFakeInvalidDeviceName[] = {0xC0, 0x00};
const char kFakeTooLongDeviceName[] = "this string is 33 bytes in UTF-8!";
const char kFakeTooLongGivenName[] = "this is a 33-byte string in utf-8";
const char kFakeTooLongTruncatedDeviceName[] =
    "this is a 33-...'s Chrome device";

nearbyshare::proto::UpdateDeviceResponse CreateResponse(
    const base::Optional<std::string>& full_name,
    const base::Optional<std::string>& icon_url) {
  nearbyshare::proto::UpdateDeviceResponse response;
  if (full_name)
    response.set_person_name(*full_name);

  if (icon_url)
    response.set_image_url(*icon_url);

  return response;
}

std::vector<nearbyshare::proto::Contact> GetFakeContacts() {
  nearbyshare::proto::Contact contact1;
  nearbyshare::proto::Contact contact2;
  contact1.mutable_identifier()->set_account_name("account1");
  contact2.mutable_identifier()->set_account_name("account2");
  return {std::move(contact1), std::move(contact2)};
}

std::vector<nearbyshare::proto::PublicCertificate> GetFakeCertificates() {
  nearbyshare::proto::PublicCertificate cert1;
  nearbyshare::proto::PublicCertificate cert2;
  cert1.set_secret_id("id1");
  cert2.set_secret_id("id2");
  return {std::move(cert1), std::move(cert2)};
}

}  // namespace

class NearbyShareLocalDeviceDataManagerImplTest
    : public ::testing::Test,
      public NearbyShareLocalDeviceDataManager::Observer {
 protected:
  struct ObserverNotification {
    ObserverNotification(bool did_device_name_change,
                         bool did_full_name_change,
                         bool did_icon_url_change)
        : did_device_name_change(did_device_name_change),
          did_full_name_change(did_full_name_change),
          did_icon_url_change(did_icon_url_change) {}
    ~ObserverNotification() = default;
    bool operator==(const ObserverNotification& other) const {
      return did_device_name_change == other.did_device_name_change &&
             did_full_name_change == other.did_full_name_change &&
             did_icon_url_change == other.did_icon_url_change;
    }

    bool did_device_name_change;
    bool did_full_name_change;
    bool did_icon_url_change;
  };

  NearbyShareLocalDeviceDataManagerImplTest() = default;
  ~NearbyShareLocalDeviceDataManagerImplTest() override = default;

  void SetUp() override {
    RegisterNearbySharingPrefs(pref_service_.registry());
    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    NearbyShareDeviceDataUpdaterImpl::Factory::SetFactoryForTesting(
        &updater_factory_);
    profile_info_provider()->set_given_name(base::UTF8ToUTF16(kFakeGivenName));
  }

  void TearDown() override {
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyShareDeviceDataUpdaterImpl::Factory::SetFactoryForTesting(nullptr);
  }

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_url_change) override {
    notifications_.emplace_back(did_device_name_change, did_full_name_change,
                                did_icon_url_change);
  }

  void CreateManager() {
    manager_ = NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
        &pref_service_, &http_client_factory_, &profile_info_provider_);
    manager_->AddObserver(this);
    ++num_manager_creations_;
    VerifyInitialization();
    manager_->Start();
  }

  void DestroyManager() {
    manager_->RemoveObserver(this);
    manager_.reset();
  }

  void DownloadDeviceData(
      const base::Optional<nearbyshare::proto::UpdateDeviceResponse>&
          response) {
    manager_->DownloadDeviceData();

    // The scheduler requests a download of device data from the server.
    EXPECT_TRUE(updater()->pending_requests().empty());
    device_data_scheduler()->InvokeRequestCallback();
    EXPECT_FALSE(updater()->pending_requests().empty());

    EXPECT_FALSE(updater()->pending_requests().front().contacts);
    EXPECT_FALSE(updater()->pending_requests().front().certificates);

    size_t num_handled_results =
        device_data_scheduler()->handled_results().size();
    updater()->RunNextRequest(response);
    EXPECT_EQ(num_handled_results + 1,
              device_data_scheduler()->handled_results().size());
    EXPECT_EQ(response.has_value(),
              device_data_scheduler()->handled_results().back());
  }

  void UploadContacts(
      const base::Optional<nearbyshare::proto::UpdateDeviceResponse>&
          response) {
    base::Optional<bool> returned_success;
    manager_->UploadContacts(
        GetFakeContacts(),
        base::BindOnce([](base::Optional<bool>* returned_success,
                          bool success) { *returned_success = success; },
                       &returned_success));

    EXPECT_FALSE(updater()->pending_requests().front().certificates);
    std::vector<nearbyshare::proto::Contact> expected_fake_contacts =
        GetFakeContacts();
    for (size_t i = 0; i < expected_fake_contacts.size(); ++i) {
      EXPECT_EQ(expected_fake_contacts[i].SerializeAsString(),
                updater()
                    ->pending_requests()
                    .front()
                    .contacts->at(i)
                    .SerializeAsString());
    }

    EXPECT_FALSE(returned_success);
    updater()->RunNextRequest(response);
    EXPECT_EQ(response.has_value(), returned_success);
  }

  void UploadCertificates(
      const base::Optional<nearbyshare::proto::UpdateDeviceResponse>&
          response) {
    base::Optional<bool> returned_success;
    manager_->UploadCertificates(
        GetFakeCertificates(),
        base::BindOnce([](base::Optional<bool>* returned_success,
                          bool success) { *returned_success = success; },
                       &returned_success));

    EXPECT_FALSE(updater()->pending_requests().front().contacts);
    std::vector<nearbyshare::proto::PublicCertificate>
        expected_fake_certificates = GetFakeCertificates();
    for (size_t i = 0; i < expected_fake_certificates.size(); ++i) {
      EXPECT_EQ(expected_fake_certificates[i].SerializeAsString(),
                updater()
                    ->pending_requests()
                    .front()
                    .certificates->at(i)
                    .SerializeAsString());
    }

    EXPECT_FALSE(returned_success);
    updater()->RunNextRequest(response);
    EXPECT_EQ(response.has_value(), returned_success);
  }

  NearbyShareLocalDeviceDataManager* manager() { return manager_.get(); }
  FakeNearbyShareProfileInfoProvider* profile_info_provider() {
    return &profile_info_provider_;
  }
  const std::vector<ObserverNotification>& notifications() {
    return notifications_;
  }
  FakeNearbyShareDeviceDataUpdater* updater() {
    return updater_factory_.instances().back();
  }
  FakeNearbyShareScheduler* device_data_scheduler() {
    return scheduler_factory_.pref_name_to_periodic_instance()
        .at(prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName)
        .fake_scheduler;
  }

 private:
  void VerifyInitialization() {
    // Verify updater inputs.
    EXPECT_LT(base::TimeDelta::FromSeconds(1),
              updater_factory_.latest_timeout());
    EXPECT_EQ(&http_client_factory_, updater_factory_.latest_client_factory());
    ASSERT_EQ(num_manager_creations_, updater_factory_.instances().size());
    EXPECT_EQ(manager_->GetId(),
              updater_factory_.instances().back()->device_id());

    // Verify device data scheduler input parameters.
    FakeNearbyShareSchedulerFactory::PeriodicInstance
        device_data_scheduler_instance =
            scheduler_factory_.pref_name_to_periodic_instance().at(
                prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName);
    EXPECT_TRUE(device_data_scheduler_instance.fake_scheduler);
    EXPECT_EQ(base::TimeDelta::FromHours(12),
              device_data_scheduler_instance.request_period);
    EXPECT_TRUE(device_data_scheduler_instance.retry_failures);
    EXPECT_TRUE(device_data_scheduler_instance.require_connectivity);
    EXPECT_EQ(&pref_service_, device_data_scheduler_instance.pref_service);
  }

  size_t num_manager_creations_ = 0;
  std::vector<ObserverNotification> notifications_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeNearbyShareClientFactory http_client_factory_;
  FakeNearbyShareProfileInfoProvider profile_info_provider_;
  FakeNearbyShareSchedulerFactory scheduler_factory_;
  FakeNearbyShareDeviceDataUpdaterFactory updater_factory_;
  std::unique_ptr<NearbyShareLocalDeviceDataManager> manager_;
};

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DeviceId) {
  CreateManager();

  // A 10-character alphanumeric ID is automatically generated if one doesn't
  // already exist.
  std::string id = manager()->GetId();
  EXPECT_EQ(10u, id.size());
  for (const char c : id)
    EXPECT_TRUE(std::isalnum(c));

  // The ID is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ(id, manager()->GetId());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DefaultDeviceName) {
  CreateManager();

  // If given name is null, only return the device type.
  profile_info_provider()->set_given_name(base::nullopt);
  EXPECT_EQ(base::UTF16ToUTF8(ui::GetChromeOSDeviceName()),
            manager()->GetDeviceName());

  // Set given name and expect full default device name of the form
  // "<given name>'s <device type>."
  profile_info_provider()->set_given_name(base::UTF8ToUTF16(kFakeGivenName));
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME,
                                      base::UTF8ToUTF16(kFakeGivenName),
                                      ui::GetChromeOSDeviceName()),
            manager()->GetDeviceName());

  // Make sure that when we use a given name that is very long we truncate
  // correctly.
  profile_info_provider()->set_given_name(
      base::UTF8ToUTF16(kFakeTooLongGivenName));
  EXPECT_EQ(kFakeTooLongTruncatedDeviceName, manager()->GetDeviceName());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, ValidateDeviceName) {
  CreateManager();
  EXPECT_EQ(manager()->ValidateDeviceName(kFakeDeviceName),
            nearby_share::mojom::DeviceNameValidationResult::kValid);
  EXPECT_EQ(manager()->ValidateDeviceName(kFakeEmptyDeviceName),
            nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty);
  EXPECT_EQ(manager()->ValidateDeviceName(kFakeTooLongDeviceName),
            nearby_share::mojom::DeviceNameValidationResult::kErrorTooLong);
  EXPECT_EQ(
      manager()->ValidateDeviceName(kFakeInvalidDeviceName),
      nearby_share::mojom::DeviceNameValidationResult::kErrorNotValidUtf8);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, SetDeviceName) {
  CreateManager();

  profile_info_provider()->set_given_name(base::UTF8ToUTF16(kFakeGivenName));
  std::string expected_default_device_name = l10n_util::GetStringFUTF8(
      IDS_NEARBY_DEFAULT_DEVICE_NAME, base::UTF8ToUTF16(kFakeGivenName),
      ui::GetChromeOSDeviceName());
  EXPECT_EQ(expected_default_device_name, manager()->GetDeviceName());
  EXPECT_TRUE(notifications().empty());

  auto error = manager()->SetDeviceName(kFakeEmptyDeviceName);
  EXPECT_EQ(error,
            nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty);
  EXPECT_EQ(expected_default_device_name, manager()->GetDeviceName());
  EXPECT_TRUE(notifications().empty());

  error = manager()->SetDeviceName(kFakeTooLongDeviceName);
  EXPECT_EQ(error,
            nearby_share::mojom::DeviceNameValidationResult::kErrorTooLong);
  EXPECT_EQ(expected_default_device_name, manager()->GetDeviceName());
  EXPECT_TRUE(notifications().empty());

  error = manager()->SetDeviceName(kFakeInvalidDeviceName);
  EXPECT_EQ(
      error,
      nearby_share::mojom::DeviceNameValidationResult::kErrorNotValidUtf8);
  EXPECT_EQ(expected_default_device_name, manager()->GetDeviceName());
  EXPECT_TRUE(notifications().empty());

  error = manager()->SetDeviceName(kFakeDeviceName);
  EXPECT_EQ(error, nearby_share::mojom::DeviceNameValidationResult::kValid);
  EXPECT_EQ(kFakeDeviceName, manager()->GetDeviceName());
  EXPECT_EQ(1u, notifications().size());
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/true,
                                 /*did_full_name_change=*/false,
                                 /*did_icon_url_change=*/false),
            notifications().back());

  // The data is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ(kFakeDeviceName, manager()->GetDeviceName());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DownloadDeviceData_Success) {
  CreateManager();
  EXPECT_FALSE(manager()->GetFullName());
  EXPECT_FALSE(manager()->GetIconUrl());
  EXPECT_TRUE(notifications().empty());
  DownloadDeviceData(CreateResponse(kFakeFullName, kFakeIconUrl));
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl, manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/false,
                                 /*did_full_name_change=*/true,
                                 /*did_icon_url_change=*/true),
            notifications()[0]);

  // The data is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl, manager()->GetIconUrl());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest,
       DownloadDeviceData_EmptyData) {
  CreateManager();
  EXPECT_FALSE(manager()->GetFullName());
  EXPECT_FALSE(manager()->GetIconUrl());
  EXPECT_TRUE(notifications().empty());

  // The server returns empty strings for the full name and icon URL.
  // GetFullName() and GetIconUrl() should return non-nullopt values even though
  // they are trivial values.
  DownloadDeviceData(CreateResponse("", ""));
  EXPECT_EQ("", manager()->GetFullName());
  EXPECT_EQ("", manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/false,
                                 /*did_full_name_change=*/true,
                                 /*did_icon_url_change=*/true),
            notifications()[0]);

  // Return empty strings again. Ensure that the trivial full name and icon URL
  // values are not considered changed and no notification is sent.
  DownloadDeviceData(CreateResponse("", ""));
  EXPECT_EQ("", manager()->GetFullName());
  EXPECT_EQ("", manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());

  // The data is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ("", manager()->GetFullName());
  EXPECT_EQ("", manager()->GetIconUrl());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DownloadDeviceData_Failure) {
  CreateManager();
  DownloadDeviceData(/*response=*/base::nullopt);

  // No full name or icon URL set because response was null.
  EXPECT_EQ(base::nullopt, manager()->GetFullName());
  EXPECT_EQ(base::nullopt, manager()->GetIconUrl());
  EXPECT_TRUE(notifications().empty());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadContacts_Success) {
  CreateManager();
  UploadContacts(CreateResponse(kFakeFullName, kFakeIconUrl));

  // TODO(http://crbug.com/1211189): Only process the UpdateDevice response for
  // DownloadDeviceData() calls. We want avoid infinite loops if the full name
  // or icon URL unexpectedly change. When the bug is resolved, check that the
  // full name and icon URL were properly handed in the response response sent
  // from uploading contacts or certificates as well.
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadContacts_Failure) {
  CreateManager();
  UploadContacts(/*response=*/base::nullopt);

  // TODO(http://crbug.com/1211189): Only process the UpdateDevice response for
  // DownloadDeviceData() calls. We want avoid infinite loops if the full name
  // or icon URL unexpectedly change. When the bug is resolved, check that the
  // full name and icon URL were properly handed in the response response sent
  // from uploading contacts or certificates as well.
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Success) {
  CreateManager();
  UploadCertificates(CreateResponse(kFakeFullName, kFakeIconUrl));

  // TODO(http://crbug.com/1211189): Only process the UpdateDevice response for
  // DownloadDeviceData() calls. We want avoid infinite loops if the full name
  // or icon URL unexpectedly change. When the bug is resolved, check that the
  // full name and icon URL were properly handed in the response response sent
  // from uploading contacts or certificates as well.
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Failure) {
  CreateManager();
  UploadCertificates(/*response=*/base::nullopt);

  // TODO(http://crbug.com/1211189): Only process the UpdateDevice response for
  // DownloadDeviceData() calls. We want avoid infinite loops if the full name
  // or icon URL unexpectedly change. When the bug is resolved, check that the
  // full name and icon URL were properly handed in the response response sent
  // from uploading contacts or certificates as well.
}
