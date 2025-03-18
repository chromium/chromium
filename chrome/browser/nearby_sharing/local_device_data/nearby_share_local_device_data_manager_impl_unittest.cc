// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"

#include <locale>
#include <memory>
#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler_factory.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

constexpr char kFakeEmail[] = "test@test";
constexpr GaiaId::Literal kFakeGaia("fakegaia");
constexpr char kFakeDeviceName[] = "My Cool Chromebook";
constexpr char kFakeEmptyDeviceName[] = "";
constexpr char kFakeFullName[] = "Barack Obama";
constexpr char16_t kFakeGivenName[] = u"Barack";
constexpr char kFakeIconUrl[] = "https://www.google.com";
constexpr char kFakeIconUrl2[] = "https://www.google.com/2";
constexpr char kFakeIconToken[] = "token";
constexpr char kFakeIconToken2[] = "token2";
constexpr char kFakeInvalidDeviceName[] = "\xC0";
constexpr char kFakeTooLongDeviceName[] = "this string is 33 bytes in UTF-8!";
constexpr char16_t kFakeTooLongGivenName[] =
    u"this is a 33-byte string in utf-8";
constexpr char kFakeTooLongTruncatedDeviceName[] =
    "this is a 33-...'s Chrome device";

nearby::sharing::proto::UpdateDeviceResponse CreateResponse(
    const std::optional<std::string>& full_name,
    const std::optional<std::string>& icon_url,
    const std::optional<std::string>& icon_token) {
  nearby::sharing::proto::UpdateDeviceResponse response;
  if (full_name)
    response.set_person_name(*full_name);

  if (icon_url)
    response.set_image_url(*icon_url);

  if (icon_token)
    response.set_image_token(*icon_token);

  return response;
}

std::vector<nearby::sharing::proto::Contact> GetFakeContacts() {
  nearby::sharing::proto::Contact contact1;
  nearby::sharing::proto::Contact contact2;
  contact1.mutable_identifier()->set_account_name("account1");
  contact2.mutable_identifier()->set_account_name("account2");
  return {std::move(contact1), std::move(contact2)};
}

