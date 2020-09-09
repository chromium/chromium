// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/fake_nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::Time t0 =
    base::Time::UnixEpoch() + base::TimeDelta::FromDays(365 * 50);

// Copied from nearby_share_certificate_manager_impl.cc.
constexpr base::TimeDelta kListPublicCertificatesTimeout =
    base::TimeDelta::FromSeconds(30);

const char kPageTokenPrefix[] = "page_token_";
const char kSecretIdPrefix[] = "secret_id_";
const char kDeviceIdPrefix[] = "users/me/devices/";
const char kDeviceId[] = "123456789A";

const std::vector<std::string> kPublicCertificateIds = {"id1", "id2", "id3"};

void CaptureDecryptedPublicCertificateCallback(
    base::Optional<NearbyShareDecryptedPublicCertificate>* dest,
    base::Optional<NearbyShareDecryptedPublicCertificate> src) {
  *dest = std::move(src);
}

}  // namespace

class NearbyShareCertificateManagerImplTest
    : public ::testing::Test,
      public NearbyShareCertificateManager::Observer {
 public:
  NearbyShareCertificateManagerImplTest() {
    // Set time to t0.
    FastForward(t0 - base::Time::UnixEpoch());

    local_device_data_manager_ =
        std::make_unique<FakeNearbyShareLocalDeviceDataManager>();
    local_device_data_manager_->SetId(kDeviceId);

    contact_manager_ = std::make_unique<FakeNearbyShareContactManager>();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kNearbySharingSchedulerDownloadPublicCertificatesPrefName);

    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(
        &cert_store_factory_);

    cert_manager_ = NearbyShareCertificateManagerImpl::Factory::Create(
        local_device_data_manager_.get(), contact_manager_.get(),
        pref_service_.get(),
        /*proto_database_provider=*/nullptr, base::FilePath(), &client_factory_,
        task_environment_.GetMockClock());
    cert_manager_->AddObserver(this);

    cert_store_ = cert_store_factory_.instances().back();

    private_cert_exp_scheduler_ =
        scheduler_factory_.pref_name_to_expiration_instance()
            .find(
                prefs::
                    kNearbySharingSchedulerPrivateCertificateExpirationPrefName)
            ->second.fake_scheduler;
    public_cert_exp_scheduler_ =
        scheduler_factory_.pref_name_to_expiration_instance()
            .find(
                prefs::
                    kNearbySharingSchedulerPublicCertificateExpirationPrefName)
            ->second.fake_scheduler;
    upload_scheduler_ =
        scheduler_factory_.pref_name_to_on_demand_instance()
            .find(
                prefs::
                    kNearbySharingSchedulerUploadLocalDeviceCertificatesPrefName)
            ->second.fake_scheduler;
    download_scheduler_ =
        scheduler_factory_.pref_name_to_periodic_instance()
            .find(prefs::
                      kNearbySharingSchedulerDownloadPublicCertificatesPrefName)
            ->second.fake_scheduler;

    PopulatePrivateCertificates();
    PopulatePublicCertificates();

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetAddress()).WillByDefault([this] {
      return bluetooth_mac_address_;
    });
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  ~NearbyShareCertificateManagerImplTest() override = default;

  void TearDown() override {
    cert_manager_->RemoveObserver(this);
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void SetBluetoothMacAddress(const std::string& bluetooth_mac_address) {
    bluetooth_mac_address_ = bluetooth_mac_address;
  }

  // NearbyShareCertificateManager::Observer:
  void OnPublicCertificatesDownloaded() override {
    ++num_public_certs_downloaded_notifications_;
  }
  void OnPrivateCertificatesChanged() override {
    ++num_private_certs_changed_notifications_;
  }

 protected:
  enum class DownloadPublicCertificatesResult {
    kSuccess,
    kTimeout,
    kHttpError,
    kStorageError
  };

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void GetPublicCertificatesCallback(
      bool success,
      const std::vector<nearbyshare::proto::PublicCertificate>& certs) {
    auto& callbacks = cert_store_->get_public_certificates_callbacks();
    auto callback = std::move(callbacks.back());
    callbacks.pop_back();
    auto pub_certs =
        std::make_unique<std::vector<nearbyshare::proto::PublicCertificate>>(
            certs.begin(), certs.end());
    std::move(callback).Run(success, std::move(pub_certs));
  }

  void HandlePrivateCertificateRefresh(bool expect_private_cert_refresh,
                                       bool expected_success) {
    if (expect_private_cert_refresh)
      private_cert_exp_scheduler_->InvokeRequestCallback();

    EXPECT_EQ(expect_private_cert_refresh ? 1u : 0u,
              private_cert_exp_scheduler_->handled_results().size());
    if (expect_private_cert_refresh) {
      EXPECT_EQ(expected_success,
                private_cert_exp_scheduler_->handled_results().back());
    }
    EXPECT_EQ(expect_private_cert_refresh && expected_success ? 1u : 0u,
              num_private_certs_changed_notifications_);
    EXPECT_EQ(expect_private_cert_refresh && expected_success ? 1u : 0u,
              upload_scheduler_->num_immediate_requests());
  }

  void VerifyPrivateCertificates(
      const nearbyshare::proto::EncryptedMetadata& expected_metadata) {
    // Expect a full set of certificates for both all-contacts and
    // selected-contacts
    std::vector<NearbySharePrivateCertificate> certs =
        *cert_store_->GetPrivateCertificates();
    EXPECT_EQ(2 * kNearbyShareNumPrivateCertificates, certs.size());

    base::Time min_not_before_all_contacts = base::Time::Max();
    base::Time min_not_before_selected_contacts = base::Time::Max();
    base::Time max_not_after_all_contacts = base::Time::Min();
    base::Time max_not_after_selected_contacts = base::Time::Min();
    for (const auto& cert : certs) {
      EXPECT_EQ(cert.not_after() - cert.not_before(),
                kNearbyShareCertificateValidityPeriod);
      switch (cert.visibility()) {
        case nearby_share::mojom::Visibility::kAllContacts:
          min_not_before_all_contacts =
              std::min(min_not_before_all_contacts, cert.not_before());
          max_not_after_all_contacts =
              std::max(max_not_after_all_contacts, cert.not_after());
          break;
        case nearby_share::mojom::Visibility::kSelectedContacts:
          min_not_before_selected_contacts =
              std::min(min_not_before_selected_contacts, cert.not_before());
          max_not_after_selected_contacts =
              std::max(max_not_after_selected_contacts, cert.not_after());
          break;
        default:
          NOTREACHED();
          break;
      }

      // Verify metadata.
      EXPECT_EQ(expected_metadata.SerializeAsString(),
                cert.unencrypted_metadata().SerializeAsString());
    }

    // Verify contiguous validity periods
    EXPECT_EQ(kNearbyShareNumPrivateCertificates *
                  kNearbyShareCertificateValidityPeriod,
              max_not_after_all_contacts - min_not_before_all_contacts);
    EXPECT_EQ(
        kNearbyShareNumPrivateCertificates *
            kNearbyShareCertificateValidityPeriod,
        max_not_after_selected_contacts - min_not_before_selected_contacts);
  }

  void RunUpload(bool success) {
    size_t initial_num_upload_calls =
        local_device_data_manager_->upload_certificates_calls().size();
    upload_scheduler_->InvokeRequestCallback();
    EXPECT_EQ(initial_num_upload_calls + 1,
              local_device_data_manager_->upload_certificates_calls().size());

    EXPECT_EQ(2 * kNearbyShareNumPrivateCertificates,
              local_device_data_manager_->upload_certificates_calls()
                  .back()
                  .certificates.size());

    size_t initial_num_handled_results =
        upload_scheduler_->handled_results().size();
    std::move(
        local_device_data_manager_->upload_certificates_calls().back().callback)
        .Run(success);
    EXPECT_EQ(initial_num_handled_results + 1,
              upload_scheduler_->handled_results().size());
    EXPECT_EQ(success, upload_scheduler_->handled_results().back());
  }

  // Test downloading public certificates with or without errors. The RPC is
  // paginated, and |num_pages| will be simulated. Any failures, as indicated by
  // |result|, will be simulated on the last page.
  void DownloadPublicCertificatesFlow(size_t num_pages,
                                      DownloadPublicCertificatesResult result) {
    size_t prev_num_results = download_scheduler_->handled_results().size();
    cert_store_->SetPublicCertificateIds(kPublicCertificateIds);

    cert_manager_->Start();
    download_scheduler_->InvokeRequestCallback();
    cert_manager_->Stop();

    size_t initial_num_notifications =
        num_public_certs_downloaded_notifications_;
    size_t initial_num_public_cert_exp_reschedules =
        public_cert_exp_scheduler_->num_reschedule_calls();
    std::string page_token;
    for (size_t page_number = 0; page_number < num_pages; ++page_number) {
      bool last_page = page_number == num_pages - 1;

      auto& request = client_factory_.instances()
                          .back()
                          ->list_public_certificates_requests()
                          .back();
      CheckRpcRequest(request.request, page_token);

      if (last_page) {
        if (result == DownloadPublicCertificatesResult::kTimeout) {
          FastForward(kListPublicCertificatesTimeout);
          break;
        } else if (result == DownloadPublicCertificatesResult::kHttpError) {
          std::move(request.error_callback)
              .Run(NearbyShareHttpError::kResponseMalformed);
          break;
        }
      }

      page_token = last_page
                       ? std::string()
                       : kPageTokenPrefix + base::NumberToString(page_number);
      std::move(request.callback)
          .Run(BuildRpcResponse(page_number, page_token));

      auto& add_cert_call = cert_store_->add_public_certificates_calls().back();
      CheckStorageAddCertificates(add_cert_call);

      std::move(add_cert_call.callback)
          .Run(/*success=*/!last_page ||
               result != DownloadPublicCertificatesResult::kStorageError);
    }
    ASSERT_EQ(download_scheduler_->handled_results().size(),
              prev_num_results + 1);

    bool success = result == DownloadPublicCertificatesResult::kSuccess;
    EXPECT_EQ(download_scheduler_->handled_results().back(), success);
    EXPECT_EQ(initial_num_notifications + (success ? 1u : 0u),
              num_public_certs_downloaded_notifications_);
    EXPECT_EQ(initial_num_public_cert_exp_reschedules + (success ? 1u : 0u),
              public_cert_exp_scheduler_->num_reschedule_calls());
  }

  void CheckRpcRequest(
      const nearbyshare::proto::ListPublicCertificatesRequest& request,
      const std::string& page_token) {
    EXPECT_EQ(request.parent(), std::string(kDeviceIdPrefix) + kDeviceId);
    ASSERT_EQ(request.secret_ids_size(),
              static_cast<int>(kPublicCertificateIds.size()));
    for (size_t i = 0; i < kPublicCertificateIds.size(); ++i) {
      EXPECT_EQ(request.secret_ids(i), kPublicCertificateIds[i]);
    }
    EXPECT_EQ(request.page_token(), page_token);
  }

  nearbyshare::proto::ListPublicCertificatesResponse BuildRpcResponse(
      size_t page_number,
      const std::string& page_token) {
    nearbyshare::proto::ListPublicCertificatesResponse response;
    for (size_t i = 0; i < public_certificates_.size(); ++i) {
      public_certificates_[i].set_secret_id(kSecretIdPrefix +
                                            base::NumberToString(page_number) +
                                            "_" + base::NumberToString(i));
      response.add_public_certificates();
      *response.mutable_public_certificates(i) = public_certificates_[i];
    }
    response.set_next_page_token(page_token);
    return response;
  }

  void CheckStorageAddCertificates(
      const FakeNearbyShareCertificateStorage::AddPublicCertificatesCall&
          add_cert_call) {
    ASSERT_EQ(add_cert_call.public_certificates.size(),
              public_certificates_.size());
    for (size_t i = 0; i < public_certificates_.size(); ++i) {
      EXPECT_EQ(add_cert_call.public_certificates[i].secret_id(),
                public_certificates_[i].secret_id());
    }
  }

  void PopulatePrivateCertificates() {
    private_certificates_.clear();
    const auto& metadata = GetNearbyShareTestMetadata();
    for (auto visibility :
         {nearby_share::mojom::Visibility::kAllContacts,
          nearby_share::mojom::Visibility::kSelectedContacts}) {
      private_certificates_.emplace_back(visibility, t0, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod * 2, metadata);
    }
  }

  void PopulatePublicCertificates() {
    public_certificates_.clear();
    metadata_encryption_keys_.clear();
    auto& metadata1 = GetNearbyShareTestMetadata();
    nearbyshare::proto::EncryptedMetadata metadata2;
    metadata2.set_device_name("device_name2");
    metadata2.set_full_name("full_name2");
    metadata2.set_icon_url("icon_url2");
    metadata2.set_bluetooth_mac_address("bluetooth_mac_address2");
    for (auto metadata : {metadata1, metadata2}) {
      auto private_cert = NearbySharePrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts, t0, metadata);
      public_certificates_.push_back(*private_cert.ToPublicCertificate());
      metadata_encryption_keys_.push_back(*private_cert.EncryptMetadataKey());
    }
  }

  FakeNearbyShareCertificateStorage* cert_store_;
  FakeNearbyShareScheduler* private_cert_exp_scheduler_;
  FakeNearbyShareScheduler* public_cert_exp_scheduler_;
  FakeNearbyShareScheduler* upload_scheduler_;
  FakeNearbyShareScheduler* download_scheduler_;
  std::string bluetooth_mac_address_ = kTestUnparsedBluetoothMacAddress;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  size_t num_public_certs_downloaded_notifications_ = 0;
  size_t num_private_certs_changed_notifications_ = 0;
  std::vector<NearbySharePrivateCertificate> private_certificates_;
  std::vector<nearbyshare::proto::PublicCertificate> public_certificates_;
  std::vector<NearbyShareEncryptedMetadataKey> metadata_encryption_keys_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeNearbyShareClientFactory client_factory_;
  FakeNearbyShareSchedulerFactory scheduler_factory_;
  FakeNearbyShareCertificateStorage::Factory cert_store_factory_;
  std::unique_ptr<FakeNearbyShareLocalDeviceDataManager>
      local_device_data_manager_;
  std::unique_ptr<FakeNearbyShareContactManager> contact_manager_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<NearbyShareCertificateManager> cert_manager_;
};

