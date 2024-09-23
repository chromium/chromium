// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/fake_nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/fake_nearby_share_profile_info_provider.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler_factory.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::Time t0 = base::Time::UnixEpoch() + base::Days(365 * 50);

// Copied from nearby_share_certificate_manager_impl.cc.
constexpr base::TimeDelta kListPublicCertificatesTimeout = base::Seconds(30);

const char kPageTokenPrefix[] = "page_token_";
const char kSecretIdPrefix[] = "secret_id_";
const char kDeviceIdPrefix[] = "users/me/devices/";
const char kDeviceId[] = "123456789A";
const char kDefaultDeviceName[] = "Josh's Chromebook";
const char kTestProfileUserName[] = "test@google.com";
const uint8_t kVisibilityCount = 3u;

const std::vector<std::string> kPublicCertificateIds = {"id1", "id2", "id3"};

void CaptureDecryptedPublicCertificateCallback(
    std::optional<NearbyShareDecryptedPublicCertificate>* dest,
    std::optional<NearbyShareDecryptedPublicCertificate> src) {
  *dest = std::move(src);
}

// Run tests with the following feature flags enabled and disabled in
// all permutations. To add or a remove a feature you can just update this list.
const std::vector<base::test::FeatureRef> kTestFeatures = {};

}  // namespace