std::vector<nearby::sharing::proto::PublicCertificate> GetFakeCertificates() {
  nearby::sharing::proto::PublicCertificate cert1;
  nearby::sharing::proto::PublicCertificate cert2;
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
                         bool did_icon_change)
        : did_device_name_change(did_device_name_change),
          did_full_name_change(did_full_name_change),
          did_icon_change(did_icon_change) {}
    ~ObserverNotification() = default;
    bool operator==(const ObserverNotification& other) const {
      return did_device_name_change == other.did_device_name_change &&
             did_full_name_change == other.did_full_name_change &&
             did_icon_change == other.did_icon_change;
    }

    bool did_device_name_change;
    bool did_full_name_change;
    bool did_icon_change;
  };

  NearbyShareLocalDeviceDataManagerImplTest() = default;
  ~NearbyShareLocalDeviceDataManagerImplTest() override = default;

  void SetUp() override {
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    fake_user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(&local_state_));
    user_ = fake_user_manager_->AddGaiaUser(
        AccountId::FromUserEmailGaiaId(kFakeEmail, kFakeGaia),
        user_manager::UserType::kRegular);
    fake_user_manager_->UserLoggedIn(
        user_->GetAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(user_->GetAccountId()));
    RegisterNearbySharingPrefs(pref_service_.registry());
    fake_user_manager_->OnUserProfileCreated(user_->GetAccountId(),
                                             &pref_service_);
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(
        &scheduler_factory_);
    NearbyShareDeviceDataUpdaterImpl::Factory::SetFactoryForTesting(
        &updater_factory_);
    fake_user_manager_->UpdateUserAccountData(
        user_->GetAccountId(), user_manager::UserManager::UserAccountData(
                                   /*display_name=*/u"",
                                   /*given_name=*/kFakeGivenName,
                                   /*locale=*/""));
  }

  void TearDown() override {
    manager_.reset();
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyShareDeviceDataUpdaterImpl::Factory::SetFactoryForTesting(nullptr);
    fake_user_manager_->OnUserProfileWillBeDestroyed(user_->GetAccountId());
    user_ = nullptr;
    fake_user_manager_.Reset();
  }

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_change) override {
    notifications_.emplace_back(did_device_name_change, did_full_name_change,
                                did_icon_change);
  }

  void CreateManager() {
    manager_ = NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
        *user_, &http_client_factory_);
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
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
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
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response) {
    std::optional<bool> returned_success;
    manager_->UploadContacts(
        GetFakeContacts(),
        base::BindOnce([](std::optional<bool>* returned_success,
                          bool success) { *returned_success = success; },
                       &returned_success));

    EXPECT_FALSE(updater()->pending_requests().front().certificates);
    std::vector<nearby::sharing::proto::Contact> expected_fake_contacts =
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
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response) {
    std::optional<bool> returned_success;
    manager_->UploadCertificates(
        GetFakeCertificates(),
        base::BindOnce([](std::optional<bool>* returned_success,
                          bool success) { *returned_success = success; },
                       &returned_success));

    EXPECT_FALSE(updater()->pending_requests().front().contacts);
    std::vector<nearby::sharing::proto::PublicCertificate>
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
  const std::vector<ObserverNotification>& notifications() {
    return notifications_;
  }
  FakeNearbyShareDeviceDataUpdater* updater() {
    return updater_factory_.instances().back();
  }
  ash::nearby::FakeNearbyScheduler* device_data_scheduler() {
    return scheduler_factory_.pref_name_to_periodic_instance()
        .at(prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName)
        .fake_scheduler;
  }

  user_manager::FakeUserManager& fake_user_manager() {
    return *fake_user_manager_;
  }
  user_manager::User& user() { return *user_; }

 private:
  void VerifyInitialization() {
    // Verify updater inputs.
    EXPECT_LT(base::Seconds(1), updater_factory_.latest_timeout());
    EXPECT_EQ(&http_client_factory_, updater_factory_.latest_client_factory());
    ASSERT_EQ(num_manager_creations_, updater_factory_.instances().size());
    EXPECT_EQ(manager_->GetId(),
              updater_factory_.instances().back()->device_id());

    // Verify device data scheduler input parameters.
    ash::nearby::FakeNearbySchedulerFactory::PeriodicInstance
        device_data_scheduler_instance =
            scheduler_factory_.pref_name_to_periodic_instance().at(
                prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName);
    EXPECT_TRUE(device_data_scheduler_instance.fake_scheduler);
    EXPECT_EQ(base::Hours(12), device_data_scheduler_instance.request_period);
    EXPECT_TRUE(device_data_scheduler_instance.retry_failures);
    EXPECT_TRUE(device_data_scheduler_instance.require_connectivity);
    EXPECT_EQ(&pref_service_, device_data_scheduler_instance.pref_service);
  }

  TestingPrefServiceSimple local_state_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  raw_ptr<user_manager::User> user_ = nullptr;
  sync_preferences::TestingPrefServiceSyncable pref_service_;

  size_t num_manager_creations_ = 0;
  std::vector<ObserverNotification> notifications_;
  FakeNearbyShareClientFactory http_client_factory_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
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
    EXPECT_TRUE(absl::ascii_isalnum(static_cast<unsigned char>(c)));

  // The ID is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ(id, manager()->GetId());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DefaultDeviceName) {
  CreateManager();

  // If given name is empty, only return the device type.
  fake_user_manager().UpdateUserAccountData(
      user().GetAccountId(), user_manager::UserManager::UserAccountData(
                                 /*display_name=*/u"",
                                 /*given_name=*/u"",
                                 /*locale=*/""));
  EXPECT_EQ(base::UTF16ToUTF8(ui::GetChromeOSDeviceName()),
            manager()->GetDeviceName());

  // Set given name and expect full default device name of the form
  // "<given name>'s <device type>."
  fake_user_manager().UpdateUserAccountData(
      user().GetAccountId(), user_manager::UserManager::UserAccountData(
                                 /*display_name=*/u"",
                                 /*given_name=*/kFakeGivenName,
                                 /*locale=*/""));
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME, kFakeGivenName,
                                ui::GetChromeOSDeviceName()),
      manager()->GetDeviceName());

  // Make sure that when we use a given name that is very long we truncate
  // correctly.
  fake_user_manager().UpdateUserAccountData(
      user().GetAccountId(), user_manager::UserManager::UserAccountData(
                                 /*display_name=*/u"",
                                 /*given_name=*/kFakeTooLongGivenName,
                                 /*locale=*/""));
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

  std::string expected_default_device_name =
      l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME, kFakeGivenName,
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
                                 /*did_icon_change=*/false),
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

  DownloadDeviceData(
      CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl, manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/false,
                                 /*did_full_name_change=*/true,
                                 /*did_icon_change=*/true),
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

  // The server returns empty strings for the full name and icon URL/token.
  // GetFullName() and GetIconUrl() should return non-nullopt values even though
  // they are trivial values.
  DownloadDeviceData(CreateResponse("", "", ""));
  EXPECT_EQ("", manager()->GetFullName());
  EXPECT_EQ("", manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/false,
                                 /*did_full_name_change=*/true,
                                 /*did_icon_change=*/true),
            notifications()[0]);

  // Return empty strings again. Ensure that the trivial full name and icon
  // URL/token values are not considered changed and no notification is sent.
  DownloadDeviceData(CreateResponse("", "", ""));
  EXPECT_EQ("", manager()->GetFullName());
  EXPECT_EQ("", manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());

  // The data is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ("", manager()->GetFullName());
  EXPECT_EQ("", manager()->GetIconUrl());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest,
       DownloadDeviceData_IconToken) {
  CreateManager();
  EXPECT_FALSE(manager()->GetFullName());
  EXPECT_FALSE(manager()->GetIconUrl());
  EXPECT_TRUE(notifications().empty());

  DownloadDeviceData(
      CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl, manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/false,
                                 /*did_full_name_change=*/true,
                                 /*did_icon_change=*/true),
            notifications()[0]);

  // Destroy and recreate to ensure name, URL, and token are all persisted.
  DestroyManager();
  CreateManager();

  // The icon URL changes but the token does not; no notification sent.
  DownloadDeviceData(
      CreateResponse(kFakeFullName, kFakeIconUrl2, kFakeIconToken));
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl2, manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());

  // The icon token changes but the URL does not; no notification sent.
  DestroyManager();
  CreateManager();
  DownloadDeviceData(
      CreateResponse(kFakeFullName, kFakeIconUrl2, kFakeIconToken2));
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl2, manager()->GetIconUrl());
  EXPECT_EQ(1u, notifications().size());

  // The icon URL and token change; notification sent.
  DestroyManager();
  CreateManager();
  DownloadDeviceData(
      CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl, manager()->GetIconUrl());
  EXPECT_EQ(2u, notifications().size());
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/false,
                                 /*did_full_name_change=*/false,
                                 /*did_icon_change=*/true),
            notifications()[1]);

  // The data is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ(kFakeFullName, manager()->GetFullName());
  EXPECT_EQ(kFakeIconUrl, manager()->GetIconUrl());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DownloadDeviceData_Failure) {
  CreateManager();
  DownloadDeviceData(/*response=*/std::nullopt);

  // No full name or icon URL set because response was null.
  EXPECT_EQ(std::nullopt, manager()->GetFullName());
  EXPECT_EQ(std::nullopt, manager()->GetIconUrl());
  EXPECT_TRUE(notifications().empty());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadContacts_Success) {
  CreateManager();
  UploadContacts(CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadContacts_Failure) {
  CreateManager();
  UploadContacts(/*response=*/std::nullopt);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Success) {
  CreateManager();
  UploadCertificates(
      CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Failure) {
  CreateManager();
  UploadCertificates(/*response=*/std::nullopt);
}
