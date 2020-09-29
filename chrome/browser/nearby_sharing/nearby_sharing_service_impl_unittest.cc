// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/nearby_sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "chrome/browser/nearby_sharing/mock_nearby_process_manager.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_decoder.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_default_device_name.h"
#include "chrome/browser/nearby_sharing/power_client.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "chrome/services/sharing/nearby/decoder/advertisement_decoder.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/test_session_controller.h"
#endif  // defined(OS_CHROMEOS)

using ::testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;

using NetConnectionType = net::NetworkChangeNotifier::ConnectionType;

using SendSurfaceState = NearbySharingService::SendSurfaceState;

class FakeFastInitiationManager : public FastInitiationManager {
 public:
  explicit FakeFastInitiationManager(
      scoped_refptr<device::BluetoothAdapter> adapter,
      bool should_succeed_on_start,
      base::OnceCallback<void()> on_stop_advertising_callback,
      base::OnceCallback<void()> on_destroy_callback)
      : FastInitiationManager(adapter),
        should_succeed_on_start_(should_succeed_on_start),
        on_stop_advertising_callback_(std::move(on_stop_advertising_callback)),
        on_destroy_callback_(std::move(on_destroy_callback)) {}

  ~FakeFastInitiationManager() override {
    std::move(on_destroy_callback_).Run();
  }

  void StartAdvertising(FastInitType type,
                        base::OnceCallback<void()> callback,
                        base::OnceCallback<void()> error_callback) override {
    ++start_advertising_call_count_;
    if (should_succeed_on_start_)
      std::move(callback).Run();
    else
      std::move(error_callback).Run();
  }

  void StopAdvertising(base::OnceCallback<void()> callback) override {
    std::move(on_stop_advertising_callback_).Run();
    std::move(callback).Run();
  }

  size_t start_advertising_call_count() {
    return start_advertising_call_count_;
  }

 private:
  bool should_succeed_on_start_;
  size_t start_advertising_call_count_ = 0u;
  base::OnceCallback<void()> on_stop_advertising_callback_;
  base::OnceCallback<void()> on_destroy_callback_;
};

class FakeFastInitiationManagerFactory : public FastInitiationManager::Factory {
 public:
  explicit FakeFastInitiationManagerFactory(bool should_succeed_on_start)
      : should_succeed_on_start_(should_succeed_on_start) {}

  std::unique_ptr<FastInitiationManager> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    auto fake_fast_initiation_manager = std::make_unique<
        FakeFastInitiationManager>(
        adapter, should_succeed_on_start_,
        base::BindOnce(&FakeFastInitiationManagerFactory::OnStopAdvertising,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(
            &FakeFastInitiationManagerFactory::OnFastInitiationManagerDestroyed,
            weak_ptr_factory_.GetWeakPtr()));
    last_fake_fast_initiation_manager_ = fake_fast_initiation_manager.get();
    return std::move(fake_fast_initiation_manager);
  }

  void OnStopAdvertising() { stop_advertising_called_ = true; }

  void OnFastInitiationManagerDestroyed() {
    fast_initiation_manager_destroyed_ = true;
    last_fake_fast_initiation_manager_ = nullptr;
  }

  size_t StartAdvertisingCount() {
    return last_fake_fast_initiation_manager_
               ? last_fake_fast_initiation_manager_
                     ->start_advertising_call_count()
               : 0;
  }

  bool StopAdvertisingCalledAndManagerDestroyed() {
    return stop_advertising_called_ && fast_initiation_manager_destroyed_;
  }

 private:
  FakeFastInitiationManager* last_fake_fast_initiation_manager_ = nullptr;
  bool should_succeed_on_start_ = false;
  bool stop_advertising_called_ = false;
  bool fast_initiation_manager_destroyed_ = false;
  base::WeakPtrFactory<FakeFastInitiationManagerFactory> weak_ptr_factory_{
      this};
};

class MockTransferUpdateCallback : public TransferUpdateCallback {
 public:
  ~MockTransferUpdateCallback() override = default;

  MOCK_METHOD(void,
              OnTransferUpdate,
              (const ShareTarget& shareTarget,
               const TransferMetadata& transferMetadata),
              (override));
};

class MockShareTargetDiscoveredCallback : public ShareTargetDiscoveredCallback {
 public:
  ~MockShareTargetDiscoveredCallback() override = default;

  MOCK_METHOD(void,
              OnShareTargetDiscovered,
              (ShareTarget shareTarget),
              (override));
  MOCK_METHOD(void, OnShareTargetLost, (ShareTarget shareTarget), (override));
};

class FakePowerClient : public PowerClient {
 public:
  // Make SetSuspended() public for testing.
  using PowerClient::SetSuspended;
};

namespace {

constexpr base::TimeDelta kDelta = base::TimeDelta::FromMilliseconds(100);

const char kProfileName[] = "profile_name";
const char kServiceId[] = "NearbySharing";
const char kDeviceName[] = "test_device_name";
const nearby_share::mojom::ShareTargetType kDeviceType =
    nearby_share::mojom::ShareTargetType::kPhone;
const char kEndpointId[] = "test_endpoint_id";
const char kTextPayload[] = "Test text payload";

constexpr int64_t kFreeDiskSpace = 10000;

const std::vector<uint8_t> kValidV1EndpointInfo = {
    0, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
    0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};

const std::vector<uint8_t> kToken = {0, 1, 2};
const char kFourDigitToken[] = "1953";

const std::vector<uint8_t> kPrivateCertificateHashAuthToken = {
    0x8b, 0xcb, 0xa2, 0xf8, 0xe4, 0x06};
const std::vector<uint8_t> kIncomingConnectionSignedData = {
    0x30, 0x45, 0x02, 0x20, 0x4f, 0x83, 0x72, 0xbd, 0x02, 0x70, 0xd9, 0xda,
    0x62, 0x83, 0x5d, 0xb2, 0xdc, 0x6e, 0x3f, 0xa6, 0xa8, 0xa1, 0x4f, 0x5f,
    0xd3, 0xe3, 0xd9, 0x1a, 0x5d, 0x2d, 0x61, 0xd2, 0x6c, 0xdd, 0x8d, 0xa5,
    0x02, 0x21, 0x00, 0xd4, 0xe1, 0x1d, 0x14, 0xcb, 0x58, 0xf7, 0x02, 0xd5,
    0xab, 0x48, 0xe2, 0x2f, 0xcb, 0xc0, 0x53, 0x41, 0x06, 0x50, 0x65, 0x95,
    0x19, 0xa9, 0x22, 0x92, 0x00, 0x42, 0x01, 0x26, 0x25, 0xcb, 0x8c};
const std::vector<uint8_t> kOutgoingConnectionSignedData = {
    0x30, 0x45, 0x02, 0x21, 0x00, 0xf9, 0xc9, 0xa8, 0x89, 0x96, 0x6e, 0x5c,
    0xea, 0x0a, 0x60, 0x37, 0x3a, 0x84, 0x7d, 0xf5, 0x31, 0x82, 0x74, 0xb9,
    0xde, 0x3f, 0x64, 0x1b, 0xff, 0x4f, 0x54, 0x31, 0x1f, 0x9e, 0x63, 0x68,
    0xca, 0x02, 0x20, 0x52, 0x43, 0x46, 0xa7, 0x6f, 0xcb, 0x96, 0x50, 0x86,
    0xfd, 0x6f, 0x9f, 0x7e, 0x50, 0xa7, 0xa0, 0x9b, 0xdf, 0xae, 0x79, 0x42,
    0x47, 0xd9, 0x60, 0x71, 0x91, 0x7a, 0xbb, 0x81, 0x9b, 0x0d, 0x2e};

constexpr int kFilePayloadId = 111;
constexpr int kPayloadSize = 1;

const std::vector<int64_t> kValidIntroductionFramePayloadIds = {1, 2, 3,
                                                                kFilePayloadId};

bool FileExists(const base::FilePath& file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return base::PathExists(file_path);
}

location::nearby::connections::mojom::PayloadPtr GetFilePayloadPtr(
    int64_t payload_id) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  base::File file(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                            base::File::Flags::FLAG_READ |
                            base::File::Flags::FLAG_WRITE);

  return location::nearby::connections::mojom::Payload::New(
      payload_id, location::nearby::connections::mojom::PayloadContent::NewFile(
                      location::nearby::connections::mojom::FilePayload::New(
                          std::move(file))));
}

location::nearby::connections::mojom::PayloadPtr GetTextPayloadPtr(
    int64_t payload_id,
    const std::string& text) {
  return location::nearby::connections::mojom::Payload::New(
      payload_id,
      location::nearby::connections::mojom::PayloadContent::NewBytes(
          location::nearby::connections::mojom::BytesPayload::New(
              std::vector<uint8_t>(text.begin(), text.end()))));
}

sharing::mojom::FramePtr GetValidIntroductionFrame() {
  std::vector<sharing::mojom::TextMetadataPtr> mojo_text_metadatas;
  // TODO(crbug.com/1123168) - Parameterise number of text and file metadatas.
  for (int i = 1; i <= 3; i++) {
    mojo_text_metadatas.push_back(sharing::mojom::TextMetadata::New(
        "title " + base::NumberToString(i),
        static_cast<sharing::mojom::TextMetadata::Type>(i), /*payload_id=*/i,
        kPayloadSize, /*id=*/i));
  }

  std::vector<sharing::mojom::FileMetadataPtr> mojo_file_metadatas;
  mojo_file_metadatas.push_back(sharing::mojom::FileMetadata::New(
      "unit_test_nearby_share_name", sharing::mojom::FileMetadata::Type::kVideo,
      kFilePayloadId, kPayloadSize, "mime type",
      /*id=*/100));

  sharing::mojom::V1FramePtr mojo_v1frame = sharing::mojom::V1Frame::New();
  mojo_v1frame->set_introduction(sharing::mojom::IntroductionFrame::New(
      std::move(mojo_file_metadatas), std::move(mojo_text_metadatas),
      /*required_package=*/base::nullopt,
      std::vector<sharing::mojom::WifiCredentialsMetadataPtr>()));

  sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
  mojo_frame->set_v1(std::move(mojo_v1frame));
  return mojo_frame;
}