class NearbyShareCertificateManagerImplTest
    : public ::testing::TestWithParam<size_t>,
      public NearbyShareCertificateManager::Observer {
 public:
  NearbyShareCertificateManagerImplTest() {
    // Set time to t0.
    FastForward(t0 - base::Time::UnixEpoch());

    local_device_data_manager_ =
        std::make_unique<FakeNearbyShareLocalDeviceDataManager>(
            kDefaultDeviceName);
    local_device_data_manager_->SetId(kDeviceId);

    contact_manager_ = std::make_unique<FakeNearbyShareContactManager>();

    profile_info_provider_ =
        std::make_unique<FakeNearbyShareProfileInfoProvider>();
    profile_info_provider_->set_profile_user_name(kTestProfileUserName);

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kNearbySharingSchedulerDownloadPublicCertificatesPrefName);

    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(
        &scheduler_factory_);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(
        &cert_store_factory_);

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetAddress()).WillByDefault([this] {
      return bluetooth_mac_address_;
    });
    ON_CALL(*mock_adapter_, IsPresent()).WillByDefault([this] {
      return is_bluetooth_adapter_present_;
    });
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault([this] {
      return is_bluetooth_adapter_powered_;
    });
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    // Set default device data.
    local_device_data_manager_->SetDeviceName(
        GetNearbyShareTestMetadata().device_name());
    local_device_data_manager_->SetFullName(
        GetNearbyShareTestMetadata().full_name());
    local_device_data_manager_->SetIconUrl(
        GetNearbyShareTestMetadata().icon_url());
    SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);
  }

  ~NearbyShareCertificateManagerImplTest() override = default;

  void TearDown() override {
    cert_manager_->RemoveObserver(this);
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void InitCertificateManager(bool use_floss) {
    if (use_floss) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {floss::features::kFlossEnabled},
          /*disabled_features=*/{});
    }

    cert_manager_ = NearbyShareCertificateManagerImpl::Factory::Create(
        local_device_data_manager_.get(), contact_manager_.get(),
        profile_info_provider_.get(), pref_service_.get(),
        /*proto_database_provider=*/nullptr, base::FilePath(), &client_factory_,
        task_environment_.GetMockClock());
    cert_manager_->AddObserver(this);

    cert_store_ = cert_store_factory_.instances().back().get();

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
  }

  void SetBluetoothMacAddress(const std::string& bluetooth_mac_address) {
    bluetooth_mac_address_ = bluetooth_mac_address;
  }

  void SetBluetoothAdapterIsPresent(bool is_present) {
    is_bluetooth_adapter_present_ = is_present;
  }

  void SetBluetoothAdapterIsPowered(bool is_powered, bool notify) {
    is_bluetooth_adapter_powered_ = is_powered;

    if (notify) {
      for (auto& observer : mock_adapter_->GetObservers()) {
        observer.AdapterPoweredChanged(mock_adapter_.get(),
                                       is_bluetooth_adapter_powered_);
      }
    }
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

  void CreateFeatureList(size_t feature_mask) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Use |feature_mask| as a bitmask to decide which features in
    // |kTestFeatures| to enable or disable.
    for (size_t i = 0; i < kTestFeatures.size(); i++) {
      if (feature_mask & 1 << i) {
        enabled_features.push_back(kTestFeatures[i]);
      } else {
        disabled_features.push_back(kTestFeatures[i]);
      }
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void GetPublicCertificatesCallback(
      bool success,
      const std::vector<nearby::sharing::proto::PublicCertificate>& certs) {
    auto& callbacks = cert_store_->get_public_certificates_callbacks();
    auto callback = std::move(callbacks.back());
    callbacks.pop_back();
    auto pub_certs = std::make_unique<
        std::vector<nearby::sharing::proto::PublicCertificate>>(certs.begin(),
                                                                certs.end());
    std::move(callback).Run(success, std::move(pub_certs));
  }

  void HandlePrivateCertificateRefresh(bool expect_private_cert_refresh,
                                       bool expected_success) {
    if (expect_private_cert_refresh) {
      private_cert_exp_scheduler_->InvokeRequestCallback();
    }

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
      const nearby::sharing::proto::EncryptedMetadata& expected_metadata) {
    // Expect a full set of certificates for both all-contacts and
    // selected-contacts
    std::vector<NearbySharePrivateCertificate> certs =
        *cert_store_->GetPrivateCertificates();
    EXPECT_EQ(kVisibilityCount * kNearbyShareNumPrivateCertificates,
              certs.size());

    base::Time min_not_before_all_contacts = base::Time::Max();
    base::Time min_not_before_selected_contacts = base::Time::Max();
    base::Time min_not_before_your_devices = base::Time::Max();
    base::Time max_not_after_all_contacts = base::Time::Min();
    base::Time max_not_after_selected_contacts = base::Time::Min();
    base::Time max_not_after_your_devices = base::Time::Min();

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
        case nearby_share::mojom::Visibility::kYourDevices:
          min_not_before_your_devices =
              std::min(min_not_before_your_devices, cert.not_before());
          max_not_after_your_devices =
              std::max(max_not_after_your_devices, cert.not_after());
          break;
        default:
          NOTREACHED_IN_MIGRATION();
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
    EXPECT_EQ(kNearbyShareNumPrivateCertificates *
                  kNearbyShareCertificateValidityPeriod,
              max_not_after_your_devices - min_not_before_your_devices);
  }

  void RunUpload(bool success) {
    size_t initial_num_upload_calls =
        local_device_data_manager_->upload_certificates_calls().size();
    upload_scheduler_->InvokeRequestCallback();
    EXPECT_EQ(initial_num_upload_calls + 1,
              local_device_data_manager_->upload_certificates_calls().size());

    EXPECT_EQ(kVisibilityCount * kNearbyShareNumPrivateCertificates,
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
              .Run(ash::nearby::NearbyHttpError::kResponseMalformed);
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
      const nearby::sharing::proto::ListPublicCertificatesRequest& request,
      const std::string& page_token) {
    EXPECT_EQ(request.parent(), std::string(kDeviceIdPrefix) + kDeviceId);

    // TODO(b/168701170): One Platform has a length restriction on request URLs.
    // Adding all secret IDs to the request, and subsequently as query
    // parameters, could result in hitting this limit. Add the secret IDs of all
    // locally stored public certificates when this length restriction is
    // circumvented.
    EXPECT_TRUE(request.secret_ids().empty());

    EXPECT_EQ(request.page_token(), page_token);
  }

  nearby::sharing::proto::ListPublicCertificatesResponse BuildRpcResponse(
      size_t page_number,
      const std::string& page_token) {
    nearby::sharing::proto::ListPublicCertificatesResponse response;
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

    std::vector<nearby_share::mojom::Visibility> visibilities;
    visibilities = {nearby_share::mojom::Visibility::kAllContacts,
                    nearby_share::mojom::Visibility::kSelectedContacts,
                    nearby_share::mojom::Visibility::kYourDevices};

    for (auto visibility : visibilities) {
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
    nearby::sharing::proto::EncryptedMetadata metadata2;
    metadata2.set_device_name("device_name2");
    metadata2.set_full_name("full_name2");
    metadata2.set_icon_url("icon_url2");
    metadata2.set_bluetooth_mac_address("bluetooth_mac_address2");
    metadata2.set_account_name("account_name2");
    for (auto metadata : {metadata1, metadata2}) {
      auto private_cert = NearbySharePrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts, t0, metadata);
      public_certificates_.push_back(*private_cert.ToPublicCertificate());
      metadata_encryption_keys_.push_back(*private_cert.EncryptMetadataKey());
    }
  }

  base::HistogramTester histogram_tester_;
  raw_ptr<FakeNearbyShareCertificateStorage, DanglingUntriaged> cert_store_;
  raw_ptr<ash::nearby::FakeNearbyScheduler, DanglingUntriaged>
      private_cert_exp_scheduler_;
  raw_ptr<ash::nearby::FakeNearbyScheduler, DanglingUntriaged>
      public_cert_exp_scheduler_;
  raw_ptr<ash::nearby::FakeNearbyScheduler, DanglingUntriaged>
      upload_scheduler_;
  raw_ptr<ash::nearby::FakeNearbyScheduler, DanglingUntriaged>
      download_scheduler_;
  bool is_bluetooth_adapter_present_ = true;
  bool is_bluetooth_adapter_powered_ = true;
  std::string bluetooth_mac_address_ = kTestUnparsedBluetoothMacAddress;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  size_t num_public_certs_downloaded_notifications_ = 0;
  size_t num_private_certs_changed_notifications_ = 0;
  std::vector<NearbySharePrivateCertificate> private_certificates_;
  std::vector<nearby::sharing::proto::PublicCertificate> public_certificates_;
  std::vector<NearbyShareEncryptedMetadataKey> metadata_encryption_keys_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeNearbyShareClientFactory client_factory_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
  FakeNearbyShareCertificateStorage::Factory cert_store_factory_;
  std::unique_ptr<FakeNearbyShareLocalDeviceDataManager>
      local_device_data_manager_;
  std::unique_ptr<FakeNearbyShareContactManager> contact_manager_;
  std::unique_ptr<FakeNearbyShareProfileInfoProvider> profile_info_provider_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<NearbyShareCertificateManager> cert_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(NearbyShareCertificateManagerImplTest,
       EncryptPrivateCertificateMetadataKey) {
  InitCertificateManager(/*use_floss=*/false);
  // No valid certificates exist.
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      nearby_share::mojom::Visibility::kAllContacts));
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      nearby_share::mojom::Visibility::kSelectedContacts));

  // Set up valid all-contacts visibility certificate.
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  // Sanity check that the cert storage is as expected.
  std::optional<std::vector<NearbySharePrivateCertificate>> stored_certs =
      cert_store_->GetPrivateCertificates();
  EXPECT_EQ(stored_certs->at(0).ToDictionary(),
            private_certificate.ToDictionary());

  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      cert_manager_->EncryptPrivateCertificateMetadataKey(
          nearby_share::mojom::Visibility::kAllContacts);
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
            encrypted_metadata_key->encrypted_key());
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
            encrypted_metadata_key->salt());
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      nearby_share::mojom::Visibility::kSelectedContacts));

  // Verify that storage is updated when salts are consumed during encryption.
  EXPECT_NE(cert_store_->GetPrivateCertificates()->at(0).ToDictionary(),
            private_certificate.ToDictionary());

  // No valid certificates exist.
  FastForward(kNearbyShareCertificateValidityPeriod);
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      nearby_share::mojom::Visibility::kAllContacts));
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      nearby_share::mojom::Visibility::kSelectedContacts));
}