TEST_F(NearbyShareCertificateManagerImplTest, GetValidPrivateCertificate) {
  cert_store_->SetPrivateCertificates(private_certificates_);
  FastForward(kNearbyShareCertificateValidityPeriod * 1.5);
  auto cert = cert_manager_->GetValidPrivateCertificate(
      nearby_share::mojom::Visibility::kAllContacts);

  EXPECT_EQ(nearby_share::mojom::Visibility::kAllContacts, cert.visibility());
  EXPECT_LE(cert.not_before(), Now());
  EXPECT_LT(Now(), cert.not_after());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateSuccess) {
  base::Optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                     &decrypted_pub_cert));

  GetPublicCertificatesCallback(true, public_certificates_);

  ASSERT_TRUE(decrypted_pub_cert);
  std::vector<uint8_t> id(public_certificates_[0].secret_id().begin(),
                          public_certificates_[0].secret_id().end());
  EXPECT_EQ(decrypted_pub_cert->id(), id);
  EXPECT_EQ(decrypted_pub_cert->unencrypted_metadata().SerializeAsString(),
            GetNearbyShareTestMetadata().SerializeAsString());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateCertNotFound) {
  auto private_cert = NearbySharePrivateCertificate(
      nearby_share::mojom::Visibility::kAllContacts, t0,
      GetNearbyShareTestMetadata());
  auto metadata_key = private_cert.EncryptMetadataKey();
  ASSERT_TRUE(metadata_key);

  base::Optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      *metadata_key, base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                                    &decrypted_pub_cert));

  GetPublicCertificatesCallback(true, public_certificates_);

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateGetPublicCertificatesFailure) {
  base::Optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                     &decrypted_pub_cert));

  GetPublicCertificatesCallback(false, {});

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesImmediateRequest) {
  size_t prev_num_requests = download_scheduler_->num_immediate_requests();
  cert_manager_->DownloadPublicCertificates();
  EXPECT_EQ(download_scheduler_->num_immediate_requests(),
            prev_num_requests + 1);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesSuccess) {
  DownloadPublicCertificatesFlow(/*num_pages=*/2,
                                 DownloadPublicCertificatesResult::kSuccess);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesTimeout) {
  DownloadPublicCertificatesFlow(/*num_pages=*/2,
                                 DownloadPublicCertificatesResult::kTimeout);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesRPCFailure) {
  DownloadPublicCertificatesFlow(/*num_pages=*/2,
                                 DownloadPublicCertificatesResult::kHttpError);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesStoreFailure) {
  DownloadPublicCertificatesFlow(
      /*num_pages=*/2, DownloadPublicCertificatesResult::kStorageError);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_ValidCertificates) {
  cert_store_->SetPrivateCertificates(private_certificates_);

  local_device_data_manager_->SetDeviceName(
      GetNearbyShareTestMetadata().device_name());
  local_device_data_manager_->SetFullName(
      GetNearbyShareTestMetadata().full_name());
  local_device_data_manager_->SetIconUrl(
      GetNearbyShareTestMetadata().icon_url());
  SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/false,
                                  /*expected_success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_NoCertificates_UploadSuccess) {
  cert_store_->SetPrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  local_device_data_manager_->SetDeviceName(
      GetNearbyShareTestMetadata().device_name());
  local_device_data_manager_->SetFullName(
      GetNearbyShareTestMetadata().full_name());
  local_device_data_manager_->SetIconUrl(
      GetNearbyShareTestMetadata().icon_url());
  SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_NoCertificates_UploadFailure) {
  cert_store_->SetPrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  local_device_data_manager_->SetDeviceName(
      GetNearbyShareTestMetadata().device_name());
  local_device_data_manager_->SetFullName(
      GetNearbyShareTestMetadata().full_name());
  local_device_data_manager_->SetIconUrl(
      GetNearbyShareTestMetadata().icon_url());
  SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/false);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RevokePrivateCertificates_OnAllowlistChanged) {
  cert_manager_->Start();

  // Destroy and recreate private certificates if and only if contacts were
  // removed from the user's list of selected contacts.
  size_t num_expected_calls = 0;
  for (bool were_contacts_added_to_allowlist : {true, false}) {
    for (bool were_contacts_removed_from_allowlist : {true, false}) {
      contact_manager_->NotifyAllowlistChanged(
          were_contacts_added_to_allowlist,
          were_contacts_removed_from_allowlist);
      if (were_contacts_removed_from_allowlist)
        ++num_expected_calls;

      EXPECT_EQ(num_expected_calls,
                cert_store_->num_clear_private_certificates_calls());
      EXPECT_EQ(num_expected_calls,
                private_cert_exp_scheduler_->num_reschedule_calls());
    }
  }
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RevokePrivateCertificates_OnContactsUploaded) {
  cert_manager_->Start();

  // Destroy and recreate private certificates if and only if the user's contact
  // list has changed since the last upload.
  size_t num_expected_calls = 0;
  for (bool did_contacts_change_since_last_upload : {true, false}) {
    contact_manager_->NotifyContactsUploaded(
        did_contacts_change_since_last_upload);
    if (did_contacts_change_since_last_upload)
      ++num_expected_calls;

    EXPECT_EQ(num_expected_calls,
              cert_store_->num_clear_private_certificates_calls());
    EXPECT_EQ(num_expected_calls,
              private_cert_exp_scheduler_->num_reschedule_calls());
  }
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_ExpiredCertificate) {
  // First certificates are expired;
  FastForward(kNearbyShareCertificateValidityPeriod * 1.5);
  cert_store_->SetPrivateCertificates(private_certificates_);

  local_device_data_manager_->SetDeviceName(
      GetNearbyShareTestMetadata().device_name());
  local_device_data_manager_->SetFullName(
      GetNearbyShareTestMetadata().full_name());
  local_device_data_manager_->SetIconUrl(
      GetNearbyShareTestMetadata().icon_url());
  SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_InvalidDeviceName) {
  cert_store_->SetPrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // Device name is missing in local device data manager.
  local_device_data_manager_->SetFullName(
      GetNearbyShareTestMetadata().full_name());
  local_device_data_manager_->SetIconUrl(
      GetNearbyShareTestMetadata().icon_url());
  SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);

  cert_manager_->Start();

  // Expect failure because a device name is required.
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/false);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_InvalidBluetoothMacAddress) {
  cert_store_->SetPrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // The bluetooth adapter returns an invalid Bluetooth MAC address.
  local_device_data_manager_->SetDeviceName(
      GetNearbyShareTestMetadata().device_name());
  local_device_data_manager_->SetFullName(
      GetNearbyShareTestMetadata().full_name());
  local_device_data_manager_->SetIconUrl(
      GetNearbyShareTestMetadata().icon_url());
  SetBluetoothMacAddress("invalid_mac_address");

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);

  // The MAC address is not set.
  nearbyshare::proto::EncryptedMetadata metadata = GetNearbyShareTestMetadata();
  metadata.clear_bluetooth_mac_address();

  VerifyPrivateCertificates(/*expected_metadata=*/metadata);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_MissingFullNameAndIconUrl) {
  cert_store_->SetPrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // Full name and icon URL are missing in local device data manager.
  local_device_data_manager_->SetDeviceName(
      GetNearbyShareTestMetadata().device_name());
  SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);

  // The full name and icon URL are not set.
  nearbyshare::proto::EncryptedMetadata metadata = GetNearbyShareTestMetadata();
  metadata.clear_full_name();
  metadata.clear_icon_url();

  VerifyPrivateCertificates(/*expected_metadata=*/metadata);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RemoveExpiredPublicCertificates_Success) {
  cert_manager_->Start();

  // The public certificate expiration scheduler notifies the certificate
  // manager that a public certificate has expired.
  EXPECT_EQ(0u, cert_store_->remove_expired_public_certificates_calls().size());
  public_cert_exp_scheduler_->InvokeRequestCallback();
  EXPECT_EQ(1u, cert_store_->remove_expired_public_certificates_calls().size());
  EXPECT_EQ(t0,
            cert_store_->remove_expired_public_certificates_calls().back().now);

  EXPECT_EQ(0u, public_cert_exp_scheduler_->handled_results().size());
  std::move(
      cert_store_->remove_expired_public_certificates_calls().back().callback)
      .Run(/*success=*/true);
  EXPECT_EQ(1u, public_cert_exp_scheduler_->handled_results().size());
  EXPECT_TRUE(public_cert_exp_scheduler_->handled_results().back());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RemoveExpiredPublicCertificates_Failue) {
  cert_manager_->Start();

  // The public certificate expiration scheduler notifies the certificate
  // manager that a public certificate has expired.
  EXPECT_EQ(0u, cert_store_->remove_expired_public_certificates_calls().size());
  public_cert_exp_scheduler_->InvokeRequestCallback();
  EXPECT_EQ(1u, cert_store_->remove_expired_public_certificates_calls().size());
  EXPECT_EQ(t0,
            cert_store_->remove_expired_public_certificates_calls().back().now);

  EXPECT_EQ(0u, public_cert_exp_scheduler_->handled_results().size());
  std::move(
      cert_store_->remove_expired_public_certificates_calls().back().callback)
      .Run(/*success=*/false);
  EXPECT_EQ(1u, public_cert_exp_scheduler_->handled_results().size());
  EXPECT_FALSE(public_cert_exp_scheduler_->handled_results().back());
}