sharing::mojom::FramePtr GetEmptyIntroductionFrame() {
  sharing::mojom::V1FramePtr mojo_v1frame = sharing::mojom::V1Frame::New();
  mojo_v1frame->set_introduction(sharing::mojom::IntroductionFrame::New());

  sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
  mojo_frame->set_v1(std::move(mojo_v1frame));
  return mojo_frame;
}

sharing::mojom::FramePtr GetConnectionResponseFrame(
    sharing::mojom::ConnectionResponseFrame::Status status) {
  sharing::mojom::V1FramePtr mojo_v1frame = sharing::mojom::V1Frame::New();
  mojo_v1frame->set_connection_response(
      sharing::mojom::ConnectionResponseFrame::New(status));

  sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
  mojo_frame->set_v1(std::move(mojo_v1frame));
  return mojo_frame;
}

std::vector<std::unique_ptr<Attachment>> CreateTextAttachments(
    std::vector<std::string> texts) {
  std::vector<std::unique_ptr<Attachment>> attachments;
  for (auto& text : texts) {
    attachments.push_back(std::make_unique<TextAttachment>(
        TextAttachment::Type::kText, std::move(text)));
  }
  return attachments;
}

std::vector<std::unique_ptr<Attachment>> CreateFileAttachments(
    std::vector<base::FilePath> file_paths) {
  std::vector<std::unique_ptr<Attachment>> attachments;
  for (auto& file_path : file_paths) {
    attachments.push_back(
        std::make_unique<FileAttachment>(std::move(file_path)));
  }
  return attachments;
}

class NearbySharingServiceImplTest : public testing::Test {
 public:
  NearbySharingServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kNearbySharing);
    RegisterNearbySharingPrefs(prefs_.registry());
  }

  ~NearbySharingServiceImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp());
    network_notifier_ = net::test::MockNetworkChangeNotifier::Create();

    NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
        &local_device_data_manager_factory_);
    NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(
        &contact_manager_factory_);
    NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(
        &certificate_manager_factory_);

    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(
            Invoke(this, &NearbySharingServiceImplTest::IsBluetoothPresent));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered())
        .WillByDefault(
            Invoke(this, &NearbySharingServiceImplTest::IsBluetoothPowered));
    ON_CALL(*mock_bluetooth_adapter_, AddObserver(_))
        .WillByDefault(
            Invoke(this, &NearbySharingServiceImplTest::AddAdapterObserver));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

#if defined(OS_CHROMEOS)
    session_controller_ = std::make_unique<TestSessionController>();