TEST_P(NearbyShareCertificateManagerImplTest, SignWithPrivateCertificate) {
  InitCertificateManager(/*use_floss=*/false);
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  // Perform sign/verify roundtrip.
  EXPECT_TRUE(GetNearbyShareTestDecryptedPublicCertificate().VerifySignature(
      GetNearbyShareTestPayloadToSign(),
      *cert_manager_->SignWithPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts,
          GetNearbyShareTestPayloadToSign())));

  // No selected-contact visibility certificate in storage.
  EXPECT_FALSE(cert_manager_->SignWithPrivateCertificate(
      nearby_share::mojom::Visibility::kSelectedContacts,
      GetNearbyShareTestPayloadToSign()));
}

TEST_P(NearbyShareCertificateManagerImplTest,
       HashAuthenticationTokenWithPrivateCertificate) {
  InitCertificateManager(/*use_floss=*/false);
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  EXPECT_EQ(private_certificate.HashAuthenticationToken(
                GetNearbyShareTestPayloadToSign()),
            cert_manager_->HashAuthenticationTokenWithPrivateCertificate(
                nearby_share::mojom::Visibility::kAllContacts,
                GetNearbyShareTestPayloadToSign()));

  // No selected-contact visibility certificate in storage.
  EXPECT_FALSE(cert_manager_->HashAuthenticationTokenWithPrivateCertificate(
      nearby_share::mojom::Visibility::kSelectedContacts,
      GetNearbyShareTestPayloadToSign()));
}

TEST_P(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateSuccess) {
  InitCertificateManager(/*use_floss=*/false);
  std::optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
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

TEST_P(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateCertNotFound) {
  InitCertificateManager(/*use_floss=*/false);
  auto private_cert = NearbySharePrivateCertificate(
      nearby_share::mojom::Visibility::kAllContacts, t0,
      GetNearbyShareTestMetadata());
  auto metadata_key = private_cert.EncryptMetadataKey();
  ASSERT_TRUE(metadata_key);

  std::optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      *metadata_key, base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                                    &decrypted_pub_cert));

  GetPublicCertificatesCallback(true, public_certificates_);

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateGetPublicCertificatesFailure) {
  InitCertificateManager(/*use_floss=*/false);
  std::optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                     &decrypted_pub_cert));

  GetPublicCertificatesCallback(false, {});

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesImmediateRequest) {
  InitCertificateManager(/*use_floss=*/false);
  size_t prev_num_requests = download_scheduler_->num_immediate_requests();
  cert_manager_->DownloadPublicCertificates();
  EXPECT_EQ(download_scheduler_->num_immediate_requests(),
            prev_num_requests + 1);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesSuccess) {
  InitCertificateManager(/*use_floss=*/false);
  DownloadPublicCertificatesFlow(/*num_pages=*/2,
                                 DownloadPublicCertificatesResult::kSuccess);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesTimeout) {
  InitCertificateManager(/*use_floss=*/false);
  DownloadPublicCertificatesFlow(/*num_pages=*/2,
                                 DownloadPublicCertificatesResult::kTimeout);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesRPCFailure) {
  InitCertificateManager(/*use_floss=*/false);
  DownloadPublicCertificatesFlow(/*num_pages=*/2,
                                 DownloadPublicCertificatesResult::kHttpError);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesStoreFailure) {
  InitCertificateManager(/*use_floss=*/false);
  DownloadPublicCertificatesFlow(
      /*num_pages=*/2, DownloadPublicCertificatesResult::kStorageError);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_ValidCertificates) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(private_certificates_);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/false,
                                  /*expected_success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_NoCertificates_UploadSuccess) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_NoCertificates_UploadFailure) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/false);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RevokePrivateCertificates_OnContactsUploaded) {
  InitCertificateManager(/*use_floss=*/false);
  cert_manager_->Start();

  // Destroy and recreate private certificates if contact data has changed since
  // the last successful upload.
  size_t num_expected_calls = 0;
  for (bool did_contacts_change_since_last_upload : {true, false}) {
    cert_store_->ReplacePrivateCertificates(private_certificates_);
    contact_manager_->NotifyContactsUploaded(
        did_contacts_change_since_last_upload);

    std::vector<NearbySharePrivateCertificate> certs =
        *cert_store_->GetPrivateCertificates();

    if (did_contacts_change_since_last_upload) {
      ++num_expected_calls;
      EXPECT_TRUE(certs.empty());
    } else {
      EXPECT_EQ(kVisibilityCount * kNearbyShareNumPrivateCertificates,
                certs.size());
    }

    EXPECT_EQ(num_expected_calls,
              private_cert_exp_scheduler_->num_immediate_requests());
  }
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_OnLocalDeviceMetadataChanged) {
  InitCertificateManager(/*use_floss=*/false);
  cert_manager_->Start();

  // Destroy and recreate private certificates if any metadata fields change.
  size_t num_expected_calls = 0;
  for (bool did_device_name_change : {true, false}) {
    for (bool did_full_name_change : {true, false}) {
      for (bool did_icon_change : {true, false}) {
        local_device_data_manager_->NotifyLocalDeviceDataChanged(
            did_device_name_change, did_full_name_change, did_icon_change);

        if (did_device_name_change || did_full_name_change || did_icon_change) {
          ++num_expected_calls;
          EXPECT_TRUE(cert_store_->GetPrivateCertificates()->empty());
        }

        EXPECT_EQ(num_expected_calls,
                  private_cert_exp_scheduler_->num_immediate_requests());
      }
    }
  }
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_ExpiredCertificate) {
  InitCertificateManager(/*use_floss=*/false);
  // First certificates are expired;
  FastForward(kNearbyShareCertificateValidityPeriod * 1.5);
  cert_store_->ReplacePrivateCertificates(private_certificates_);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_InvalidDeviceName) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // Device name is missing in local device data manager.
  local_device_data_manager_->SetDeviceName(std::string());

  cert_manager_->Start();

  // Expect failure because a device name is required.
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/false);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_BluetoothAdapterNotPresent) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  SetBluetoothAdapterIsPresent(false);

  cert_manager_->Start();

  // Expect failure because a Bluetooth MAC address is required.
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/false);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_InvalidBluetoothMacAddress) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // The bluetooth adapter returns an invalid Bluetooth MAC address.
  SetBluetoothMacAddress("invalid_mac_address");

  cert_manager_->Start();

  // Expect failure because a Bluetooth MAC address is required.
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/false);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_MissingFullNameAndIconUrl) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // Full name and icon URL are missing in local device data manager.
  local_device_data_manager_->SetFullName(std::nullopt);
  local_device_data_manager_->SetIconUrl(std::nullopt);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);

  // The full name and icon URL are not set.
  nearby::sharing::proto::EncryptedMetadata metadata =
      GetNearbyShareTestMetadata();
  metadata.clear_full_name();
  metadata.clear_icon_url();

  VerifyPrivateCertificates(/*expected_metadata=*/metadata);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_MissingAccountName) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // Full name and icon URL are missing in local device data manager.
  profile_info_provider_->set_profile_user_name(std::nullopt);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);

  // The account name isn't set.
  nearby::sharing::proto::EncryptedMetadata metadata =
      GetNearbyShareTestMetadata();
  metadata.clear_account_name();

  VerifyPrivateCertificates(/*expected_metadata=*/metadata);
}