#endif  // OS_CHROMEOS

    service_ = CreateService();
    SetFakeFastInitiationManagerFactory(/*should_succeed_on_start=*/true);

    EXPECT_CALL(mock_nearby_process_manager(),
                GetOrStartNearbySharingDecoder(testing::_))
        .WillRepeatedly(testing::Return(&mock_decoder_));

    service_->set_free_disk_space_for_testing(kFreeDiskSpace);

    // From now on we don't allow any blocking tasks anymore.
    disallow_blocking_ = std::make_unique<base::ScopedDisallowBlocking>();
  }

  void TearDown() override {
    if (service_)
      service_->Shutdown();

    if (profile_) {
      DownloadCoreServiceFactory::GetForBrowserContext(profile_)
          ->SetDownloadManagerDelegateForTesting(nullptr);
      profile_ = nullptr;
    }

    profile_manager_.DeleteAllTestingProfiles();

    NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
        nullptr);
    NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(nullptr);
    NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(nullptr);
    FastInitiationManager::Factory::SetFactoryForTesting(nullptr);
  }

  std::unique_ptr<NearbySharingServiceImpl> CreateService() {
    profile_ = profile_manager_.CreateTestingProfile(kProfileName);

    fake_nearby_connections_manager_ = new FakeNearbyConnectionsManager();
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
    NotificationDisplayService* notification_display_service =
        NotificationDisplayServiceFactory::GetForProfile(profile_);
    auto power_client = std::make_unique<FakePowerClient>();
    power_client_ = power_client.get();
    auto service = std::make_unique<NearbySharingServiceImpl>(
        &prefs_, notification_display_service, profile_,
        base::WrapUnique(fake_nearby_connections_manager_),
        &mock_nearby_process_manager_, std::move(power_client));
    ON_CALL(mock_nearby_process_manager_, IsActiveProfile(profile_))
        .WillByDefault(Return(true));

    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile_));

    // Allow the posted tasks to fetch the BluetoothAdapter and set the default
    // device name to finish.
    task_environment_.RunUntilIdle();

    return service;
  }

  void SetVisibility(nearby_share::mojom::Visibility visibility) {
    NearbyShareSettings settings(&prefs_, local_device_data_manager());
    settings.SetVisibility(visibility);
  }

  void SetFakeFastInitiationManagerFactory(bool should_succeed_on_start) {
    fast_initiation_manager_factory_ =
        std::make_unique<FakeFastInitiationManagerFactory>(
            should_succeed_on_start);
    FastInitiationManager::Factory::SetFactoryForTesting(
        fast_initiation_manager_factory_.get());
  }

  bool IsBluetoothPresent() { return is_bluetooth_present_; }
  bool IsBluetoothPowered() { return is_bluetooth_powered_; }

  void SetBluetoothIsPresent(bool present) {
    is_bluetooth_present_ = present;
    adapter_observer_->AdapterPresentChanged(mock_bluetooth_adapter_.get(),
                                             present);
  }

  void SetBluetoothIsPowered(bool powered) {
    is_bluetooth_powered_ = powered;
    adapter_observer_->AdapterPoweredChanged(mock_bluetooth_adapter_.get(),
                                             powered);
  }

  void AddAdapterObserver(device::BluetoothAdapter::Observer* observer) {
    DCHECK(!adapter_observer_);
    adapter_observer_ = observer;
  }

  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    network_notifier_->SetConnectionType(type);
    network_notifier_->NotifyObserversOfNetworkChangeForTests(
        network_notifier_->GetConnectionType());
  }

  NiceMock<MockNearbyProcessManager>& mock_nearby_process_manager() {
    return mock_nearby_process_manager_;
  }

  void SetUpForegroundReceiveSurface(
      NiceMock<MockTransferUpdateCallback>& callback) {
    NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
        &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
    EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
    EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  }

  void ProcessLatestPublicCertificateDecryption(size_t expected_num_calls,
                                                bool success) {
    std::vector<
        FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall>&
        calls = certificate_manager()->get_decrypted_public_certificate_calls();

    ASSERT_FALSE(calls.empty());
    EXPECT_EQ(expected_num_calls, calls.size());
    EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
              calls.back().encrypted_metadata_key.salt());
    EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
              calls.back().encrypted_metadata_key.encrypted_key());

    if (success) {
      std::move(calls.back().callback)
          .Run(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              GetNearbyShareTestPublicCertificate(
                  nearby_share::mojom::Visibility::kAllContacts),
              GetNearbyShareTestEncryptedMetadataKey()));
    } else {
      std::move(calls.back().callback).Run(base::nullopt);
    }
  }

  void SetUpKeyVerification(
      bool is_incoming,
      sharing::mojom::PairedKeyResultFrame::Status status) {
    SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
    local_device_data_manager()->SetDeviceName(kDeviceName);

    std::string encryption_frame = "test_encryption_frame";
    std::vector<uint8_t> encryption_bytes(encryption_frame.begin(),
                                          encryption_frame.end());
    EXPECT_CALL(mock_decoder_,
                DecodeFrame(testing::Eq(encryption_bytes), testing::_))
        .WillOnce(testing::Invoke(
            [is_incoming](
                const std::vector<uint8_t>& data,
                MockNearbySharingDecoder::DecodeFrameCallback callback) {
              sharing::mojom::V1FramePtr mojo_v1frame =
                  sharing::mojom::V1Frame::New();
              mojo_v1frame->set_paired_key_encryption(
                  sharing::mojom::PairedKeyEncryptionFrame::New(
                      is_incoming ? kIncomingConnectionSignedData
                                  : kOutgoingConnectionSignedData,
                      kPrivateCertificateHashAuthToken));
              sharing::mojom::FramePtr mojo_frame =
                  sharing::mojom::Frame::New();
              mojo_frame->set_v1(std::move(mojo_v1frame));
              std::move(callback).Run(std::move(mojo_frame));
            }));
    connection_.AppendReadableData(encryption_bytes);

    std::string encryption_result = "test_encryption_result";
    std::vector<uint8_t> result_bytes(encryption_result.begin(),
                                      encryption_result.end());
    EXPECT_CALL(mock_decoder_,
                DecodeFrame(testing::Eq(result_bytes), testing::_))
        .WillOnce(testing::Invoke(
            [=](const std::vector<uint8_t>& data,
                MockNearbySharingDecoder::DecodeFrameCallback callback) {
              sharing::mojom::V1FramePtr mojo_v1frame =
                  sharing::mojom::V1Frame::New();
              mojo_v1frame->set_paired_key_result(
                  sharing::mojom::PairedKeyResultFrame::New(status));

              sharing::mojom::FramePtr mojo_frame =
                  sharing::mojom::Frame::New();
              mojo_frame->set_v1(std::move(mojo_v1frame));
              std::move(callback).Run(std::move(mojo_frame));
            }));
    connection_.AppendReadableData(result_bytes);
  }

  void SetUpAdvertisementDecoder(const std::vector<uint8_t>& endpoint_info,
                                 bool return_empty_advertisement) {
    EXPECT_CALL(mock_decoder_,
                DecodeAdvertisement(testing::Eq(endpoint_info), testing::_))
        .WillOnce(testing::Invoke(
            [=](const std::vector<uint8_t>& data,
                MockNearbySharingDecoder::DecodeAdvertisementCallback
                    callback) {
              if (return_empty_advertisement) {
                std::move(callback).Run(nullptr);
                return;
              }

              sharing::mojom::AdvertisementPtr advertisement =
                  sharing::mojom::Advertisement::New(
                      GetNearbyShareTestEncryptedMetadataKey().salt(),
                      GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
                      kDeviceType, kDeviceName);
              std::move(callback).Run(std::move(advertisement));
            }));
  }

  void SetUpIntroductionFrameDecoder(bool return_empty_introduction_frame) {
    std::string intro = "introduction_frame";
    std::vector<uint8_t> bytes(intro.begin(), intro.end());
    EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
        .WillOnce(testing::Invoke(
            [=](const std::vector<uint8_t>& data,
                MockNearbySharingDecoder::DecodeFrameCallback callback) {
              std::move(callback).Run(return_empty_introduction_frame
                                          ? GetEmptyIntroductionFrame()
                                          : GetValidIntroductionFrame());
            }));
    connection_.AppendReadableData(bytes);
  }

  void SendConnectionResponse(
      sharing::mojom::ConnectionResponseFrame::Status status) {
    std::string intro = "connection_result_frame";
    std::vector<uint8_t> bytes(intro.begin(), intro.end());
    EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
        .WillOnce(testing::Invoke(
            [=](const std::vector<uint8_t>& data,
                MockNearbySharingDecoder::DecodeFrameCallback callback) {
              std::move(callback).Run(GetConnectionResponseFrame(status));
            }));
    connection_.AppendReadableData(bytes);
  }

  ShareTarget SetUpIncomingConnection(
      NiceMock<MockTransferUpdateCallback>& callback) {
    fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                                kToken);
    SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                              /*return_empty_advertisement=*/false);
    SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

    ShareTarget share_target;
    SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
    base::RunLoop run_loop;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& incoming_share_target,
                                      TransferMetadata metadata) {
          EXPECT_FALSE(metadata.is_final_status());
          EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                    metadata.status());
          share_target = incoming_share_target;
          run_loop.Quit();
        }));

    SetUpKeyVerification(/*is_incoming=*/true,
                         sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
    SetUpForegroundReceiveSurface(callback);
    service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                   &connection_);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    run_loop.Run();

    EXPECT_TRUE(
        fake_nearby_connections_manager_->DidUpgradeBandwidth(kEndpointId));

    return share_target;
  }

  ShareTarget SetUpOutgoingShareTarget(
      MockTransferUpdateCallback& transfer_callback,
      MockShareTargetDiscoveredCallback& discovery_callback) {
    SetUpKeyVerification(/*is_incoming=*/false,
                         sharing::mojom::PairedKeyResultFrame_Status::kSuccess);

    fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                                kToken);
    fake_nearby_connections_manager_->set_nearby_connection(&connection_);

    return DiscoverShareTarget(transfer_callback, discovery_callback);
  }

  ShareTarget DiscoverShareTarget(
      MockTransferUpdateCallback& transfer_callback,
      MockShareTargetDiscoveredCallback& discovery_callback) {
    SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

    // Ensure decoder parses a valid endpoint advertisement.
    SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                              /*return_empty_advertisement=*/false);

    // Start discovering, to ensure a discovery listener is registered.
    base::RunLoop run_loop;
    EXPECT_EQ(
        NearbySharingService::StatusCodes::kOk,
        service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                      SendSurfaceState::kForeground));
    EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

    ShareTarget discovered_target;
    // Discover a new endpoint, with fields set up a valid certificate.
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([&run_loop, &discovered_target](ShareTarget share_target) {
          discovered_target = share_target;
          run_loop.Quit();
        });
    fake_nearby_connections_manager_->OnEndpointFound(
        kEndpointId,
        location::nearby::connections::mojom::DiscoveredEndpointInfo::New(
            kValidV1EndpointInfo, kServiceId));
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    run_loop.Run();
    return discovered_target;
  }

  sharing::nearby::Frame GetWrittenFrame() {
    std::vector<uint8_t> data = connection_.GetWrittenData();
    sharing::nearby::Frame frame;
    frame.ParseFromArray(data.data(), data.size());
    return frame;
  }

  void ExpectPairedKeyEncryptionFrame() {
    sharing::nearby::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_encryption());
  }

  void ExpectPairedKeyResultFrame() {
    sharing::nearby::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_result());
  }

  void ExpectConnectionResponseFrame(
      sharing::nearby::ConnectionResponseFrame::Status status) {
    sharing::nearby::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_connection_response());
    EXPECT_EQ(status, frame.v1().connection_response().status());
  }

  sharing::nearby::IntroductionFrame ExpectIntroductionFrame() {
    sharing::nearby::Frame frame = GetWrittenFrame();
    EXPECT_TRUE(frame.has_v1());
    EXPECT_TRUE(frame.v1().has_introduction());
    return frame.v1().introduction();
  }

  void ExpectTransferUpdates(
      MockTransferUpdateCallback& transfer_callback,
      const ShareTarget& target,
      const std::vector<TransferMetadata::Status>& updates,
      base::OnceClosure callback) {
    auto barrier = base::BarrierClosure(updates.size(), std::move(callback));
    auto& expectation =
        EXPECT_CALL(transfer_callback, OnTransferUpdate).Times(updates.size());
    for (TransferMetadata::Status status : updates) {
      expectation.WillOnce(testing::Invoke(
          [=](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_EQ(target.id, share_target.id);
            EXPECT_EQ(status, metadata.status());
            barrier.Run();
          }));
    }
  }

  void SetUpOutgoingConnectionUntilAccept(
      MockTransferUpdateCallback& transfer_callback,
      const ShareTarget& target) {
    base::RunLoop introduction_run_loop;
    ExpectTransferUpdates(transfer_callback, target,
                          {TransferMetadata::Status::kConnecting,
                           TransferMetadata::Status::kAwaitingLocalConfirmation,
                           TransferMetadata::Status::kAwaitingRemoteAcceptance},
                          introduction_run_loop.QuitClosure());

    EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
              service_->SendAttachments(target,
                                        CreateTextAttachments({kTextPayload})));
    introduction_run_loop.Run();

    // Verify data sent to the remote device so far.
    ExpectPairedKeyEncryptionFrame();
    ExpectPairedKeyResultFrame();
    ExpectIntroductionFrame();
  }

  struct PayloadInfo {
    int64_t payload_id;
    NearbyConnectionsManager::PayloadStatusListener* listener;
  };

  PayloadInfo AcceptAndSendPayload(
      MockTransferUpdateCallback& transfer_callback,
      const ShareTarget& target) {
    PayloadInfo info = {};
    base::RunLoop payload_run_loop;
    fake_nearby_connections_manager_->set_send_payload_callback(
        base::BindLambdaForTesting(
            [&](NearbyConnectionsManager::PayloadPtr payload,
                NearbyConnectionsManager::PayloadStatusListener* listener) {
              ASSERT_TRUE(payload->content->is_bytes());
              std::vector<uint8_t> bytes = payload->content->get_bytes()->bytes;
              EXPECT_EQ(kTextPayload, std::string(bytes.begin(), bytes.end()));
              info.payload_id = payload->id;
              info.listener = listener;
              payload_run_loop.Quit();
            }));

    // We're now waiting for the remote device to respond with the accept
    // result.
    base::RunLoop accept_run_loop;
    ExpectTransferUpdates(transfer_callback, target,
                          {TransferMetadata::Status::kInProgress},
                          accept_run_loop.QuitClosure());

    // Kick off send process by accepting the transfer from the remote device.
    SendConnectionResponse(
        sharing::mojom::ConnectionResponseFrame::Status::kAccept);

    accept_run_loop.Run();
    payload_run_loop.Run();
    return info;
  }

  void FinishOutgoingTransfer(MockTransferUpdateCallback& transfer_callback,
                              const ShareTarget& target,
                              const PayloadInfo& info) {
    // Simulate a successful transfer via Nearby Connections.
    base::RunLoop success_run_loop;
    ExpectTransferUpdates(transfer_callback, target,
                          {TransferMetadata::Status::kComplete},
                          success_run_loop.QuitClosure());
    ASSERT_TRUE(info.listener);
    info.listener->OnStatusUpdate(
        location::nearby::connections::mojom::PayloadTransferUpdate::New(
            info.payload_id,
            location::nearby::connections::mojom::PayloadStatus::kSuccess,
            /*total_bytes=*/strlen(kTextPayload),
            /*bytes_transferred=*/strlen(kTextPayload)));
    success_run_loop.Run();
  }

  std::unique_ptr<sharing::Advertisement> GetCurrentAdvertisement() {
    auto endpoint_info =
        fake_nearby_connections_manager_->advertising_endpoint_info();
    if (!endpoint_info)
      return nullptr;

    return sharing::AdvertisementDecoder::FromEndpointInfo(base::make_span(
        *fake_nearby_connections_manager_->advertising_endpoint_info()));
  }

 protected:
  FakeNearbyShareLocalDeviceDataManager* local_device_data_manager() {
    EXPECT_EQ(1u, local_device_data_manager_factory_.instances().size());
    return local_device_data_manager_factory_.instances().back();
  }

  FakeNearbyShareContactManager* contact_manager() {
    EXPECT_EQ(1u, contact_manager_factory_.instances().size());
    return contact_manager_factory_.instances().back();
  }

  FakeNearbyShareCertificateManager* certificate_manager() {
    EXPECT_EQ(1u, certificate_manager_factory_.instances().size());
    return certificate_manager_factory_.instances().back();
  }

  base::FilePath CreateTestFile(const std::string& name,
                                const std::vector<uint8_t>& content) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path = temp_dir_.GetPath().AppendASCII(name);
    base::File file(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                              base::File::Flags::FLAG_READ |
                              base::File::Flags::FLAG_WRITE);
    EXPECT_TRUE(file.WriteAndCheck(
        /*offset=*/0, base::make_span(content)));
    EXPECT_TRUE(file.Flush());
    file.Close();
    return path;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  // We need to ensure that |network_notifier_| is created and destroyed after
  // |task_environment_| to avoid UAF issues when using
  // ChromeDownloadManagerDelegate.
  std::unique_ptr<net::test::MockNetworkChangeNotifier> network_notifier_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  Profile* profile_ = nullptr;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeNearbyConnectionsManager* fake_nearby_connections_manager_ = nullptr;
  FakePowerClient* power_client_ = nullptr;
  FakeNearbyShareLocalDeviceDataManager::Factory
      local_device_data_manager_factory_;
  FakeNearbyShareContactManager::Factory contact_manager_factory_;
  FakeNearbyShareCertificateManager::Factory certificate_manager_factory_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  NiceMock<MockNearbyProcessManager> mock_nearby_process_manager_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<TestSessionController> session_controller_;