TEST_P(NearbyShareCertificateManagerImplTest,
       RemoveExpiredPublicCertificates_Success) {
  InitCertificateManager(/*use_floss=*/false);
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

TEST_P(NearbyShareCertificateManagerImplTest,
       RemoveExpiredPublicCertificates_Failure) {
  InitCertificateManager(/*use_floss=*/false);
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

// Regression test b/315277593. Private certificates were refreshed when the
// Adapter was powered off, which on Floss, meant that no address was available.
// This causes failures in the Metadata generation. To resolve this, prevent
// private certificate refresh until the adapter is ready, which on Floss,
// means waiting for it to become powered on.
//
// TODO(b/331869121): Add test coverage for the case when the
// Bluetooth adapter acquisition is triggered after the private certificate
// refresh is requested. The expected behavior is that `OnGetDevice()` stores
// the pending request to fresh the private certificates until the adapter is
// acquired.
TEST_F(NearbyShareCertificateManagerImplTest,
       NoPrivateCertificateRefreshIfFlossAdapterIsNotPoweredOn) {
  InitCertificateManager(/*use_floss=*/true);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // The adapter returns an empty BT address and is powered off to simulate
  // the state of the Floss adapter being off.
  SetBluetoothMacAddress(std::string());
  SetBluetoothAdapterIsPowered(/*is_powered=*/false, /*notify=*/false);

  cert_manager_->Start();
  private_cert_exp_scheduler_->InvokeRequestCallback();

  // Expect that the certificates were not generated yet; the Adapter was not
  // ready to refresh the private certificates, so the `cert_manager_` waits
  // until the adapter is ready.
  EXPECT_EQ(0u, private_cert_exp_scheduler_->handled_results().size());
  histogram_tester_.ExpectTotalCount(
      "Nearby.Share.Certificates.Manager."
      "BluetoothMacAddressPresentForPrivateCertificateCreation",
      0);

  // Simulate the adapter being powered on, and the address being available
  // now, to replicate the behavior on Floss.
  SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);
  SetBluetoothAdapterIsPowered(/*is_powered=*/true, /*notify=*/true);

  // Expect success because the Adapter is ready now since it is powered on,
  // and the address is available.
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Share.Certificates.Manager."
      "BluetoothMacAddressPresentForPrivateCertificateCreation",
      /*bucket: success=*/true, 1);
  EXPECT_TRUE(private_cert_exp_scheduler_->handled_results().back());
  EXPECT_EQ(1u, private_cert_exp_scheduler_->handled_results().size());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       PrivateCertificateRefreshIfBlueZAdapterIsNotPoweredOn) {
  InitCertificateManager(/*use_floss=*/false);
  cert_store_->ReplacePrivateCertificates(
      std::vector<NearbySharePrivateCertificate>());

  // The adapter returns a valid BT address when powered off to simulate
  // the state of the BlueZ adapter being off.
  SetBluetoothAdapterIsPowered(/*is_powered=*/false, /*notify=*/false);

  cert_manager_->Start();
  private_cert_exp_scheduler_->InvokeRequestCallback();

  // Expect success because the `cert_manager_` does not need to wait for
  // the adapter to be on, since on BlueZ, the address remains available.
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Share.Certificates.Manager."
      "BluetoothMacAddressPresentForPrivateCertificateCreation",
      /*bucket: success=*/true, 1);
  EXPECT_TRUE(private_cert_exp_scheduler_->handled_results().back());
  EXPECT_EQ(1u, private_cert_exp_scheduler_->handled_results().size());
}

INSTANTIATE_TEST_SUITE_P(NearbyShareCertificateManagerImplTest,
                         NearbyShareCertificateManagerImplTest,
                         testing::Range<size_t>(0, 1 << kTestFeatures.size()));