#endif  // OS_CHROMEOS
  std::unique_ptr<NearbySharingServiceImpl> service_;
  std::unique_ptr<base::ScopedDisallowBlocking> disallow_blocking_;
  std::unique_ptr<FakeFastInitiationManagerFactory>
      fast_initiation_manager_factory_;
  bool is_bluetooth_present_ = true;
  bool is_bluetooth_powered_ = true;
  device::BluetoothAdapter::Observer* adapter_observer_ = nullptr;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  NiceMock<MockNearbySharingDecoder> mock_decoder_;
  FakeNearbyConnection connection_;
};

struct ValidSendSurfaceTestData {
  bool bluetooth_enabled;
  net::NetworkChangeNotifier::ConnectionType connection_type;
} kValidSendSurfaceTestData[] = {
    // No network connection, only bluetooth available
    {true, net::NetworkChangeNotifier::CONNECTION_NONE},
    // Wifi available
    {true, net::NetworkChangeNotifier::CONNECTION_WIFI},
    // Ethernet available
    {true, net::NetworkChangeNotifier::CONNECTION_ETHERNET},
    // 3G available
    {true, net::NetworkChangeNotifier::CONNECTION_3G}};

class NearbySharingServiceImplValidSendTest
    : public NearbySharingServiceImplTest,
      public testing::WithParamInterface<ValidSendSurfaceTestData> {};

struct InvalidSendSurfaceTestData {
  bool screen_locked;
  bool bluetooth_enabled;
  net::NetworkChangeNotifier::ConnectionType connection_type;
} kInvalidSendSurfaceTestData[] = {
#if defined(OS_CHROMEOS)
    // Screen locked
    {/*screen_locked=*/true, true, net::NetworkChangeNotifier::CONNECTION_WIFI},
#endif  // OS_CHROMEOS
    // No network connection and no bluetooth
    {/*screen_locked=*/false, false,
     net::NetworkChangeNotifier::CONNECTION_NONE},
    // 3G available and no bluetooth
    {/*screen_locked=*/false, false, net::NetworkChangeNotifier::CONNECTION_3G},
    // Wifi available and no bluetooth (invalid until WiFi LAN is supported)
    {/*screen_locked=*/false, false,
     net::NetworkChangeNotifier::CONNECTION_WIFI},
    // Ethernet available and no bluetooth (invalid until WiFi LAN is supported)
    {/*screen_locked=*/false, false,
     net::NetworkChangeNotifier::CONNECTION_ETHERNET}};

class NearbySharingServiceImplInvalidSendTest
    : public NearbySharingServiceImplTest,
      public testing::WithParamInterface<InvalidSendSurfaceTestData> {};

using ResponseFrameStatus = sharing::mojom::ConnectionResponseFrame::Status;

struct SendFailureTestData {
  ResponseFrameStatus response_status;
  TransferMetadata::Status expected_status;
} kSendFailureTestData[] = {
    {ResponseFrameStatus::kReject, TransferMetadata::Status::kRejected},
    {ResponseFrameStatus::kNotEnoughSpace,
     TransferMetadata::Status::kNotEnoughSpace},
    {ResponseFrameStatus::kUnsupportedAttachmentType,
     TransferMetadata::Status::kUnsupportedAttachmentType},
    {ResponseFrameStatus::kTimedOut, TransferMetadata::Status::kTimedOut},
    {ResponseFrameStatus::kUnknown, TransferMetadata::Status::kFailed},
};

class NearbySharingServiceImplSendFailureTest
    : public NearbySharingServiceImplTest,
      public testing::WithParamInterface<SendFailureTestData> {};

class TestObserver : public NearbySharingService::Observer {
 public:
  explicit TestObserver(NearbySharingService* service) : service_(service) {
    service_->AddObserver(this);
  }

  void OnHighVisibilityChanged(bool in_high_visibility) override {
    in_high_visibility_ = in_high_visibility;
  }

  void OnShutdown() override {
    shutdown_called_ = true;
    service_->RemoveObserver(this);
  }

  bool in_high_visibility_ = false;
  bool shutdown_called_ = false;
  NearbySharingService* service_;
};

}  // namespace

TEST_F(NearbySharingServiceImplTest, AddsNearbyProcessObserver) {
  EXPECT_TRUE(
      mock_nearby_process_manager().observers_.HasObserver(service_.get()));
}

TEST_F(NearbySharingServiceImplTest, RemovesNearbyProcessObserver) {
  service_->Shutdown();
  service_.reset();
  EXPECT_FALSE(mock_nearby_process_manager().observers_.might_have_observers());
}

TEST_F(NearbySharingServiceImplTest, DisableNearbyShutdownConnections) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);
  service_->FlushMojoForTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, StartFastInitiationAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_EQ(1u, fast_initiation_manager_factory_->StartAdvertisingCount());

  // Call RegisterSendSurface a second time and make sure StartAdvertising is
  // not called again.
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kError,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_EQ(1u, fast_initiation_manager_factory_->StartAdvertisingCount());
}

TEST_F(NearbySharingServiceImplTest, StartFastInitiationAdvertisingError) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetFakeFastInitiationManagerFactory(/*should_succeed_on_start=*/false);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundStartFastInitiationAdvertisingError) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kBackground));
  EXPECT_EQ(0u, fast_initiation_manager_factory_->StartAdvertisingCount());
}

TEST_F(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPresent) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  is_bluetooth_present_ = false;
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
}

TEST_F(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPowered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  is_bluetooth_powered_ = false;
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
}

TEST_F(NearbySharingServiceImplTest, StopFastInitiationAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_EQ(1u, fast_initiation_manager_factory_->StartAdvertisingCount());
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->UnregisterSendSurface(&transfer_callback, &discovery_callback));
  EXPECT_TRUE(fast_initiation_manager_factory_
                  ->StopAdvertisingCalledAndManagerDestroyed());
}

TEST_F(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPresent) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  SetBluetoothIsPresent(false);
  EXPECT_TRUE(fast_initiation_manager_factory_
                  ->StopAdvertisingCalledAndManagerDestroyed());
}

TEST_F(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPowered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  SetBluetoothIsPowered(false);
  EXPECT_TRUE(fast_initiation_manager_factory_
                  ->StopAdvertisingCalledAndManagerDestroyed());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterSendSurfaceNoActiveProfilesNotDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  ON_CALL(mock_nearby_process_manager_, IsActiveProfile(_))
      .WillByDefault(Return(false));
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, RegisterSendSurface_BluetoothNotPresent) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  is_bluetooth_present_ = false;
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest, RegisterSendSurface_BluetoothNotPowered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  is_bluetooth_powered_ = false;
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterSendSurfaceStartsDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterSendSurfaceTwiceKeepsDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  EXPECT_EQ(
      NearbySharingService::StatusCodes::kError,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       BluetoothBecomesNotPresentStopDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  SetBluetoothIsPresent(false);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  SetBluetoothIsPresent(true);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       BluetoothBecomesNotPoweredStopDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  SetBluetoothIsPowered(false);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  SetBluetoothIsPowered(true);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterSendSurfaceAlreadyReceivingNotDiscovering) {
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);
  EXPECT_FALSE(connection_.IsClosed());

  MockTransferUpdateCallback send_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(NearbySharingService::StatusCodes::kTransferAlreadyInProgress,
            service_->RegisterSendSurface(&send_callback, &discovery_callback,
                                          SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundRegisterSendSurfaceNotDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kBackground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       DifferentSurfaceRegisterSendSurfaceTwiceKeepsDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  EXPECT_EQ(
      NearbySharingService::StatusCodes::kError,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kBackground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterSendSurfaceEndpointFoundDiscoveryCallbackNotified) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Ensure decoder parses a valid endpoint advertisement.
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);

  // Start discovering, to ensure a discovery listener is registered.
  base::RunLoop run_loop;
  MockTransferUpdateCallback transfer_callback;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Discover a new endpoint, with fields set up a valid certificate.
  EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
      .WillOnce([&run_loop](ShareTarget share_target) {
        EXPECT_FALSE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_FALSE(share_target.has_attachments());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);
        run_loop.Quit();
      });
  fake_nearby_connections_manager_->OnEndpointFound(
      kEndpointId,
      location::nearby::connections::mojom::DiscoveredEndpointInfo::New(
          kValidV1EndpointInfo, kServiceId));
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // Register another send surface, which will automatically catch up discovered
  // endpoints.
  base::RunLoop run_loop2;
  MockTransferUpdateCallback transfer_callback2;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback2;
  EXPECT_CALL(discovery_callback2, OnShareTargetDiscovered)
      .WillOnce([&run_loop2](ShareTarget share_target) {
        EXPECT_EQ(kDeviceName, share_target.device_name);
        run_loop2.Quit();
      });

  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback2, &discovery_callback2,
                                    SendSurfaceState::kForeground));
  run_loop2.Run();
}

TEST_F(NearbySharingServiceImplTest, RegisterSendSurfaceEmptyCertificate) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Ensure decoder parses a valid endpoint advertisement.
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);

  // Start discovering, to ensure a discovery listener is registered.
  base::RunLoop run_loop;
  MockTransferUpdateCallback transfer_callback;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Discover a new endpoint, with fields set up a valid certificate.
  EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
      .WillOnce([&run_loop](ShareTarget share_target) {
        EXPECT_FALSE(share_target.is_incoming);
        EXPECT_FALSE(share_target.is_known);
        EXPECT_FALSE(share_target.has_attachments());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_FALSE(share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_EQ(kEndpointId, share_target.device_id);
        EXPECT_FALSE(share_target.full_name);
        run_loop.Quit();
      });
  fake_nearby_connections_manager_->OnEndpointFound(
      kEndpointId,
      location::nearby::connections::mojom::DiscoveredEndpointInfo::New(
          kValidV1EndpointInfo, kServiceId));
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  run_loop.Run();

  // Register another send surface, which will automatically catch up discovered
  // endpoints.
  base::RunLoop run_loop2;
  MockTransferUpdateCallback transfer_callback2;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback2;
  EXPECT_CALL(discovery_callback2, OnShareTargetDiscovered)
      .WillOnce([&run_loop2](ShareTarget share_target) {
        EXPECT_EQ(kDeviceName, share_target.device_name);
        run_loop2.Quit();
      });

  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback2, &discovery_callback2,
                                    SendSurfaceState::kForeground));
  run_loop2.Run();
}

TEST_P(NearbySharingServiceImplValidSendTest,
       RegisterSendSurfaceIsDiscovering) {
  is_bluetooth_present_ = GetParam().bluetooth_enabled;
  SetConnectionType(GetParam().connection_type);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

INSTANTIATE_TEST_SUITE_P(NearbySharingServiceImplTest,
                         NearbySharingServiceImplValidSendTest,
                         testing::ValuesIn(kValidSendSurfaceTestData));

TEST_P(NearbySharingServiceImplInvalidSendTest,
       RegisterSendSurfaceNotDiscovering) {
#if defined(OS_CHROMEOS)
  session_controller_->SetScreenLocked(GetParam().screen_locked);
#endif  // OS_CHROMEOS
  is_bluetooth_present_ = GetParam().bluetooth_enabled;
  SetConnectionType(GetParam().connection_type);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

INSTANTIATE_TEST_SUITE_P(NearbySharingServiceImplTest,
                         NearbySharingServiceImplInvalidSendTest,
                         testing::ValuesIn(kInvalidSendSurfaceTestData));

TEST_F(NearbySharingServiceImplTest, DisableFeatureSendSurfaceNotDiscovering) {
  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);
  service_->FlushMojoForTesting();
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_TRUE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       DisableFeatureSendSurfaceStopsDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);
  service_->FlushMojoForTesting();
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_TRUE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, UnregisterSendSurfaceStopsDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  EXPECT_CALL(mock_nearby_process_manager(), StopProcess(profile_))
      .Times(AtLeast(1));
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->UnregisterSendSurface(&transfer_callback, &discovery_callback));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       UnregisterSendSurfaceDifferentCallbackKeepDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  MockTransferUpdateCallback transfer_callback2;
  MockShareTargetDiscoveredCallback discovery_callback2;
  EXPECT_EQ(NearbySharingService::StatusCodes::kError,
            service_->UnregisterSendSurface(&transfer_callback2,
                                            &discovery_callback2));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest, UnregisterSendSurfaceNeverRegistered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  EXPECT_CALL(mock_nearby_process_manager(), StopProcess(profile_))
      .Times(AtLeast(1));
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kError,
      service_->UnregisterSendSurface(&transfer_callback, &discovery_callback));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterReceiveSurfaceIsAdvertisingAllContacts) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  local_device_data_manager()->SetDeviceName(kDeviceName);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(PowerLevel::kHighPower,
            fake_nearby_connections_manager_->advertising_power_level());
  ASSERT_TRUE(fake_nearby_connections_manager_->advertising_endpoint_info());
  auto advertisement = GetCurrentAdvertisement();
  ASSERT_TRUE(advertisement);
  EXPECT_EQ(kDeviceName, advertisement->device_name());
  EXPECT_EQ(nearby_share::mojom::ShareTargetType::kLaptop,
            advertisement->device_type());
  auto& test_metadata_key = GetNearbyShareTestEncryptedMetadataKey();
  EXPECT_EQ(test_metadata_key.salt(), advertisement->salt());
  EXPECT_EQ(test_metadata_key.encrypted_key(),
            advertisement->encrypted_metadata_key());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterReceiveSurfaceIsAdvertisingNoOne) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kNoOne);
  local_device_data_manager()->SetDeviceName(kDeviceName);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(PowerLevel::kHighPower,
            fake_nearby_connections_manager_->advertising_power_level());
  ASSERT_TRUE(fake_nearby_connections_manager_->advertising_endpoint_info());
  auto advertisement = GetCurrentAdvertisement();
  ASSERT_TRUE(advertisement);
  EXPECT_EQ(kDeviceName, advertisement->device_name());
  EXPECT_EQ(nearby_share::mojom::ShareTargetType::kLaptop,
            advertisement->device_type());
  // Expecting random metadata key.
  EXPECT_EQ(static_cast<size_t>(sharing::Advertisement::kSaltSize),
            advertisement->salt().size());
  EXPECT_EQ(static_cast<size_t>(
                sharing::Advertisement::kMetadataEncryptionKeyHashByteSize),
            advertisement->encrypted_metadata_key().size());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundRegisterReceiveSurfaceIsAdvertisingSelectedContacts) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kSelectedContacts);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kSelectedContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(PowerLevel::kLowPower,
            fake_nearby_connections_manager_->advertising_power_level());
  ASSERT_TRUE(fake_nearby_connections_manager_->advertising_endpoint_info());
  auto advertisement = GetCurrentAdvertisement();
  ASSERT_TRUE(advertisement);
  EXPECT_FALSE(advertisement->device_name());
  EXPECT_EQ(nearby_share::mojom::ShareTargetType::kLaptop,
            advertisement->device_type());
  auto& test_metadata_key = GetNearbyShareTestEncryptedMetadataKey();
  EXPECT_EQ(test_metadata_key.salt(), advertisement->salt());
  EXPECT_EQ(test_metadata_key.encrypted_key(),
            advertisement->encrypted_metadata_key());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceTwiceSameCallbackKeepAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  NearbySharingService::StatusCodes result2 = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceTwiceKeepAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  MockTransferUpdateCallback callback2;
  NearbySharingService::StatusCodes result2 = service_->RegisterReceiveSurface(
      &callback2, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

#if defined(OS_CHROMEOS)
TEST_F(NearbySharingServiceImplTest,
       ScreenLockedRegisterReceiveSurfaceNotAdvertising) {
  session_controller_->SetScreenLocked(true);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, ScreenLocksDuringAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  session_controller_->SetScreenLocked(true);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  session_controller_->SetScreenLocked(false);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}
#endif  // OS_CHROMEOS

TEST_F(NearbySharingServiceImplTest,
       SuspendedRegisterReceiveSurfaceNotAdvertising) {
  power_client_->SetSuspended(true);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, SuspendDuringAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  power_client_->SetSuspended(true);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  power_client_->SetSuspended(false);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}
TEST_F(NearbySharingServiceImplTest,
       DataUsageChangedRegisterReceiveSurfaceRestartsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  prefs_.SetInteger(prefs::kNearbySharingDataUsageName,
                    static_cast<int>(DataUsage::kOffline));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(DataUsage::kOffline,
            fake_nearby_connections_manager_->advertising_data_usage());

  prefs_.SetInteger(prefs::kNearbySharingDataUsageName,
                    static_cast<int>(DataUsage::kOnline));
  service_->FlushMojoForTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(DataUsage::kOnline,
            fake_nearby_connections_manager_->advertising_data_usage());
}

TEST_F(NearbySharingServiceImplTest,
       NoNetworkRegisterReceiveSurfaceIsAdvertising) {
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  // Succeeds since bluetooth is present.
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothNoNetworkRegisterReceiveSurfaceNotAdvertising) {
  is_bluetooth_present_ = false;
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, WifiRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       EthernetRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       ThreeGRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_3G);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  // Since bluetooth is on, connection still succeeds.
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothWifiReceiveSurfaceIsAdvertising) {
  is_bluetooth_present_ = false;
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);

  // TODO(crbug.com/1129069): When WiFi LAN is supported we will expect this to
  // be true.
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothEthernetReceiveSurfaceIsAdvertising) {
  is_bluetooth_present_ = false;
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);

  // TODO(crbug.com/1129069): When WiFi LAN is supported we will expect this to
  // be true.
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothThreeGReceiveSurfaceNotAdvertising) {
  is_bluetooth_present_ = false;
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_3G);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       DisableFeatureReceiveSurfaceNotAdvertising) {
  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);
  service_->FlushMojoForTesting();
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_TRUE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       DisableFeatureReceiveSurfaceStopsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);
  service_->FlushMojoForTesting();
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_TRUE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceNoOneVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kNoOne));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceNoOneVisibilityNotAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kNoOne));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceVisibilityToNoOneStopsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kSelectedContacts));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kNoOne));
  service_->FlushMojoForTesting();
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceVisibilityToSelectedStartsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kNoOne));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kSelectedContacts));
  service_->FlushMojoForTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceSelectedContactsVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kSelectedContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceSelectedContactsVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kSelectedContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceAllContactsVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kAllContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceAllContactsVisibilityNotAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(Visibility::kAllContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest, UnregisterReceiveSurfaceStopsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  EXPECT_CALL(mock_nearby_process_manager(), StopProcess(profile_))
      .Times(AtLeast(1));
  NearbySharingService::StatusCodes result2 =
      service_->UnregisterReceiveSurface(&callback);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       UnregisterReceiveSurfaceDifferentCallbackKeepAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  MockTransferUpdateCallback callback2;
  NearbySharingService::StatusCodes result2 =
      service_->UnregisterReceiveSurface(&callback2);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest, UnregisterReceiveSurfaceNeverRegistered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  EXPECT_CALL(mock_nearby_process_manager(), StopProcess(profile_))
      .Times(AtLeast(1));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result =
      service_->UnregisterReceiveSurface(&callback);
  // This is no longer considered an error condition.
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_ClosedReadingIntroduction) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_)).Times(0);

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  connection_.Close();

  // Introduction is ignored without any side effect.

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_EmptyIntroductionFrame) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/true);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kUnsupportedAttachmentType,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_FALSE(share_target.has_attachments());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectConnectionResponseFrame(
      sharing::nearby::ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE);

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_ValidIntroductionFrame_InvalidCertificate) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_FALSE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_FALSE(share_target.is_known);
        EXPECT_TRUE(share_target.has_attachments());
        EXPECT_EQ(3u, share_target.text_attachments.size());
        EXPECT_EQ(1u, share_target.file_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_FALSE(share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_EQ(kEndpointId, share_target.device_id);
        EXPECT_FALSE(share_target.full_name);
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  run_loop.Run();

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, IncomingConnection_TimedOut) {
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);
  EXPECT_FALSE(connection_.IsClosed());

  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_TRUE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kTimedOut, metadata.status());
          }));

  task_environment_.FastForwardBy(kReadResponseFrameTimeout +
                                  kIncomingRejectionDelay + kDelta);
  EXPECT_TRUE(connection_.IsClosed());
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_ClosedWaitingLocalConfirmation) {
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_2;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop_2](const ShareTarget& share_target,
                                              TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kFailed, metadata.status());
        run_loop_2.Quit();
      }));

  connection_.Close();
  run_loop_2.Run();

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, IncomingConnection_OutOfStorage) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);

  // Set a huge file size in introduction frame to go out of storage.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .WillOnce(testing::Invoke(
          [](const std::vector<uint8_t>& data,
             MockNearbySharingDecoder::DecodeFrameCallback callback) {
            std::vector<sharing::mojom::FileMetadataPtr> mojo_file_metadatas;
            mojo_file_metadatas.push_back(sharing::mojom::FileMetadata::New(
                "name", sharing::mojom::FileMetadata::Type::kAudio,
                /*payload_id=*/1, kFreeDiskSpace + 1, "mime_type",
                /*id=*/123));

            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_introduction(
                sharing::mojom::IntroductionFrame::New(
                    std::move(mojo_file_metadatas),
                    std::vector<sharing::mojom::TextMetadataPtr>(),
                    /*required_package=*/base::nullopt,
                    std::vector<sharing::mojom::WifiCredentialsMetadataPtr>()));

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));

            std::move(callback).Run(std::move(mojo_frame));
          }));
  connection_.AppendReadableData(std::move(bytes));

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_TRUE(share_target.has_attachments());
        EXPECT_EQ(0u, share_target.text_attachments.size());
        EXPECT_EQ(1u, share_target.file_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);

        EXPECT_EQ(TransferMetadata::Status::kNotEnoughSpace, metadata.status());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, IncomingConnection_FileSizeOverflow) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);

  // Set file size sum huge to check for overflow.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .WillOnce(testing::Invoke(
          [](const std::vector<uint8_t>& data,
             MockNearbySharingDecoder::DecodeFrameCallback callback) {
            std::vector<sharing::mojom::FileMetadataPtr> mojo_file_metadatas;
            mojo_file_metadatas.push_back(sharing::mojom::FileMetadata::New(
                "name_1", sharing::mojom::FileMetadata::Type::kAudio,
                /*payload_id=*/1, /*size=*/std::numeric_limits<int64_t>::max(),
                "mime_type",
                /*id=*/123));
            mojo_file_metadatas.push_back(sharing::mojom::FileMetadata::New(
                "name_2", sharing::mojom::FileMetadata::Type::kVideo,
                /*payload_id=*/2, /*size=*/100, "mime_type",
                /*id=*/124));

            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_introduction(
                sharing::mojom::IntroductionFrame::New(
                    std::move(mojo_file_metadatas),
                    std::vector<sharing::mojom::TextMetadataPtr>(),
                    /*required_package=*/base::nullopt,
                    std::vector<sharing::mojom::WifiCredentialsMetadataPtr>()));

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));

            std::move(callback).Run(std::move(mojo_frame));
          }));
  connection_.AppendReadableData(std::move(bytes));

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);

        EXPECT_EQ(TransferMetadata::Status::kNotEnoughSpace, metadata.status());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_ValidIntroductionFrame_ValidCertificate) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_FALSE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_TRUE(share_target.has_attachments());
        EXPECT_EQ(3u, share_target.text_attachments.size());
        EXPECT_EQ(1u, share_target.file_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);

        EXPECT_FALSE(metadata.token().has_value());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptInvalidShareTarget) {
  ShareTarget share_target;
  base::RunLoop run_loop;
  service_->Accept(
      share_target,
      base::BindLambdaForTesting(
          [&](NearbySharingServiceImpl::StatusCodes status_code) {
            EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOutOfOrderApiCall,
                      status_code);
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(NearbySharingServiceImplTest,
       AcceptValidShareTarget_RegisterPayloadError) {
  fake_nearby_connections_manager_->SetPayloadPathStatus(
      kFilePayloadId, location::nearby::connections::mojom::Status::kError);
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_accept;
  service_->Accept(share_target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(
                             NearbySharingServiceImpl::StatusCodes::kError,
                             status_code);
                         run_loop_accept.Quit();
                       }));

  run_loop_accept.Run();

  EXPECT_TRUE(
      fake_nearby_connections_manager_->DidUpgradeBandwidth(kEndpointId));

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();

  EXPECT_FALSE(connection_.IsClosed());

  // TODO(https://crbug.com/1122552) - Remove cleanups after bugfix
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::Optional<base::FilePath> path =
        fake_nearby_connections_manager_->GetRegisteredPayloadPath(
            kFilePayloadId);
    EXPECT_TRUE(path);
    base::DeleteFile(*path);
  }

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTarget) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, location::nearby::connections::mojom::Status::kSuccess);
  }

  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_accept;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kAwaitingRemoteAcceptance,
                      metadata.status());
          }));

  service_->Accept(share_target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop_accept.Quit();
                       }));

  run_loop_accept.Run();

  EXPECT_TRUE(
      fake_nearby_connections_manager_->DidUpgradeBandwidth(kEndpointId));

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectConnectionResponseFrame(
      sharing::nearby::ConnectionResponseFrame::ACCEPT);

  EXPECT_FALSE(connection_.IsClosed());

  // TODO(https://crbug.com/1122552) - Remove cleanups after bugfix
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::Optional<base::FilePath> path =
        fake_nearby_connections_manager_->GetRegisteredPayloadPath(
            kFilePayloadId);
    EXPECT_TRUE(path);
    base::DeleteFile(*path);
  }

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTarget_PayloadSuccessful) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, location::nearby::connections::mojom::Status::kSuccess);
  }

  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_accept;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kAwaitingRemoteAcceptance,
                      metadata.status());
          }));

  service_->Accept(share_target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop_accept.Quit();
                       }));

  run_loop_accept.Run();

  fake_nearby_connections_manager_->SetIncomingPayload(
      kFilePayloadId, GetFilePayloadPtr(kFilePayloadId));

  for (int64_t id : kValidIntroductionFramePayloadIds) {
    // Update file payload at the end.
    if (id == kFilePayloadId)
      continue;

    fake_nearby_connections_manager_->SetIncomingPayload(
        id, GetTextPayloadPtr(id, kTextPayload));

    auto* listener =
        fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
            id);
    ASSERT_TRUE(listener);

    base::RunLoop run_loop_progress;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                      TransferMetadata metadata) {
          EXPECT_FALSE(metadata.is_final_status());
          EXPECT_EQ(TransferMetadata::Status::kInProgress, metadata.status());
          run_loop_progress.Quit();
        }));

    location::nearby::connections::mojom::PayloadTransferUpdatePtr payload =
        location::nearby::connections::mojom::PayloadTransferUpdate::New(
            id, location::nearby::connections::mojom::PayloadStatus::kSuccess,
            /*total_bytes=*/kPayloadSize,
            /*bytes_transferred=*/kPayloadSize);
    listener->OnStatusUpdate(std::move(payload));
    run_loop_progress.Run();

    task_environment_.FastForwardBy(kMinProgressUpdateFrequency);
  }

  base::FilePath file_path;
  base::RunLoop run_loop_success;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_TRUE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kComplete, metadata.status());

            ASSERT_TRUE(share_target.has_attachments());
            EXPECT_EQ(1u, share_target.file_attachments.size());
            for (const FileAttachment& file : share_target.file_attachments) {
              EXPECT_TRUE(file.file_path());
              file_path = *file.file_path();
            }

            EXPECT_EQ(3u, share_target.text_attachments.size());
            for (const TextAttachment& text : share_target.text_attachments) {
              EXPECT_EQ(kTextPayload, text.text_body());
            }
            run_loop_success.Quit();
          }));

  auto* listener =
      fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
          kFilePayloadId);
  ASSERT_TRUE(listener);

  location::nearby::connections::mojom::PayloadTransferUpdatePtr payload =
      location::nearby::connections::mojom::PayloadTransferUpdate::New(
          kFilePayloadId,
          location::nearby::connections::mojom::PayloadStatus::kSuccess,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
  listener->OnStatusUpdate(std::move(payload));
  run_loop_success.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // TODO(crbug.com/1123022): This check is flaky, should be investigated.
  // EXPECT_TRUE(FileExists(file_path));

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);

  // Remove test file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::DeleteFile(file_path);
  }
}

TEST_F(NearbySharingServiceImplTest,
       AcceptValidShareTarget_PayloadSuccessful_IncomingPayloadNotFound) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, location::nearby::connections::mojom::Status::kSuccess);
  }

  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_accept;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kAwaitingRemoteAcceptance,
                      metadata.status());
          }));

  service_->Accept(share_target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop_accept.Quit();
                       }));

  run_loop_accept.Run();

  fake_nearby_connections_manager_->SetIncomingPayload(
      kFilePayloadId, GetFilePayloadPtr(kFilePayloadId));

  for (int64_t id : kValidIntroductionFramePayloadIds) {
    // Update file payload at the end.
    if (id == kFilePayloadId)
      continue;

    // Deliberately not calling SetIncomingPayload() for text payloads to check
    // for failure condition.

    auto* listener =
        fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
            id);
    ASSERT_TRUE(listener);

    base::RunLoop run_loop_progress;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                      TransferMetadata metadata) {
          EXPECT_FALSE(metadata.is_final_status());
          EXPECT_EQ(TransferMetadata::Status::kInProgress, metadata.status());
          run_loop_progress.Quit();
        }));

    location::nearby::connections::mojom::PayloadTransferUpdatePtr payload =
        location::nearby::connections::mojom::PayloadTransferUpdate::New(
            id, location::nearby::connections::mojom::PayloadStatus::kSuccess,
            /*total_bytes=*/kPayloadSize,
            /*bytes_transferred=*/kPayloadSize);
    listener->OnStatusUpdate(std::move(payload));
    run_loop_progress.Run();

    task_environment_.FastForwardBy(kMinProgressUpdateFrequency);
  }

  base::RunLoop run_loop_success;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_TRUE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kFailed, metadata.status());

            ASSERT_TRUE(share_target.has_attachments());
            EXPECT_EQ(1u, share_target.file_attachments.size());
            const FileAttachment& file = share_target.file_attachments[0];
            EXPECT_FALSE(file.file_path());
            run_loop_success.Quit();
          }));

  auto* listener =
      fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
          kFilePayloadId);
  ASSERT_TRUE(listener);

  location::nearby::connections::mojom::PayloadTransferUpdatePtr payload =
      location::nearby::connections::mojom::PayloadTransferUpdate::New(
          kFilePayloadId,
          location::nearby::connections::mojom::PayloadStatus::kSuccess,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
  listener->OnStatusUpdate(std::move(payload));
  run_loop_success.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  task_environment_.RunUntilIdle();

  base::Optional<base::FilePath> file_path =
      fake_nearby_connections_manager_->GetRegisteredPayloadPath(
          kFilePayloadId);
  ASSERT_TRUE(file_path);
  EXPECT_FALSE(FileExists(*file_path));

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTarget_PayloadFailed) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, location::nearby::connections::mojom::Status::kSuccess);
  }

  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_accept;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kAwaitingRemoteAcceptance,
                      metadata.status());
          }));

  service_->Accept(share_target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop_accept.Quit();
                       }));

  run_loop_accept.Run();

  auto* listener =
      fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
          kFilePayloadId);
  ASSERT_TRUE(listener);

  base::RunLoop run_loop_failure;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_TRUE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kFailed, metadata.status());

            ASSERT_TRUE(share_target.has_attachments());
            EXPECT_EQ(1u, share_target.file_attachments.size());
            const FileAttachment& file = share_target.file_attachments[0];
            EXPECT_FALSE(file.file_path());
            run_loop_failure.Quit();
          }));

  location::nearby::connections::mojom::PayloadTransferUpdatePtr payload =
      location::nearby::connections::mojom::PayloadTransferUpdate::New(
          kFilePayloadId,
          location::nearby::connections::mojom::PayloadStatus::kFailure,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
  listener->OnStatusUpdate(std::move(payload));
  run_loop_failure.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  task_environment_.RunUntilIdle();

  base::Optional<base::FilePath> file_path =
      fake_nearby_connections_manager_->GetRegisteredPayloadPath(
          kFilePayloadId);
  ASSERT_TRUE(file_path);
  EXPECT_FALSE(FileExists(*file_path));

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTarget_PayloadCancelled) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, location::nearby::connections::mojom::Status::kSuccess);
  }

  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_accept;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kAwaitingRemoteAcceptance,
                      metadata.status());
          }));

  service_->Accept(share_target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop_accept.Quit();
                       }));

  run_loop_accept.Run();

  auto* listener =
      fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
          kFilePayloadId);
  ASSERT_TRUE(listener);

  base::RunLoop run_loop_failure;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_TRUE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kCancelled, metadata.status());

            ASSERT_TRUE(share_target.has_attachments());
            EXPECT_EQ(1u, share_target.file_attachments.size());
            const FileAttachment& file = share_target.file_attachments[0];
            EXPECT_FALSE(file.file_path());
            run_loop_failure.Quit();
          }));

  location::nearby::connections::mojom::PayloadTransferUpdatePtr payload =
      location::nearby::connections::mojom::PayloadTransferUpdate::New(
          kFilePayloadId,
          location::nearby::connections::mojom::PayloadStatus::kCanceled,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
  listener->OnStatusUpdate(std::move(payload));
  run_loop_failure.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  task_environment_.RunUntilIdle();

  base::Optional<base::FilePath> file_path =
      fake_nearby_connections_manager_->GetRegisteredPayloadPath(
          kFilePayloadId);
  ASSERT_TRUE(file_path);
  EXPECT_FALSE(FileExists(*file_path));

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, RejectInvalidShareTarget) {
  ShareTarget share_target;
  base::RunLoop run_loop;
  service_->Reject(
      share_target,
      base::BindLambdaForTesting(
          [&](NearbySharingServiceImpl::StatusCodes status_code) {
            EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOutOfOrderApiCall,
                      status_code);
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(NearbySharingServiceImplTest, RejectValidShareTarget) {
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_reject;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_TRUE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kRejected, metadata.status());
          }));

  service_->Reject(share_target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop_reject.Quit();
                       }));

  run_loop_reject.Run();

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectConnectionResponseFrame(
      sharing::nearby::ConnectionResponseFrame::REJECT);

  task_environment_.FastForwardBy(kIncomingRejectionDelay + kDelta);
  EXPECT_TRUE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_KeyVerificationRunnerStatusUnable) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_FALSE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_TRUE(share_target.has_attachments());
        EXPECT_EQ(3u, share_target.text_attachments.size());
        EXPECT_EQ(1u, share_target.file_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);

        EXPECT_EQ(kFourDigitToken, metadata.token());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kUnable);
  SetUpForegroundReceiveSurface(callback);

  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  EXPECT_TRUE(
      fake_nearby_connections_manager_->DidUpgradeBandwidth(kEndpointId));

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_KeyVerificationRunnerStatusUnable_LowPower) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_FALSE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_TRUE(share_target.has_attachments());
        EXPECT_EQ(3u, share_target.text_attachments.size());
        EXPECT_EQ(1u, share_target.file_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);

        EXPECT_EQ(kFourDigitToken, metadata.token());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kUnable);

  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->DidUpgradeBandwidth(kEndpointId));

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_KeyVerificationRunnerStatusFail) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kFail);
  SetUpForegroundReceiveSurface(callback);

  // Ensures that introduction is never received for failed key verification.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .Times(0);
  connection_.AppendReadableData(bytes);

  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  EXPECT_TRUE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnection_EmptyAuthToken_KeyVerificationRunnerStatusFail) {
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;

  SetUpForegroundReceiveSurface(callback);

  // Ensures that introduction is never received for empty auth token.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .Times(0);
  connection_.AppendReadableData(bytes);

  service_->OnIncomingConnection(kEndpointId, kValidV1EndpointInfo,
                                 &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  EXPECT_TRUE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, RegisterReceiveSurfaceWhileSending) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  base::RunLoop run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingLocalConfirmation,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        run_loop.QuitClosure());
  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  run_loop.Run();

  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kTransferAlreadyInProgress);

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendAttachments_WithoutAttachments) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kError,
            service_->SendAttachments(target, /*attachments=*/{}));
  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendText_AlreadySending) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  base::RunLoop run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingLocalConfirmation,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        run_loop.QuitClosure());
  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  run_loop.Run();

  // We're now in the sending state, try to send again should fail
  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kError,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendText_WithoutScanning) {
  ShareTarget target;
  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kError,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
}

TEST_F(NearbySharingServiceImplTest, SendText_UnknownTarget) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  DiscoverShareTarget(transfer_callback, discovery_callback);

  ShareTarget target;
  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kError,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendText_FailedCreateEndpointInfo) {
  // Set name with too many characters.
  local_device_data_manager()->SetDeviceName(std::string(300, 'a'));

  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kError,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendText_FailedToConnect) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  // Call DiscoverShareTarget() instead of SetUpOutgoingShareTarget() as we want
  // to fail before key verification is done.
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  base::RunLoop run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kFailed},
                        run_loop.QuitClosure());

  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  run_loop.Run();

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendText_FailedKeyVerification) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  base::RunLoop run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kFailed},
                        run_loop.QuitClosure());

  SetUpKeyVerification(/*is_incoming=*/false,
                       sharing::mojom::PairedKeyResultFrame_Status::kFail);
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  fake_nearby_connections_manager_->set_nearby_connection(&connection_);

  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  run_loop.Run();

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendText_UnableToVerifyKey) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  base::RunLoop run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingLocalConfirmation,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        run_loop.QuitClosure());

  SetUpKeyVerification(/*is_incoming=*/false,
                       sharing::mojom::PairedKeyResultFrame_Status::kUnable);
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  fake_nearby_connections_manager_->set_nearby_connection(&connection_);

  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  run_loop.Run();

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_P(NearbySharingServiceImplSendFailureTest, SendText_RemoteFailure) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  base::RunLoop introduction_run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingLocalConfirmation,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        introduction_run_loop.QuitClosure());

  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  introduction_run_loop.Run();

  // Verify data sent to the remote device so far.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectIntroductionFrame();

  // We're now waiting for the remote device to respond with the accept result.
  base::RunLoop reject_run_loop;
  ExpectTransferUpdates(transfer_callback, target, {GetParam().expected_status},
                        reject_run_loop.QuitClosure());

  // Cancel the transfer by rejecting it.
  SendConnectionResponse(GetParam().response_status);
  reject_run_loop.Run();

  EXPECT_TRUE(connection_.IsClosed());

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_P(NearbySharingServiceImplSendFailureTest, SendFiles_RemoteFailure) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  std::vector<uint8_t> test_data = {'T', 'e', 's', 't'};
  base::FilePath path = CreateTestFile("text.txt", test_data);

  base::RunLoop introduction_run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingLocalConfirmation,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        introduction_run_loop.QuitClosure());

  EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
            service_->SendAttachments(target, CreateFileAttachments({path})));
  introduction_run_loop.Run();

  // Verify data sent to the remote device so far.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectIntroductionFrame();

  // We're now waiting for the remote device to respond with the accept result.
  base::RunLoop reject_run_loop;
  ExpectTransferUpdates(transfer_callback, target, {GetParam().expected_status},
                        reject_run_loop.QuitClosure());

  // Cancel the transfer by rejecting it.
  SendConnectionResponse(GetParam().response_status);
  reject_run_loop.Run();

  EXPECT_TRUE(connection_.IsClosed());

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

INSTANTIATE_TEST_SUITE_P(NearbySharingServiceImplSendFailureTest,
                         NearbySharingServiceImplSendFailureTest,
                         testing::ValuesIn(kSendFailureTestData));

TEST_F(NearbySharingServiceImplTest, SendText_Success) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  base::RunLoop introduction_run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingLocalConfirmation,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        introduction_run_loop.QuitClosure());

  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  introduction_run_loop.Run();

  // Verify data sent to the remote device so far.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  auto intro = ExpectIntroductionFrame();

  ASSERT_EQ(1, intro.text_metadata_size());
  auto meta = intro.text_metadata(0);

  EXPECT_EQ(kTextPayload, meta.text_title());
  EXPECT_EQ(strlen(kTextPayload), static_cast<size_t>(meta.size()));
  EXPECT_EQ(sharing::nearby::TextMetadata_Type_TEXT, meta.type());

  ASSERT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  auto advertisement =
      sharing::AdvertisementDecoder::FromEndpointInfo(base::make_span(
          *fake_nearby_connections_manager_->connection_endpoint_info(
              kEndpointId)));
  ASSERT_TRUE(advertisement);
  EXPECT_EQ(kDeviceName, advertisement->device_name());
  EXPECT_EQ(nearby_share::mojom::ShareTargetType::kLaptop,
            advertisement->device_type());
  auto& test_metadata_key = GetNearbyShareTestEncryptedMetadataKey();
  EXPECT_EQ(test_metadata_key.salt(), advertisement->salt());
  EXPECT_EQ(test_metadata_key.encrypted_key(),
            advertisement->encrypted_metadata_key());

  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target);
  FinishOutgoingTransfer(transfer_callback, target, info);

  // We should not have called disconnect yet as we want to wait for 1 minute to
  // make sure all outgoing packets have been sent properly.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));

  // Forward time until we send the disconnect request to Nearby Connections.
  task_environment_.FastForwardBy(kOutgoingDisconnectionDelay);

  // Expect to be disconnected now.
  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendText_SuccessClosedConnection) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);
  SetUpOutgoingConnectionUntilAccept(transfer_callback, target);
  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target);
  FinishOutgoingTransfer(transfer_callback, target, info);

  // We should not have called disconnect yet as we want to wait for 1 minute to
  // make sure all outgoing packets have been sent properly.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));

  // Call disconnect on the connection early before the timeout has passed.
  connection_.Close();

  // Expect that we haven't called disconnect again as the endpoint is already
  // disconnected.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));

  // Make sure the scheduled disconnect callback does nothing.
  task_environment_.FastForwardBy(kOutgoingDisconnectionDelay);

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, SendFiles_Success) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  std::vector<uint8_t> test_data = {'T', 'e', 's', 't'};
  std::string file_name = "test.txt";
  base::FilePath path = CreateTestFile(file_name, test_data);

  base::RunLoop introduction_run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingLocalConfirmation,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        introduction_run_loop.QuitClosure());

  EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
            service_->SendAttachments(target, CreateFileAttachments({path})));
  introduction_run_loop.Run();

  // Verify data sent to the remote device so far.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  auto intro = ExpectIntroductionFrame();

  ASSERT_EQ(1, intro.file_metadata_size());
  auto meta = intro.file_metadata(0);

  EXPECT_EQ(file_name, meta.name());
  EXPECT_EQ("text/plain", meta.mime_type());
  EXPECT_EQ(test_data.size(), static_cast<size_t>(meta.size()));
  EXPECT_EQ(sharing::nearby::FileMetadata_Type_UNKNOWN, meta.type());

  // Expect the file payload to be sent in the end.
  base::RunLoop payload_run_loop;
  fake_nearby_connections_manager_->set_send_payload_callback(
      base::BindLambdaForTesting(
          [&](NearbyConnectionsManager::PayloadPtr payload,
              NearbyConnectionsManager::PayloadStatusListener* listener) {
            base::ScopedAllowBlockingForTesting allow_blocking;

            ASSERT_TRUE(payload->content->is_file());
            base::File file = std::move(payload->content->get_file()->file);
            ASSERT_TRUE(file.IsValid());

            std::vector<uint8_t> payload_bytes(test_data.size());
            EXPECT_TRUE(file.ReadAndCheck(/*offset=*/0,
                                          base::make_span(payload_bytes)));
            EXPECT_EQ(test_data, payload_bytes);
            file.Close();

            payload_run_loop.Quit();
          }));

  // We're now waiting for the remote device to respond with the accept result.
  base::RunLoop accept_run_loop;
  ExpectTransferUpdates(transfer_callback, target,
                        {TransferMetadata::Status::kInProgress},
                        accept_run_loop.QuitClosure());

  // Kick off send process by accepting the transfer from the remote device.
  SendConnectionResponse(
      sharing::mojom::ConnectionResponseFrame::Status::kAccept);

  accept_run_loop.Run();
  payload_run_loop.Run();

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_F(NearbySharingServiceImplTest, ProfileChangedControlsDiscovery) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  ON_CALL(mock_nearby_process_manager_, IsActiveProfile(profile_))
      .WillByDefault(Return(false));
  service_->OnNearbyProfileChanged(/*profile=*/nullptr);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  ON_CALL(mock_nearby_process_manager_, IsActiveProfile(profile_))
      .WillByDefault(Return(true));
  service_->OnNearbyProfileChanged(/*profile=*/nullptr);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest, ProfileChangedControlsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  ON_CALL(mock_nearby_process_manager_, IsActiveProfile(profile_))
      .WillByDefault(Return(false));
  service_->OnNearbyProfileChanged(/*profile=*/nullptr);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  ON_CALL(mock_nearby_process_manager_, IsActiveProfile(profile_))
      .WillByDefault(Return(true));
  service_->OnNearbyProfileChanged(/*profile=*/nullptr);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterForegroundReceiveSurfaceEntersHighVisibility) {
  TestObserver observer(service_.get());
  NiceMock<MockTransferUpdateCallback> callback;

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  local_device_data_manager()->SetDeviceName(kDeviceName);

  // To start, we should not be in high visibility state.
  EXPECT_FALSE(service_->IsInHighVisibility());

  // If we register a foreground surface we should end up in high visibility
  // state.
  SetUpForegroundReceiveSurface(callback);

  // At this point we should have a new high visibility state and the observer
  // should have been called as well.
  EXPECT_TRUE(service_->IsInHighVisibility());
  EXPECT_TRUE(observer.in_high_visibility_);

  // If we unregister the foreground receive surface we should no longer be in
  // high visibility and the observer should be notified.
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            service_->UnregisterReceiveSurface(&callback));
  EXPECT_FALSE(service_->IsInHighVisibility());
  EXPECT_FALSE(observer.in_high_visibility_);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
}

TEST_F(NearbySharingServiceImplTest, ShutdownCallsObservers) {
  TestObserver observer(service_.get());

  EXPECT_FALSE(observer.shutdown_called_);

  service_->Shutdown();

  EXPECT_TRUE(observer.shutdown_called_);

  // Prevent a double shutdown.
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest,
       PeriodicallyRotateBackgroundAdvertisement) {
  certificate_manager()->set_next_salt({0x00, 0x01});
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  NiceMock<MockTransferUpdateCallback> callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_initial =
      fake_nearby_connections_manager_->advertising_endpoint_info();

  certificate_manager()->set_next_salt({0x00, 0x02});
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(15));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_rotated =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  EXPECT_NE(endpoint_info_initial, endpoint_info_rotated);
}
