// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/nearby_sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"
#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_advertiser.h"
#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/power_client.h"
#include "chrome/browser/nearby_sharing/wifi_network_configuration/fake_wifi_network_configuration_handler.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/session/test_session_controller.h"
#include "chrome/services/sharing/nearby/decoder/advertisement_decoder.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/components/nearby/common/connections_manager/fake_nearby_connection.h"
#include "chromeos/ash/components/nearby/common/connections_manager/fake_nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_low_energy_scan_session.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"
#include "third_party/nearby/sharing/proto/wire_format.pb.h"

using ::testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;

using NetConnectionType = net::NetworkChangeNotifier::ConnectionType;

using SendSurfaceState = NearbySharingService::SendSurfaceState;

using NearbyProcessShutdownReason =
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason;

namespace {

class FakeFastInitiationAdvertiser : public FastInitiationAdvertiser {
 public:
  explicit FakeFastInitiationAdvertiser(
      scoped_refptr<device::BluetoothAdapter> adapter,
      bool should_succeed_on_start,
      base::OnceCallback<void()> on_stop_advertising_callback,
      base::OnceCallback<void()> on_destroy_callback)
      : FastInitiationAdvertiser(adapter),
        should_succeed_on_start_(should_succeed_on_start),
        on_stop_advertising_callback_(std::move(on_stop_advertising_callback)),
        on_destroy_callback_(std::move(on_destroy_callback)) {}

  ~FakeFastInitiationAdvertiser() override {
    std::move(on_destroy_callback_).Run();
  }

  void StartAdvertising(FastInitType type,
                        base::OnceCallback<void()> callback,
                        base::OnceCallback<void()> error_callback) override {
    ++start_advertising_call_count_;
    if (should_succeed_on_start_) {
      std::move(callback).Run();
    } else {
      std::move(error_callback).Run();
    }
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

class FakeFastInitiationAdvertiserFactory
    : public FastInitiationAdvertiser::Factory {
 public:
  explicit FakeFastInitiationAdvertiserFactory(bool should_succeed_on_start)
      : should_succeed_on_start_(should_succeed_on_start) {}

  std::unique_ptr<FastInitiationAdvertiser> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    auto fake_fast_initiation_advertiser =
        std::make_unique<FakeFastInitiationAdvertiser>(
            adapter, should_succeed_on_start_,
            base::BindOnce(
                &FakeFastInitiationAdvertiserFactory::OnStopAdvertising,
                weak_ptr_factory_.GetWeakPtr()),
            base::BindOnce(&FakeFastInitiationAdvertiserFactory::
                               OnFastInitiationAdvertiserDestroyed,
                           weak_ptr_factory_.GetWeakPtr()));
    last_fake_fast_initiation_advertiser_ =
        fake_fast_initiation_advertiser.get();
    return std::move(fake_fast_initiation_advertiser);
  }

  void OnStopAdvertising() { stop_advertising_called_ = true; }

  void OnFastInitiationAdvertiserDestroyed() {
    fast_initiation_advertiser_destroyed_ = true;
    last_fake_fast_initiation_advertiser_ = nullptr;
  }

  size_t StartAdvertisingCount() {
    return last_fake_fast_initiation_advertiser_
               ? last_fake_fast_initiation_advertiser_
                     ->start_advertising_call_count()
               : 0;
  }

  bool StopAdvertisingCalledAndAdvertiserDestroyed() {
    return stop_advertising_called_ && fast_initiation_advertiser_destroyed_;
  }

 private:
  raw_ptr<FakeFastInitiationAdvertiser> last_fake_fast_initiation_advertiser_ =
      nullptr;
  bool should_succeed_on_start_ = false;
  bool stop_advertising_called_ = false;
  bool fast_initiation_advertiser_destroyed_ = false;
  base::WeakPtrFactory<FakeFastInitiationAdvertiserFactory> weak_ptr_factory_{
      this};
};

class FakeFastInitiationScanner : public FastInitiationScanner {
 public:
  FakeFastInitiationScanner(scoped_refptr<device::BluetoothAdapter> adapter,
                            base::OnceClosure destructor_callback)
      : FastInitiationScanner(adapter),
        destructor_callback_(std::move(destructor_callback)) {}

  ~FakeFastInitiationScanner() override {
    std::move(destructor_callback_).Run();
  }

  void StartScanning(base::RepeatingClosure devices_detected_callback,
                     base::RepeatingClosure devices_not_detected_callback,
                     base::OnceClosure scanner_invalidated_callback) override {
    ++start_scanning_call_count_;
    devices_detected_callback_ = std::move(devices_detected_callback);
    devices_not_detected_callback_ = std::move(devices_not_detected_callback);
    scanner_invalidated_callback_ = std::move(scanner_invalidated_callback);
  }

  void DevicesDetected() { devices_detected_callback_.Run(); }

  void DevicesNotDetected() { devices_not_detected_callback_.Run(); }

  void ScannerInvalidated() { std::move(scanner_invalidated_callback_).Run(); }

 private:
  base::OnceClosure destructor_callback_;
  base::RepeatingClosure devices_detected_callback_;
  base::RepeatingClosure devices_not_detected_callback_;
  base::OnceClosure scanner_invalidated_callback_;
  size_t start_scanning_call_count_ = 0u;
};

class FakeFastInitiationScannerFactory : public FastInitiationScanner::Factory {
 public:
  std::unique_ptr<FastInitiationScanner> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    ++scanner_created_count_;
    auto scanner = std::make_unique<FakeFastInitiationScanner>(
        adapter,
        base::BindOnce(&FakeFastInitiationScannerFactory::OnScannerDestroyed,
                       weak_ptr_factory_.GetWeakPtr()));
    last_fake_fast_initiation_scanner_ = scanner.get();
    return std::move(scanner);
  }

  bool IsHardwareSupportAvailable() override {
    return is_hardware_support_available_;
  }

  void SetHardwareSupportAvailable(bool is_hardware_support_available) {
    is_hardware_support_available_ = is_hardware_support_available;
  }

  FakeFastInitiationScanner* last_fake_fast_initiation_scanner() {
    return last_fake_fast_initiation_scanner_;
  }
  size_t scanner_created_count() { return scanner_created_count_; }
  size_t scanner_destroyed_count() { return scanner_destroyed_count_; }

 private:
  void OnScannerDestroyed() { ++scanner_destroyed_count_; }

  raw_ptr<FakeFastInitiationScanner, DanglingUntriaged>
      last_fake_fast_initiation_scanner_ = nullptr;
  size_t scanner_created_count_ = 0u;
  size_t scanner_destroyed_count_ = 0u;
  bool is_hardware_support_available_ = true;

  base::WeakPtrFactory<FakeFastInitiationScannerFactory> weak_ptr_factory_{
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

class FakeArcNearbyShareSession {
 public:
  void OnCleanupCallbackStub() { callback_called = true; }
  bool CleanupCallbackCalled() { return callback_called; }

 private:
  bool callback_called = false;
};

}  // namespace

namespace NearbySharingServiceUnitTests {

constexpr base::TimeDelta kDelta = base::Milliseconds(100);

const char kProfileName[] = "profile_name";
const char kServiceId[] = "NearbySharing";
const char kDeviceName[] = "test_device_name";
const nearby_share::mojom::ShareTargetType kDeviceType =
    nearby_share::mojom::ShareTargetType::kPhone;
const char kEndpointId[] = "test_endpoint_id";
const char kTextPayload[] = "Test text payload";
const char kSsid[] = "Test_SSID";
const char kWifiPassword[] = "test_password";
const sharing::mojom::WifiCredentialsMetadata::SecurityType kWifiSecurityType =
    sharing::mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk;

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
constexpr int kWifiCredentialsId = 222;
constexpr int kWifiCredentialsPayloadId = 333;

const std::vector<int64_t> kValidIntroductionFramePayloadIds = {
    1, 2, 3, kFilePayloadId, kWifiCredentialsPayloadId};

constexpr size_t kMaxCertificateDownloadsDuringDiscovery = 3u;
constexpr base::TimeDelta kCertificateDownloadDuringDiscoveryPeriod =
    base::Seconds(10);

// We will run tests with the following feature flags enabled and disabled in
// all permutations. To add or a remove a feature you can just update this list.
const std::vector<base::test::FeatureRef> kTestFeatures = {
    chromeos::features::kQuickShareV2};

bool FileExists(const base::FilePath& file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return base::PathExists(file_path);
}

nearby::connections::mojom::PayloadPtr GetFilePayloadPtr(int64_t payload_id) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  base::File file(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                            base::File::Flags::FLAG_READ |
                            base::File::Flags::FLAG_WRITE);

  return nearby::connections::mojom::Payload::New(
      payload_id,
      nearby::connections::mojom::PayloadContent::NewFile(
          nearby::connections::mojom::FilePayload::New(std::move(file))));
}

nearby::connections::mojom::PayloadPtr GetTextPayloadPtr(
    int64_t payload_id,
    const std::string& text) {
  return nearby::connections::mojom::Payload::New(
      payload_id, nearby::connections::mojom::PayloadContent::NewBytes(
                      nearby::connections::mojom::BytesPayload::New(
                          std::vector<uint8_t>(text.begin(), text.end()))));
}

nearby::connections::mojom::PayloadPtr GetWifiPayloadPtr(
    int64_t payload_id,
    const std::string& password) {
  nearby::sharing::service::proto::WifiCredentials credentials_proto;
  credentials_proto.set_password(password);
  const std::string& proto_string = credentials_proto.SerializeAsString();
  return nearby::connections::mojom::Payload::New(
      payload_id,
      nearby::connections::mojom::PayloadContent::NewBytes(
          nearby::connections::mojom::BytesPayload::New(
              std::vector<uint8_t>(proto_string.begin(), proto_string.end()))));
}

sharing::mojom::FramePtr GetIntroductionFrame(
    int text_metadata_count,
    int file_metadata_count,
    int wifi_credentials_metadata_count) {
  std::vector<sharing::mojom::TextMetadataPtr> mojo_text_metadatas;
  for (int i = 1; i <= text_metadata_count; i++) {
    mojo_text_metadatas.push_back(sharing::mojom::TextMetadata::New(
        "title " + base::NumberToString(i),
        static_cast<sharing::mojom::TextMetadata::Type>(i), /*payload_id=*/i,
        kPayloadSize, /*id=*/i));
  }

  std::vector<sharing::mojom::FileMetadataPtr> mojo_file_metadatas;
  for (int i = 1; i <= file_metadata_count; i++) {
    mojo_file_metadatas.push_back(sharing::mojom::FileMetadata::New(
        "unit_test_nearby_share_name_\x80",  // Filename contains non-ascii
                                             // char.
        sharing::mojom::FileMetadata::Type::kVideo, kFilePayloadId,
        kPayloadSize, "mime type", /*id=*/100));
  }

  std::vector<sharing::mojom::WifiCredentialsMetadataPtr>
      mojo_wifi_credentials_metadatas;
  for (int i = 1; i <= wifi_credentials_metadata_count; i++) {
    mojo_wifi_credentials_metadatas.push_back(
        sharing::mojom::WifiCredentialsMetadata::New(kSsid, kWifiSecurityType,
                                                     kWifiCredentialsPayloadId,
                                                     kWifiCredentialsId));
  }

  sharing::mojom::V1FramePtr mojo_v1frame =
      sharing::mojom::V1Frame::NewIntroduction(
          sharing::mojom::IntroductionFrame::New(
              std::move(mojo_file_metadatas), std::move(mojo_text_metadatas),
              /*required_package=*/std::nullopt,
              std::move(mojo_wifi_credentials_metadatas)));

  sharing::mojom::FramePtr mojo_frame =
      sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));
  return mojo_frame;
}

sharing::mojom::FramePtr GetConnectionResponseFrame(
    sharing::mojom::ConnectionResponseFrame::Status status) {
  sharing::mojom::V1FramePtr mojo_v1frame =
      sharing::mojom::V1Frame::NewConnectionResponse(
          sharing::mojom::ConnectionResponseFrame::New(status));

  sharing::mojom::FramePtr mojo_frame =
      sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));
  return mojo_frame;
}

sharing::mojom::FramePtr GetCancelFrame() {
  sharing::mojom::V1FramePtr mojo_v1frame =
      sharing::mojom::V1Frame::NewCancelFrame(
          sharing::mojom::CancelFrame::New());

  sharing::mojom::FramePtr mojo_frame =
      sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));
  return mojo_frame;
}

std::vector<std::unique_ptr<Attachment>> CreateTextAttachments(
    std::vector<std::string> texts) {
  std::vector<std::unique_ptr<Attachment>> attachments;
  for (auto& text : texts) {
    attachments.push_back(std::make_unique<TextAttachment>(
        TextAttachment::Type::kText, std::move(text), /*title=*/std::nullopt,
        /*mime_type=*/std::nullopt));
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

class MockBluetoothAdapterWithIntervals : public device::MockBluetoothAdapter {
 public:
  MOCK_METHOD2(OnSetAdvertisingInterval, void(int64_t, int64_t));

  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) override {
    std::move(callback).Run();
    OnSetAdvertisingInterval(min.InMilliseconds(), max.InMilliseconds());
  }

 protected:
  ~MockBluetoothAdapterWithIntervals() override = default;
};

class NearbySharingServiceImplTestBase : public testing::Test {
 public:
  explicit NearbySharingServiceImplTestBase(size_t feature_mask)
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    CreateFeatureList(feature_mask);
    RegisterNearbySharingPrefs(prefs_.registry());
  }

  ~NearbySharingServiceImplTestBase() override = default;

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
        base::MakeRefCounted<NiceMock<MockBluetoothAdapterWithIntervals>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(Invoke(
            this, &NearbySharingServiceImplTestBase::IsBluetoothPresent));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered())
        .WillByDefault(Invoke(
            this, &NearbySharingServiceImplTestBase::IsBluetoothPowered));
    ON_CALL(*mock_bluetooth_adapter_, AddObserver(_))
        .WillByDefault(Invoke(
            this, &NearbySharingServiceImplTestBase::AddAdapterObserver));
    ON_CALL(*mock_bluetooth_adapter_, OnSetAdvertisingInterval(_, _))
        .WillByDefault(Invoke(
            this, &NearbySharingServiceImplTestBase::OnSetAdvertisingInterval));
    ON_CALL(*mock_bluetooth_adapter_, StartLowEnergyScanSession(_, _))
        .WillByDefault(Invoke(
            this,
            &NearbySharingServiceImplTestBase::StartLowEnergyScanSession));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

    session_controller_ = std::make_unique<TestSessionController>();

    EXPECT_CALL(mock_nearby_process_manager(), GetNearbyProcessReference)
        .WillRepeatedly(
            [&](ash::nearby::NearbyProcessManager::NearbyProcessStoppedCallback
                    process_stopped_callback) {
              process_stopped_callback_ = std::move(process_stopped_callback);
              auto mock_reference_ptr =
                  std::make_unique<ash::nearby::MockNearbyProcessManager::
                                       MockNearbyProcessReference>();

              EXPECT_CALL(*(mock_reference_ptr.get()), GetNearbySharingDecoder)
                  .WillRepeatedly(
                      testing::ReturnRef(mock_decoder_.shared_remote()));

              return mock_reference_ptr;
            });

    SetFakeFastInitiationScannerFactory();
    service_ = CreateService();
    SetFakeFastInitiationAdvertiserFactory(/*should_succeed_on_start=*/true);

    // Visibility timer starts on |service_| creation because default visibility
    // is 'Your Devices'.
    EXPECT_TRUE(IsVisibilityReminderTimerRunning());

    service_->set_free_disk_space_for_testing(kFreeDiskSpace);

    // From now on we don't allow any blocking tasks anymore.
    disallow_blocking_ = std::make_unique<base::ScopedDisallowBlocking>();
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
    }

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
    FastInitiationAdvertiser::Factory::SetFactoryForTesting(nullptr);
  }

  void SetManagedEnabled(bool is_enabled) {
    prefs_.SetManagedPref(prefs::kNearbySharingEnabledPrefName,
                          std::make_unique<base::Value>(is_enabled));
    ASSERT_TRUE(
        prefs_.IsManagedPreference(prefs::kNearbySharingEnabledPrefName));
  }

  std::unique_ptr<NearbySharingServiceImpl> CreateService() {
    NearbySharingServiceFactory::
        SetIsNearbyShareSupportedForBrowserContextForTesting(true);

    profile_ = profile_manager_.CreateTestingProfile(kProfileName);
    prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, true);

    fake_nearby_connections_manager_ = new FakeNearbyConnectionsManager();
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
    NotificationDisplayService* notification_display_service =
        NotificationDisplayServiceFactory::GetForProfile(profile_);
    auto power_client = std::make_unique<FakePowerClient>();
    power_client_ = power_client.get();
    auto wifi_network_handler =
        std::make_unique<FakeWifiNetworkConfigurationHandler>();
    wifi_network_handler_ = wifi_network_handler.get();
    auto service = std::make_unique<NearbySharingServiceImpl>(
        &prefs_, notification_display_service, profile_,
        base::WrapUnique(fake_nearby_connections_manager_.get()),
        &mock_nearby_process_manager_, std::move(power_client),
        std::move(wifi_network_handler));

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

    // This ensures that the change propagates through mojo and the observers
    // are called.
    base::RunLoop().RunUntilIdle();
  }

  nearby_share::mojom::Visibility GetVisibility() {
    NearbyShareSettings settings(&prefs_, local_device_data_manager());
    return settings.GetVisibility();
  }

  void SetIsEnabled(bool is_enabled) {
    NearbyShareSettings settings(&prefs_, local_device_data_manager());
    if (is_enabled) {
      settings.SetIsOnboardingComplete(is_enabled);
    }
    settings.SetEnabled(is_enabled);

    // This ensures that the change propagates through mojo and the observers
    // are called.
    base::RunLoop().RunUntilIdle();
  }

  void SetFastInitiationNotificationState(
      nearby_share::mojom::FastInitiationNotificationState state) {
    NearbyShareSettings settings(&prefs_, local_device_data_manager());
    settings.SetFastInitiationNotificationState(state);

    // This ensures that the change propagates through mojo and the observers
    // are called.
    base::RunLoop().RunUntilIdle();
  }

  void SetFakeFastInitiationAdvertiserFactory(bool should_succeed_on_start) {
    fast_initiation_advertiser_factory_ =
        std::make_unique<FakeFastInitiationAdvertiserFactory>(
            should_succeed_on_start);
    FastInitiationAdvertiser::Factory::SetFactoryForTesting(
        fast_initiation_advertiser_factory_.get());
  }

  void SetFakeFastInitiationScannerFactory() {
    fast_initiation_scanner_factory_ =
        std::make_unique<FakeFastInitiationScannerFactory>();
    FastInitiationScanner::Factory::SetFactoryForTesting(
        fast_initiation_scanner_factory_.get());
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

  void SetHardwareSupportState(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
          state) {
    hardware_support_state_ = state;
    adapter_observer_->LowEnergyScanSessionHardwareOffloadingStatusChanged(
        state);
  }

  void AddAdapterObserver(device::BluetoothAdapter::Observer* observer) {
    DCHECK(!adapter_observer_);
    adapter_observer_ = observer;
  }

  void OnSetAdvertisingInterval(int64_t min, int64_t max) {
    ++set_advertising_interval_call_count_;
    last_advertising_interval_min_ = min;
    last_advertising_interval_max_ = max;
  }

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
    auto mock_scan_session =
        std::make_unique<device::MockBluetoothLowEnergyScanSession>(
            base::BindOnce(
                &NearbySharingServiceImplTestBase::OnScanSessionDestroyed,
                weak_ptr_factory_.GetWeakPtr()));
    mock_scan_session_ = mock_scan_session.get();
    return mock_scan_session;
  }

  void OnScanSessionDestroyed() { mock_scan_session_ = nullptr; }

  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    network_notifier_->SetConnectionType(type);
    network_notifier_->NotifyObserversOfNetworkChangeForTests(
        network_notifier_->GetConnectionType());
  }

  NiceMock<ash::nearby::MockNearbyProcessManager>&
  mock_nearby_process_manager() {
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
                                                bool success,
                                                bool for_self_share = false) {
    // Ensure that all pending mojo messages are processed and the certificate
    // manager state is as expected up to this point.
    base::RunLoop().RunUntilIdle();
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
      nearby::sharing::proto::PublicCertificate cert =
          GetNearbyShareTestPublicCertificate(
              nearby_share::mojom::Visibility::kAllContacts);
      cert.set_for_self_share(for_self_share);
      std::move(calls.back().callback)
          .Run(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              cert, GetNearbyShareTestEncryptedMetadataKey()));
    } else {
      std::move(calls.back().callback).Run(std::nullopt);
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
                ash::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                    callback) {
              sharing::mojom::V1FramePtr mojo_v1frame =
                  sharing::mojom::V1Frame::NewPairedKeyEncryption(
                      sharing::mojom::PairedKeyEncryptionFrame::New(
                          is_incoming ? kIncomingConnectionSignedData
                                      : kOutgoingConnectionSignedData,
                          kPrivateCertificateHashAuthToken, std::nullopt));
              sharing::mojom::FramePtr mojo_frame =
                  sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));
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
                ash::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                    callback) {
              sharing::mojom::V1FramePtr mojo_v1frame =
                  sharing::mojom::V1Frame::NewPairedKeyResult(
                      sharing::mojom::PairedKeyResultFrame::New(status));

              sharing::mojom::FramePtr mojo_frame =
                  sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));
              std::move(callback).Run(std::move(mojo_frame));
            }));
    connection_.AppendReadableData(result_bytes);
  }

  void SetUpAdvertisementDecoder(const std::vector<uint8_t>& endpoint_info,
                                 bool return_empty_advertisement,
                                 bool return_empty_device_name,
                                 size_t expected_number_of_calls) {
    EXPECT_CALL(mock_decoder_,
                DecodeAdvertisement(testing::Eq(endpoint_info), testing::_))
        .Times(expected_number_of_calls)
        .WillRepeatedly(
            testing::Invoke([=](const std::vector<uint8_t>& data,
                                ash::nearby::MockNearbySharingDecoder::
                                    DecodeAdvertisementCallback callback) {
              if (return_empty_advertisement) {
                std::move(callback).Run(nullptr);
                return;
              }

              std::optional<std::string> device_name;
              if (!return_empty_device_name) {
                device_name = kDeviceName;
              }

              sharing::mojom::AdvertisementPtr advertisement =
                  sharing::mojom::Advertisement::New(
                      GetNearbyShareTestEncryptedMetadataKey().salt(),
                      GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
                      kDeviceType, device_name);
              std::move(callback).Run(std::move(advertisement));
            }));
  }

  // By default, set up the decoder to return an introduction frame with every
  // attachment type.
  void SetUpIntroductionFrameDecoder(int text_metadata_count = 3,
                                     int file_metadata_count = 1,
                                     int wifi_credentials_metadata_count = 1) {
    std::string intro = "introduction_frame";
    std::vector<uint8_t> bytes(intro.begin(), intro.end());
    EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
        .WillOnce(testing::Invoke(
            [=](const std::vector<uint8_t>& data,
                ash::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                    callback) {
              std::move(callback).Run(
                  GetIntroductionFrame(text_metadata_count, file_metadata_count,
                                       wifi_credentials_metadata_count));
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
                ash::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                    callback) {
              std::move(callback).Run(GetConnectionResponseFrame(status));
            }));
    connection_.AppendReadableData(bytes);
  }

  void SendCancel() {
    std::string data = "cancel_frame";
    std::vector<uint8_t> bytes(data.begin(), data.end());
    EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
        .WillOnce(testing::Invoke(
            [=](const std::vector<uint8_t>& data,
                ash::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                    callback) { std::move(callback).Run(GetCancelFrame()); }));
    connection_.AppendReadableData(bytes);
  }

  ShareTarget SetUpIncomingConnection(
      NiceMock<MockTransferUpdateCallback>& callback,
      bool for_self_share = false,
      int wifi_credentials_metadata_count = 1) {
    fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                                kToken);
    SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                              /*return_empty_advertisement=*/false,
                              /*return_empty_device_name=*/false,
                              /*expected_number_of_calls=*/1u);
    SetUpIntroductionFrameDecoder(
        /*text_metadata_count=*/3,
        /*file_metadata_count=*/1,
        /*wifi_credentials_metadata_count=*/wifi_credentials_metadata_count);

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
    service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                           &connection_);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true, for_self_share);
    run_loop.Run();

    EXPECT_TRUE(
        fake_nearby_connections_manager_->DidUpgradeBandwidth(kEndpointId));
    EXPECT_EQ((uint8_t)wifi_credentials_metadata_count,
              share_target.wifi_credentials_attachments.size());

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
                              /*return_empty_advertisement=*/false,
                              /*return_empty_device_name=*/false,
                              /*expected_number_of_calls=*/1u);

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
        kEndpointId, nearby::connections::mojom::DiscoveredEndpointInfo::New(
                         kValidV1EndpointInfo, kServiceId));
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    run_loop.Run();
    return discovered_target;
  }

  std::optional<ShareTarget> CreateShareTarget(
      const sharing::mojom::AdvertisementPtr& advertisement,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
    return service_->CreateShareTarget(kEndpointId, advertisement, certificate,
                                       /*is_incoming=*/true);
  }

  nearby::sharing::service::proto::Frame GetWrittenFrame() {
    std::vector<uint8_t> data = connection_.GetWrittenData();
    nearby::sharing::service::proto::Frame frame;
    frame.ParseFromArray(data.data(), data.size());
    return frame;
  }

  void ExpectPairedKeyEncryptionFrame() {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_encryption());
  }

  void ExpectPairedKeyResultFrame() {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_result());
  }

  void ExpectConnectionResponseFrame(
      nearby::sharing::service::proto::ConnectionResponseFrame::Status status) {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_connection_response());
    EXPECT_EQ(status, frame.v1().connection_response().status());
  }

  nearby::sharing::service::proto::IntroductionFrame ExpectIntroductionFrame() {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    EXPECT_TRUE(frame.has_v1());
    EXPECT_TRUE(frame.v1().has_introduction());
    return frame.v1().introduction();
  }

  void ExpectCancelFrame() {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    EXPECT_EQ(nearby::sharing::service::proto::V1Frame::CANCEL,
              frame.v1().type());
  }

  // Optionally, |new_share_target| is updated with the ShareTargets sent to
  // OnTransferUpdate() calls.
  void ExpectTransferUpdates(
      MockTransferUpdateCallback& transfer_callback,
      const ShareTarget& target,
      const std::vector<TransferMetadata::Status>& updates,
      base::OnceClosure callback,
      ShareTarget* new_share_target = nullptr) {
    auto barrier = base::BarrierClosure(updates.size(), std::move(callback));
    auto& expectation =
        EXPECT_CALL(transfer_callback, OnTransferUpdate).Times(updates.size());

    for (TransferMetadata::Status status : updates) {
      expectation.WillOnce(testing::Invoke([=, this](
                                               const ShareTarget& share_target,
                                               TransferMetadata metadata) {
        EXPECT_EQ(target.id, share_target.id);
        EXPECT_EQ(status, metadata.status());
        if (new_share_target) {
          *new_share_target = share_target;
        }

        // Though this is indirect, verify that the highest level
        // success/failure metric was logged. We expect transfer updates to
        // be a few indeterminate status then only one success or failure
        // status.
        TransferMetadata::Result result = TransferMetadata::ToResult(status);
        histogram_tester_.ExpectBucketCount(
            "ChromeOS.FeatureUsage.NearbyShare",
            ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess,
            result == TransferMetadata::Result::kSuccess ? 1 : 0);
        histogram_tester_.ExpectBucketCount(
            "ChromeOS.FeatureUsage.NearbyShare",
            ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure,
            result == TransferMetadata::Result::kFailure ? 1 : 0);

        barrier.Run();
      }));
    }
  }

  // Returns the modified ShareTarget received from a TransferUpdate.
  ShareTarget SetUpOutgoingConnectionUntilAccept(
      MockTransferUpdateCallback& transfer_callback,
      const ShareTarget& target) {
    ShareTarget new_share_target;
    base::RunLoop introduction_run_loop;
    ExpectTransferUpdates(transfer_callback, target,
                          {TransferMetadata::Status::kConnecting,
                           TransferMetadata::Status::kAwaitingLocalConfirmation,
                           TransferMetadata::Status::kAwaitingRemoteAcceptance},
                          introduction_run_loop.QuitClosure(),
                          &new_share_target);

    EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
              service_->SendAttachments(target,
                                        CreateTextAttachments({kTextPayload})));
    introduction_run_loop.Run();

    // Verify data sent to the remote device so far.
    ExpectPairedKeyEncryptionFrame();
    ExpectPairedKeyResultFrame();
    ExpectIntroductionFrame();

    return new_share_target;
  }

  struct PayloadInfo {
    int64_t payload_id;
    base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener;
  };

  PayloadInfo AcceptAndSendPayload(
      MockTransferUpdateCallback& transfer_callback,
      const ShareTarget& target) {
    PayloadInfo info = {};
    base::RunLoop payload_run_loop;
    fake_nearby_connections_manager_->set_send_payload_callback(
        base::BindLambdaForTesting(
            [&](NearbyConnectionsManager::PayloadPtr payload,
                base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener>
                    listener) {
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
        nearby::connections::mojom::PayloadTransferUpdate::New(
            info.payload_id,
            nearby::connections::mojom::PayloadStatus::kSuccess,
            /*total_bytes=*/strlen(kTextPayload),
            /*bytes_transferred=*/strlen(kTextPayload)),
        /*upgraded_medium=*/std::nullopt);
    success_run_loop.Run();
  }

  std::unique_ptr<sharing::Advertisement> GetCurrentAdvertisement() {
    auto endpoint_info =
        fake_nearby_connections_manager_->advertising_endpoint_info();
    if (!endpoint_info) {
      return nullptr;
    }

    return sharing::AdvertisementDecoder::FromEndpointInfo(base::make_span(
        *fake_nearby_connections_manager_->advertising_endpoint_info()));
  }

  void FindEndpoint(const std::string& endpoint_id) {
    fake_nearby_connections_manager_->OnEndpointFound(
        endpoint_id, nearby::connections::mojom::DiscoveredEndpointInfo::New(
                         kValidV1EndpointInfo, kServiceId));
  }

  void LoseEndpoint(const std::string& endpoint_id) {
    fake_nearby_connections_manager_->OnEndpointLost(endpoint_id);
  }

  // This method sets up an incoming connection and performs the steps required
  // to simulate a successful incoming transfer.
  void SuccessfullyReceiveTransfer() {
    for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
      fake_nearby_connections_manager_->SetPayloadPathStatus(
          payload_id, nearby::connections::mojom::Status::kSuccess);
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

    service_->Accept(
        share_target,
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
      if (id == kFilePayloadId) {
        continue;
      }

      if (id != kWifiCredentialsPayloadId) {
        fake_nearby_connections_manager_->SetIncomingPayload(
            id, GetTextPayloadPtr(id, kTextPayload));
      }

      if (id == kWifiCredentialsPayloadId) {
        fake_nearby_connections_manager_->SetIncomingPayload(
            id, GetWifiPayloadPtr(id, kWifiPassword));
      }

      base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
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

      nearby::connections::mojom::PayloadTransferUpdatePtr payload =
          nearby::connections::mojom::PayloadTransferUpdate::New(
              id, nearby::connections::mojom::PayloadStatus::kSuccess,
              /*total_bytes=*/kPayloadSize,
              /*bytes_transferred=*/kPayloadSize);
      listener->OnStatusUpdate(std::move(payload),
                               /*upgraded_medium=*/std::nullopt);
      run_loop_progress.Run();

      task_environment_.FastForwardBy(kMinProgressUpdateFrequency);
    }

    base::FilePath file_path;
    base::RunLoop run_loop_success;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                      TransferMetadata metadata) {
          EXPECT_TRUE(metadata.is_final_status());
          EXPECT_EQ(metadata.progress(), 100);
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

          EXPECT_EQ(1u, share_target.wifi_credentials_attachments.size());
          for (const WifiCredentialsAttachment& wifi_credentials :
               share_target.wifi_credentials_attachments) {
            EXPECT_EQ(kSsid, wifi_credentials.ssid());
            EXPECT_EQ(kWifiPassword, wifi_credentials.wifi_password());
            EXPECT_EQ(kWifiSecurityType, wifi_credentials.security_type());
          }

          EXPECT_EQ(1u, wifi_network_handler_->num_configure_network_calls());
          EXPECT_EQ(kSsid, wifi_network_handler_->last_attachment().ssid());
          EXPECT_EQ(kWifiPassword,
                    wifi_network_handler_->last_attachment().wifi_password());
          EXPECT_EQ(kWifiSecurityType,
                    wifi_network_handler_->last_attachment().security_type());

          run_loop_success.Quit();
        }));

    base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
        fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
            kFilePayloadId);
    ASSERT_TRUE(listener);

    nearby::connections::mojom::PayloadTransferUpdatePtr payload =
        nearby::connections::mojom::PayloadTransferUpdate::New(
            kFilePayloadId, nearby::connections::mojom::PayloadStatus::kSuccess,
            /*total_bytes=*/kPayloadSize,
            /*bytes_transferred=*/kPayloadSize);
    listener->OnStatusUpdate(std::move(payload),
                             /*upgraded_medium=*/std::nullopt);
    run_loop_success.Run();

    EXPECT_FALSE(fake_nearby_connections_manager_->connection_endpoint_info(
        kEndpointId));
    EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

    // TODO(crbug.com/40716484): This check is flaky, should be investigated.
    // EXPECT_TRUE(FileExists(file_path));

    // To avoid UAF in OnIncomingTransferUpdate().
    service_->UnregisterReceiveSurface(&callback);

    // Remove test file.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::DeleteFile(file_path);
    }
  }

  void ReceiveBadWifiPayload(
      nearby::connections::mojom::PayloadPtr wifi_payload) {
    for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
      fake_nearby_connections_manager_->SetPayloadPathStatus(
          payload_id, nearby::connections::mojom::Status::kSuccess);
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

    service_->Accept(
        share_target,
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
      if (id == kFilePayloadId) {
        continue;
      }

      if (id != kWifiCredentialsPayloadId) {
        fake_nearby_connections_manager_->SetIncomingPayload(
            id, GetTextPayloadPtr(id, kTextPayload));
      } else {
        fake_nearby_connections_manager_->SetIncomingPayload(
            id, std::move(wifi_payload));
      }

      base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
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

      nearby::connections::mojom::PayloadTransferUpdatePtr payload =
          nearby::connections::mojom::PayloadTransferUpdate::New(
              id, nearby::connections::mojom::PayloadStatus::kSuccess,
              /*total_bytes=*/kPayloadSize,
              /*bytes_transferred=*/kPayloadSize);
      listener->OnStatusUpdate(std::move(payload),
                               /*upgraded_medium=*/std::nullopt);
      run_loop_progress.Run();

      task_environment_.FastForwardBy(kMinProgressUpdateFrequency);
    }

    base::RunLoop run_loop_success;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                      TransferMetadata metadata) {
          EXPECT_TRUE(metadata.is_final_status());
          EXPECT_LT(metadata.progress(), 100);
          EXPECT_EQ(TransferMetadata::Status::kIncompletePayloads,
                    metadata.status());

          ASSERT_TRUE(share_target.has_attachments());
          EXPECT_EQ(0u, wifi_network_handler_->num_configure_network_calls());

          run_loop_success.Quit();
        }));

    base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
        fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
            kFilePayloadId);
    ASSERT_TRUE(listener);

    nearby::connections::mojom::PayloadTransferUpdatePtr payload =
        nearby::connections::mojom::PayloadTransferUpdate::New(
            kFilePayloadId, nearby::connections::mojom::PayloadStatus::kSuccess,
            /*total_bytes=*/kPayloadSize,
            /*bytes_transferred=*/kPayloadSize);
    listener->OnStatusUpdate(std::move(payload),
                             /*upgraded_medium=*/std::nullopt);

    run_loop_success.Run();

    EXPECT_FALSE(fake_nearby_connections_manager_->connection_endpoint_info(
        kEndpointId));
    EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

    // To avoid UAF in OnIncomingTransferUpdate().
    service_->UnregisterReceiveSurface(&callback);
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
    base::FilePath path = temp_dir_.GetPath().Append(name);
    base::File file(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                              base::File::Flags::FLAG_READ |
                              base::File::Flags::FLAG_WRITE);
    EXPECT_TRUE(file.WriteAndCheck(
        /*offset=*/0, base::make_span(content)));
    EXPECT_TRUE(file.Flush());
    file.Close();
    return path;
  }

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

  size_t set_advertising_interval_call_count() {
    return set_advertising_interval_call_count_;
  }

  int64_t last_advertising_interval_min() {
    return last_advertising_interval_min_;
  }

  int64_t last_advertising_interval_max() {
    return last_advertising_interval_max_;
  }

  bool IsProcessShutdownTimerRunning() {
    return service_->process_shutdown_pending_timer_.IsRunning();
  }

  void FireProcessShutdownIfRunning() {
    if (IsProcessShutdownTimerRunning()) {
      service_->process_shutdown_pending_timer_.FireNow();
    }
  }

  bool IsBoundToProcess() { return service_->process_reference_ != nullptr; }

  void SetRecentNearbyProcessShutdownCount(int count) {
    service_->recent_nearby_process_unexpected_shutdown_count_ = count;
  }

  bool IsVisibilityReminderTimerRunning() {
    return service_->visibility_reminder_timer_.IsRunning();
  }

  void ShutdownNearbyProcess(NearbyProcessShutdownReason shutdown_reason) {
    fake_nearby_connections_manager_->CleanupForProcessStopped();
    std::move(process_stopped_callback_).Run(shutdown_reason);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  // We need to ensure that |network_notifier_| is created and destroyed after
  // |task_environment_| to avoid UAF issues when using
  // ChromeDownloadManagerDelegate.
  std::unique_ptr<net::test::MockNetworkChangeNotifier> network_notifier_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<Profile> profile_ = nullptr;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  raw_ptr<FakeNearbyConnectionsManager, DanglingUntriaged>
      fake_nearby_connections_manager_ = nullptr;
  raw_ptr<FakePowerClient, DanglingUntriaged> power_client_ = nullptr;
  raw_ptr<FakeWifiNetworkConfigurationHandler, DanglingUntriaged>
      wifi_network_handler_ = nullptr;
  FakeNearbyShareLocalDeviceDataManager::Factory
      local_device_data_manager_factory_;
  FakeNearbyShareContactManager::Factory contact_manager_factory_;
  FakeNearbyShareCertificateManager::Factory certificate_manager_factory_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  NiceMock<ash::nearby::MockNearbyProcessManager> mock_nearby_process_manager_;
  std::unique_ptr<TestSessionController> session_controller_;
  std::unique_ptr<NearbySharingServiceImpl> service_;
  std::unique_ptr<base::ScopedDisallowBlocking> disallow_blocking_;
  std::unique_ptr<FakeFastInitiationAdvertiserFactory>
      fast_initiation_advertiser_factory_;
  std::unique_ptr<FakeFastInitiationScannerFactory>
      fast_initiation_scanner_factory_;
  bool is_bluetooth_present_ = true;
  bool is_bluetooth_powered_ = true;
  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
      hardware_support_state_;
  raw_ptr<device::BluetoothAdapter::Observer, DanglingUntriaged>
      adapter_observer_ = nullptr;
  scoped_refptr<NiceMock<MockBluetoothAdapterWithIntervals>>
      mock_bluetooth_adapter_;
  raw_ptr<device::MockBluetoothLowEnergyScanSession> mock_scan_session_ =
      nullptr;
  NiceMock<ash::nearby::MockNearbySharingDecoder> mock_decoder_;
  FakeNearbyConnection connection_;
  size_t set_advertising_interval_call_count_ = 0u;
  int64_t last_advertising_interval_min_ = 0;
  int64_t last_advertising_interval_max_ = 0;
  ash::nearby::NearbyProcessManager::NearbyProcessStoppedCallback
      process_stopped_callback_;
  base::HistogramTester histogram_tester_;

  base::WeakPtrFactory<NearbySharingServiceImplTestBase> weak_ptr_factory_{
      this};
};

// We parameterize these tests to run them with upcoming features enabled and
// disabled.
class NearbySharingServiceImplTest
    : public NearbySharingServiceImplTestBase,
      public testing::WithParamInterface<size_t> {
 public:
  NearbySharingServiceImplTest()
      : NearbySharingServiceImplTestBase(/*feature_mask=*/GetParam()) {}
};

struct ValidSendSurfaceTestDataInternal {
  bool bluetooth_enabled;
  net::NetworkChangeNotifier::ConnectionType connection_type;
} kValidSendSurfaceTestData[] = {
    // No network connection, only bluetooth available
    {true, net::NetworkChangeNotifier::CONNECTION_NONE},
    // Bluetooth & Wifi available
    {true, net::NetworkChangeNotifier::CONNECTION_WIFI},
    // Bluetooth & Ethernet available
    {true, net::NetworkChangeNotifier::CONNECTION_ETHERNET},
    // Bluetooth & 3G available
    {true, net::NetworkChangeNotifier::CONNECTION_3G},
    // No Bluetooth, Wifi available
    {false, net::NetworkChangeNotifier::CONNECTION_WIFI},
    // No Bluetooth, Ethernet available
    {false, net::NetworkChangeNotifier::CONNECTION_ETHERNET},
};

// size_t parameter is |feature_mask|.
using ValidSendSurfaceTestData =
    std::tuple<ValidSendSurfaceTestDataInternal, size_t>;

class NearbySharingServiceImplValidSendTest
    : public NearbySharingServiceImplTestBase,
      public testing::WithParamInterface<ValidSendSurfaceTestData> {
 public:
  NearbySharingServiceImplValidSendTest()
      : NearbySharingServiceImplTestBase(
            /*feature_mask=*/std::get<1>(GetParam())) {}
};

struct InvalidSendSurfaceTestDataInternal {
  bool screen_locked;
  bool bluetooth_enabled;
  net::NetworkChangeNotifier::ConnectionType connection_type;
} kInvalidSendSurfaceTestData[] = {
    // Screen locked
    {/*screen_locked=*/true, true, net::NetworkChangeNotifier::CONNECTION_WIFI},
    // No network connection and no bluetooth
    {/*screen_locked=*/false, false,
     net::NetworkChangeNotifier::CONNECTION_NONE},
    // 3G available and no bluetooth
    {/*screen_locked=*/false, false, net::NetworkChangeNotifier::CONNECTION_3G},
};

// size_t parameter is |feature_mask|.
using InvalidSendSurfaceTestData =
    std::tuple<InvalidSendSurfaceTestDataInternal, size_t>;

class NearbySharingServiceImplInvalidSendTest
    : public NearbySharingServiceImplTestBase,
      public testing::WithParamInterface<InvalidSendSurfaceTestData> {
 public:
  NearbySharingServiceImplInvalidSendTest()
      : NearbySharingServiceImplTestBase(
            /*feature_mask==*/std::get<1>(GetParam())) {}
};

using ResponseFrameStatus = sharing::mojom::ConnectionResponseFrame::Status;

struct SendFailureTestDataInternal {
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

// size_t parameter is |feature_mask|.
using SendFailureTestData = std::tuple<SendFailureTestDataInternal, size_t>;

class NearbySharingServiceImplSendFailureTest
    : public NearbySharingServiceImplTestBase,
      public testing::WithParamInterface<SendFailureTestData> {
 public:
  NearbySharingServiceImplSendFailureTest()
      : NearbySharingServiceImplTestBase(
            /*feature_mask=*/std::get<1>(GetParam())) {}
};

class TestObserver : public NearbySharingService::Observer {
 public:
  explicit TestObserver(NearbySharingService* service) : service_(service) {
    service_->AddObserver(this);
  }

  void OnHighVisibilityChanged(bool in_high_visibility) override {
    in_high_visibility_ = in_high_visibility;
  }

  void OnNearbyProcessStopped() override { process_stopped_called_ = true; }

  void OnStartAdvertisingFailure() override {
    on_start_advertising_failure_called_ = true;
  }

  void OnFastInitiationDevicesDetected() override {
    devices_detected_called_ = true;
  }
  void OnFastInitiationDevicesNotDetected() override {
    devices_not_detected_called_ = true;
  }
  void OnFastInitiationScanningStopped() override {
    scanning_stopped_called_ = true;
  }

  void OnShutdown() override {
    shutdown_called_ = true;
    service_->RemoveObserver(this);
  }

  bool in_high_visibility_ = false;
  bool shutdown_called_ = false;
  bool process_stopped_called_ = false;
  bool on_start_advertising_failure_called_ = false;
  bool devices_detected_called_ = false;
  bool devices_not_detected_called_ = false;
  bool scanning_stopped_called_ = false;
  raw_ptr<NearbySharingService, DanglingUntriaged> service_;
};

TEST_P(NearbySharingServiceImplTest, DisableNearbyShutdownConnections) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);
  service_->FlushMojoForTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_P(NearbySharingServiceImplTest, StartFastInitiationAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_EQ(1u, fast_initiation_advertiser_factory_->StartAdvertisingCount());

  // Call RegisterSendSurface a second time and make sure StartAdvertising is
  // not called again.
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kError,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_EQ(1u, fast_initiation_advertiser_factory_->StartAdvertisingCount());
}

TEST_P(NearbySharingServiceImplTest, StartFastInitiationAdvertisingError) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetFakeFastInitiationAdvertiserFactory(/*should_succeed_on_start=*/false);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
}

TEST_P(NearbySharingServiceImplTest,
       BackgroundStartFastInitiationAdvertisingError) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kBackground));
  EXPECT_EQ(0u, fast_initiation_advertiser_factory_->StartAdvertisingCount());
}

TEST_P(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPresent) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetBluetoothIsPresent(false);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  // We can register a send surface via mDNS, but not send fast initiation
  // advertisements over BLE.
  EXPECT_EQ(0u, fast_initiation_advertiser_factory_->StartAdvertisingCount());
}

TEST_P(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPowered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  is_bluetooth_powered_ = false;
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  // We can register a send surface via mDNS, but not send fast initiation
  // advertisements over BLE.
  EXPECT_EQ(0u, fast_initiation_advertiser_factory_->StartAdvertisingCount());
}

TEST_P(NearbySharingServiceImplTest, StopFastInitiationAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_EQ(1u, fast_initiation_advertiser_factory_->StartAdvertisingCount());
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->UnregisterSendSurface(&transfer_callback, &discovery_callback));
  EXPECT_TRUE(fast_initiation_advertiser_factory_
                  ->StopAdvertisingCalledAndAdvertiserDestroyed());
}

TEST_P(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPresent) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  SetBluetoothIsPresent(false);
  EXPECT_TRUE(fast_initiation_advertiser_factory_
                  ->StopAdvertisingCalledAndAdvertiserDestroyed());
}

TEST_P(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPowered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  SetBluetoothIsPowered(false);
  EXPECT_TRUE(fast_initiation_advertiser_factory_
                  ->StopAdvertisingCalledAndAdvertiserDestroyed());
}

TEST_P(NearbySharingServiceImplTest, RegisterSendSurface_BluetoothNotPresent) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  SetBluetoothIsPresent(false);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kNoAvailableConnectionMedium,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_P(NearbySharingServiceImplTest, RegisterSendSurface_BluetoothNotPowered) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  is_bluetooth_powered_ = false;
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kNoAvailableConnectionMedium,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
       RegisterSendSurfaceEndpointFoundDiscoveryCallbackNotified) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Ensure decoder parses a valid endpoint advertisement.
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

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
        EXPECT_FALSE(share_target.for_self_share);
        run_loop.Quit();
      });
  fake_nearby_connections_manager_->OnEndpointFound(
      kEndpointId, nearby::connections::mojom::DiscoveredEndpointInfo::New(
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

  // Shut down the service while the discovery callbacks are still in scope.
  // OnShareTargetLost() will be invoked during shutdown.
  service_->Shutdown();
  service_.reset();
}

TEST_P(NearbySharingServiceImplTest, RegisterSendSurfaceEmptyCertificate) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Ensure decoder parses a valid endpoint advertisement.
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

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
        EXPECT_FALSE(share_target.for_self_share);
        run_loop.Quit();
      });
  fake_nearby_connections_manager_->OnEndpointFound(
      kEndpointId, nearby::connections::mojom::DiscoveredEndpointInfo::New(
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

  // Shut down the service while the discovery callbacks are still in scope.
  // OnShareTargetLost() will be invoked during shutdown.
  service_->Shutdown();
  service_.reset();
}

TEST_P(NearbySharingServiceImplValidSendTest,
       RegisterSendSurfaceIsDiscovering) {
  is_bluetooth_present_ = std::get<0>(GetParam()).bluetooth_enabled;
  SetConnectionType(std::get<0>(GetParam()).connection_type);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

INSTANTIATE_TEST_SUITE_P(
    NearbySharingServiceImplTest,
    NearbySharingServiceImplValidSendTest,
    testing::Combine(testing::ValuesIn(kValidSendSurfaceTestData),
                     testing::Range<size_t>(0, 1 << kTestFeatures.size())));

TEST_P(NearbySharingServiceImplInvalidSendTest,
       RegisterSendSurfaceNotDiscovering) {
  session_controller_->SetScreenLocked(std::get<0>(GetParam()).screen_locked);
  is_bluetooth_present_ = std::get<0>(GetParam()).bluetooth_enabled;
  net::NetworkChangeNotifier::ConnectionType connection_type =
      std::get<0>(GetParam()).connection_type;
  SetConnectionType(connection_type);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());

  // Either Bluetooth or a Wifi/Ethernet network must be available to
  // register a send surface.
  NearbySharingService::StatusCodes expected_status =
      (is_bluetooth_present_ ||
       connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
       connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI)
          ? NearbySharingService::StatusCodes::kOk
          : NearbySharingService::StatusCodes::kNoAvailableConnectionMedium;

  EXPECT_EQ(expected_status, service_->RegisterSendSurface(
                                 &transfer_callback, &discovery_callback,
                                 SendSurfaceState::kForeground));

  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

INSTANTIATE_TEST_SUITE_P(
    NearbySharingServiceImplTest,
    NearbySharingServiceImplInvalidSendTest,
    testing::Combine(testing::ValuesIn(kInvalidSendSurfaceTestData),
                     testing::Range<size_t>(0, 1 << kTestFeatures.size())));

TEST_P(NearbySharingServiceImplTest, DisableFeatureSendSurfaceNotDiscovering) {
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest, UnregisterSendSurfaceStopsDiscovering) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->UnregisterSendSurface(&transfer_callback, &discovery_callback));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest, UnregisterSendSurfaceNeverRegistered) {
  // Set visibility to Hidden to stop advertising.
  SetVisibility(nearby_share::mojom::Visibility::kNoOne);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kError,
      service_->UnregisterSendSurface(&transfer_callback, &discovery_callback));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_TRUE(IsBoundToProcess());
  FireProcessShutdownIfRunning();
  EXPECT_FALSE(IsBoundToProcess());
}

TEST_P(NearbySharingServiceImplTest,
       ForegroundRegisterReceiveSurfaceIsAdvertisingAllContacts) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  local_device_data_manager()->SetDeviceName(kDeviceName);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(NearbyConnectionsManager::PowerLevel::kHighPower,
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

TEST_P(NearbySharingServiceImplTest,
       ForegroundRegisterReceiveSurfaceIsAdvertisingNoOne) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kNoOne);
  local_device_data_manager()->SetDeviceName(kDeviceName);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(NearbyConnectionsManager::PowerLevel::kHighPower,
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

TEST_P(NearbySharingServiceImplTest,
       BackgroundRegisterReceiveSurfaceIsAdvertisingSelectedContacts) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kSelectedContacts);
  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kSelectedContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(NearbyConnectionsManager::PowerLevel::kLowPower,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
       ScreenLockedRegisterReceiveSurfaceNotAdvertising) {
  session_controller_->SetScreenLocked(true);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_P(NearbySharingServiceImplTest, ScreenLocksDuringAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  session_controller_->SetScreenLocked(true);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  session_controller_->SetScreenLocked(false);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_P(NearbySharingServiceImplTest, ScreenLocksDuringDiscovery) {
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest, SuspendDuringAdvertising) {
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

TEST_P(NearbySharingServiceImplTest,
       DataUsageChangedRegisterReceiveSurfaceRestartsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  prefs_.SetInteger(prefs::kNearbySharingDataUsageName,
                    static_cast<int>(nearby_share::mojom::DataUsage::kOffline));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(nearby_share::mojom::DataUsage::kOffline,
            fake_nearby_connections_manager_->advertising_data_usage());

  prefs_.SetInteger(prefs::kNearbySharingDataUsageName,
                    static_cast<int>(nearby_share::mojom::DataUsage::kOnline));
  service_->FlushMojoForTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(nearby_share::mojom::DataUsage::kOnline,
            fake_nearby_connections_manager_->advertising_data_usage());
}

TEST_P(
    NearbySharingServiceImplTest,
    UnregisterForegroundReceiveSurfaceVisibilityAllContactsRestartAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));
  service_->FlushMojoForTesting();

  // Register both foreground and background receive surfaces
  MockTransferUpdateCallback background_transfer_callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &background_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  MockTransferUpdateCallback foreground_transfer_callback;
  result = service_->RegisterReceiveSurface(
      &foreground_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  // Unregister the foreground surface. Advertising is stopped and restarted
  // with low power. The service reports InHighVisibility until the
  // StopAdvertising callback is called.
  FakeNearbyConnectionsManager::ConnectionsCallback stop_advertising_callback =
      fake_nearby_connections_manager_->GetStopAdvertisingCallback();
  FakeNearbyConnectionsManager::ConnectionsCallback start_advertising_callback =
      fake_nearby_connections_manager_->GetStartAdvertisingCallback();
  result = service_->UnregisterReceiveSurface(&foreground_transfer_callback);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_TRUE(service_->IsInHighVisibility());

  std::move(stop_advertising_callback)
      .Run(NearbyConnectionsManager::ConnectionsStatus::kSuccess);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(service_->IsInHighVisibility());

  std::move(start_advertising_callback)
      .Run(NearbyConnectionsManager::ConnectionsStatus::kSuccess);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(service_->IsInHighVisibility());
}

TEST_P(NearbySharingServiceImplTest,
       NoNetworkRegisterReceiveSurfaceIsAdvertising) {
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  // Succeeds since bluetooth is present.
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       NoBluetoothNoNetworkRegisterForegroundReceiveSurfaceNotAdvertising) {
  SetBluetoothIsPresent(false);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_P(NearbySharingServiceImplTest,
       NoBluetoothNoNetworkRegisterBackgroundReceiveSurfaceWorks) {
  SetBluetoothIsPresent(false);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest, WifiRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       EthernetRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       ThreeGRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_3G);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  // Since bluetooth is on, connection still succeeds.
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       NoBluetoothWifiReceiveSurfaceIsAdvertising) {
  SetBluetoothIsPresent(false);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);

  // TODO(crbug.com/1129069): When WiFi LAN is supported we will expect this to
  // be true.
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       NoBluetoothEthernetReceiveSurfaceIsAdvertising) {
  SetBluetoothIsPresent(false);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);

  // TODO(crbug.com/1129069): When WiFi LAN is supported we will expect this to
  // be true.
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       NoBluetoothThreeGReceiveSurfaceNotAdvertising) {
  SetBluetoothIsPresent(false);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_3G);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceNoOneVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(nearby_share::mojom::Visibility::kNoOne));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceNoOneVisibilityNotAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(nearby_share::mojom::Visibility::kNoOne));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_P(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceVisibilityToNoOneStopsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kSelectedContacts));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(nearby_share::mojom::Visibility::kNoOne));
  service_->FlushMojoForTesting();
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_P(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceVisibilityToSelectedStartsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                    static_cast<int>(nearby_share::mojom::Visibility::kNoOne));
  service_->FlushMojoForTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kSelectedContacts));
  service_->FlushMojoForTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceSelectedContactsVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kSelectedContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceSelectedContactsVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kSelectedContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceAllContactsVisibilityIsAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceAllContactsVisibilityNotAdvertising) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  prefs_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_P(NearbySharingServiceImplTest, UnregisterReceiveSurfaceStopsAdvertising) {
  // Set visibility to Hidden to stop advertising.
  SetVisibility(nearby_share::mojom::Visibility::kNoOne);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  NearbySharingService::StatusCodes result2 =
      service_->UnregisterReceiveSurface(&callback);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
  EXPECT_TRUE(IsBoundToProcess());
  FireProcessShutdownIfRunning();
  EXPECT_FALSE(IsBoundToProcess());
}

TEST_P(NearbySharingServiceImplTest,
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

TEST_P(NearbySharingServiceImplTest, UnregisterReceiveSurfaceNeverRegistered) {
  // Set visibility to Hidden to stop advertising.
  SetVisibility(nearby_share::mojom::Visibility::kNoOne);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result =
      service_->UnregisterReceiveSurface(&callback);
  // This is no longer considered an error condition.
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_TRUE(IsBoundToProcess());
  FireProcessShutdownIfRunning();
  EXPECT_FALSE(IsBoundToProcess());
}

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_ClosedReadingIntroduction) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_)).Times(0);

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  // Ensure that the messages sent by ProcessLatestPublicCertificateDecryption
  // are processed prior to closing connection.
  base::RunLoop().RunUntilIdle();

  connection_.Close();

  // Introduction is ignored without any side effect.

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_EmptyIntroductionFrame) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);
  SetUpIntroductionFrameDecoder(
      /*text_metadata_count=*/0,
      /*file_metadata_count=*/0,
      /*wifi_credentials_metadata_count=*/0);

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
        EXPECT_FALSE(share_target.for_self_share);
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectConnectionResponseFrame(
      nearby::sharing::service::proto::ConnectionResponseFrame::
          UNSUPPORTED_ATTACHMENT_TYPE);

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_ValidIntroductionFrame_InvalidCertificate) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);
  SetUpIntroductionFrameDecoder();

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
        EXPECT_FALSE(share_target.for_self_share);
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  run_loop.Run();

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, IncomingConnection_TimedOut) {
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

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_ClosedWaitingLocalConfirmation) {
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);

  base::RunLoop run_loop_2;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop_2](const ShareTarget& share_target,
                                              TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kUnexpectedDisconnection,
                  metadata.status());
        run_loop_2.Quit();
      }));

  connection_.Close();
  run_loop_2.Run();

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, IncomingConnection_OutOfStorage) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

  // Set a huge file size in introduction frame to go out of storage.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .WillOnce(testing::Invoke([](const std::vector<uint8_t>& data,
                                   ash::nearby::MockNearbySharingDecoder::
                                       DecodeFrameCallback callback) {
        std::vector<sharing::mojom::FileMetadataPtr> mojo_file_metadatas;
        mojo_file_metadatas.push_back(sharing::mojom::FileMetadata::New(
            "name", sharing::mojom::FileMetadata::Type::kAudio,
            /*payload_id=*/1, kFreeDiskSpace + 1, "mime_type",
            /*id=*/123));

        sharing::mojom::V1FramePtr mojo_v1frame =
            sharing::mojom::V1Frame::NewIntroduction(
                sharing::mojom::IntroductionFrame::New(
                    std::move(mojo_file_metadatas),
                    std::vector<sharing::mojom::TextMetadataPtr>(),
                    /*required_package=*/std::nullopt,
                    std::vector<sharing::mojom::WifiCredentialsMetadataPtr>()));

        sharing::mojom::FramePtr mojo_frame =
            sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));

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
        EXPECT_EQ(0u, share_target.wifi_credentials_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);
        EXPECT_FALSE(share_target.for_self_share);

        EXPECT_EQ(TransferMetadata::Status::kNotEnoughSpace, metadata.status());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, IncomingConnection_FileSizeOverflow) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

  // Set file size sum huge to check for overflow.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .WillOnce(testing::Invoke([](const std::vector<uint8_t>& data,
                                   ash::nearby::MockNearbySharingDecoder::
                                       DecodeFrameCallback callback) {
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
            sharing::mojom::V1Frame::NewIntroduction(
                sharing::mojom::IntroductionFrame::New(
                    std::move(mojo_file_metadatas),
                    std::vector<sharing::mojom::TextMetadataPtr>(),
                    /*required_package=*/std::nullopt,
                    std::vector<sharing::mojom::WifiCredentialsMetadataPtr>()));

        sharing::mojom::FramePtr mojo_frame =
            sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));

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
        EXPECT_FALSE(share_target.for_self_share);

        EXPECT_EQ(TransferMetadata::Status::kNotEnoughSpace, metadata.status());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_ValidIntroductionFrame_ValidCertificate) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);
  SetUpIntroductionFrameDecoder();

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
        EXPECT_EQ(1u, share_target.wifi_credentials_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);
        EXPECT_FALSE(share_target.for_self_share);

        EXPECT_FALSE(metadata.token().has_value());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_InvalidWifiIntroductionPayload) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .WillOnce(testing::Invoke(
          [](const std::vector<uint8_t>& data,
             ash::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                 callback) {
            std::vector<sharing::mojom::WifiCredentialsMetadataPtr>
                mojo_wifi_credentials_metadatas;
            mojo_wifi_credentials_metadatas.push_back(
                sharing::mojom::WifiCredentialsMetadata::New(
                    /*ssid=*/"", kWifiSecurityType, kWifiCredentialsPayloadId,
                    kWifiCredentialsId));

            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::NewIntroduction(
                    sharing::mojom::IntroductionFrame::New(
                        std::vector<sharing::mojom::FileMetadataPtr>(),
                        std::vector<sharing::mojom::TextMetadataPtr>(),
                        /*required_package=*/std::nullopt,
                        std::move(mojo_wifi_credentials_metadatas)));

            sharing::mojom::FramePtr mojo_frame =
                sharing::mojom::Frame::NewV1(std::move(mojo_v1frame));

            std::move(callback).Run(std::move(mojo_frame));
          }));
  connection_.AppendReadableData(std::move(bytes));

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;
  base::RunLoop run_loop;

  // This call works when the Receive Wi-Fi flag is both enabled and disabled
  // by checking for two different fail cases:
  // 1. When the flag is enabled, but the introduction frame has an empty SSID
  //    we expect kUnsupportedAttachmentType.
  // 2. When the flag is disabled, we should not accept Wi-Fi credentials yet
  //    so we also expect kUnsupportedAttachmentType.
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop](const ShareTarget& share_target,
                                            TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kUnsupportedAttachmentType,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(0u, share_target.wifi_credentials_attachments.size());
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kSuccess);
  SetUpForegroundReceiveSurface(callback);
  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  run_loop.Run();

  // To avoid UAF in OncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, AcceptInvalidShareTarget) {
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

TEST_P(NearbySharingServiceImplTest,
       AcceptValidShareTarget_RegisterPayloadError) {
  fake_nearby_connections_manager_->SetPayloadPathStatus(
      kFilePayloadId, nearby::connections::mojom::Status::kError);
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

  // TODO(crbug.com/40146639) - Remove cleanups after bugfix
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::optional<base::FilePath> path =
        fake_nearby_connections_manager_->GetRegisteredPayloadPath(
            kFilePayloadId);
    EXPECT_TRUE(path);
    base::DeleteFile(*path);
  }

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, AcceptValidShareTarget) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, nearby::connections::mojom::Status::kSuccess);
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
      nearby::sharing::service::proto::ConnectionResponseFrame::ACCEPT);

  EXPECT_FALSE(connection_.IsClosed());

  // TODO(crbug.com/40146639) - Remove cleanups after bugfix
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::optional<base::FilePath> path =
        fake_nearby_connections_manager_->GetRegisteredPayloadPath(
            kFilePayloadId);
    EXPECT_TRUE(path);
    base::DeleteFile(*path);
  }

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, AcceptValidShareTarget_PayloadSuccessful) {
  SuccessfullyReceiveTransfer();
}

TEST_P(NearbySharingServiceImplTest, AcceptValidShareTarget_NoWifiPayload) {
  ReceiveBadWifiPayload(nearby::connections::mojom::Payload::New());
}

TEST_P(NearbySharingServiceImplTest,
       AcceptValidShareTarget_WifiPayloadBytesEmpty) {
  ReceiveBadWifiPayload(nearby::connections::mojom::Payload::New(
      kWifiCredentialsPayloadId,
      nearby::connections::mojom::PayloadContent::NewBytes(
          nearby::connections::mojom::BytesPayload::New(
              std::vector<uint8_t>()))));
}

TEST_P(NearbySharingServiceImplTest,
       AcceptValidShareTarget_NoCredentialsInProto) {
  const std::string& proto_string = "Random string that's not a proto";

  ReceiveBadWifiPayload(nearby::connections::mojom::Payload::New(
      kWifiCredentialsPayloadId,
      nearby::connections::mojom::PayloadContent::NewBytes(
          nearby::connections::mojom::BytesPayload::New(std::vector<uint8_t>(
              proto_string.begin(), proto_string.end())))));
}

TEST_P(NearbySharingServiceImplTest,
       AcceptValidShareTarget_WifiProtoPasswordEmpty) {
  nearby::sharing::service::proto::WifiCredentials credentials_proto;
  credentials_proto.set_password("");
  credentials_proto.set_hidden_ssid(false);
  const std::string& proto_string = credentials_proto.SerializeAsString();

  ReceiveBadWifiPayload(nearby::connections::mojom::Payload::New(
      kWifiCredentialsPayloadId,
      nearby::connections::mojom::PayloadContent::NewBytes(
          nearby::connections::mojom::BytesPayload::New(std::vector<uint8_t>(
              proto_string.begin(), proto_string.end())))));
}

TEST_P(NearbySharingServiceImplTest, AcceptValidShareTarget_HiddenNetwork) {
  nearby::sharing::service::proto::WifiCredentials credentials_proto;
  credentials_proto.set_password(kWifiPassword);
  credentials_proto.set_hidden_ssid(true);
  const std::string& proto_string = credentials_proto.SerializeAsString();

  ReceiveBadWifiPayload(nearby::connections::mojom::Payload::New(
      kWifiCredentialsPayloadId,
      nearby::connections::mojom::PayloadContent::NewBytes(
          nearby::connections::mojom::BytesPayload::New(std::vector<uint8_t>(
              proto_string.begin(), proto_string.end())))));
}

TEST_P(NearbySharingServiceImplTest,
       AcceptValidShareTarget_PayloadSuccessful_IncomingPayloadNotFound) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, nearby::connections::mojom::Status::kSuccess);
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
    if (id == kFilePayloadId) {
      continue;
    }

    // Deliberately not calling SetIncomingPayload() for text payloads to check
    // for failure condition.

    if (id == kWifiCredentialsPayloadId) {
      fake_nearby_connections_manager_->SetIncomingPayload(
          id, GetWifiPayloadPtr(kWifiCredentialsPayloadId, kWifiPassword));
    }

    base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
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

    nearby::connections::mojom::PayloadTransferUpdatePtr payload =
        nearby::connections::mojom::PayloadTransferUpdate::New(
            id, nearby::connections::mojom::PayloadStatus::kSuccess,
            /*total_bytes=*/kPayloadSize,
            /*bytes_transferred=*/kPayloadSize);
    listener->OnStatusUpdate(std::move(payload),
                             /*upgraded_medium=*/std::nullopt);
    run_loop_progress.Run();

    task_environment_.FastForwardBy(kMinProgressUpdateFrequency);
  }

  base::RunLoop run_loop_success;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_TRUE(metadata.is_final_status());
            EXPECT_LT(metadata.progress(), 100);
            EXPECT_EQ(TransferMetadata::Status::kIncompletePayloads,
                      metadata.status());

            ASSERT_TRUE(share_target.has_attachments());
            EXPECT_EQ(1u, share_target.file_attachments.size());
            const FileAttachment& file = share_target.file_attachments[0];
            EXPECT_FALSE(file.file_path());
            EXPECT_EQ(0u, wifi_network_handler_->num_configure_network_calls());
            run_loop_success.Quit();
          }));

  base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
      fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
          kFilePayloadId);
  ASSERT_TRUE(listener);

  nearby::connections::mojom::PayloadTransferUpdatePtr payload =
      nearby::connections::mojom::PayloadTransferUpdate::New(
          kFilePayloadId, nearby::connections::mojom::PayloadStatus::kSuccess,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
  listener->OnStatusUpdate(std::move(payload),
                           /*upgraded_medium=*/std::nullopt);
  run_loop_success.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  task_environment_.RunUntilIdle();

  std::optional<base::FilePath> file_path =
      fake_nearby_connections_manager_->GetRegisteredPayloadPath(
          kFilePayloadId);
  ASSERT_TRUE(file_path);
  EXPECT_FALSE(FileExists(*file_path));

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, AcceptValidShareTarget_PayloadFailed) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, nearby::connections::mojom::Status::kSuccess);
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

  base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
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

  nearby::connections::mojom::PayloadTransferUpdatePtr payload =
      nearby::connections::mojom::PayloadTransferUpdate::New(
          kFilePayloadId, nearby::connections::mojom::PayloadStatus::kFailure,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
  listener->OnStatusUpdate(std::move(payload),
                           /*upgraded_medium=*/std::nullopt);
  run_loop_failure.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  task_environment_.RunUntilIdle();

  std::optional<base::FilePath> file_path =
      fake_nearby_connections_manager_->GetRegisteredPayloadPath(
          kFilePayloadId);
  ASSERT_TRUE(file_path);
  EXPECT_FALSE(FileExists(*file_path));

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, AcceptValidShareTarget_PayloadCancelled) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, nearby::connections::mojom::Status::kSuccess);
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

  base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener> listener =
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

  nearby::connections::mojom::PayloadTransferUpdatePtr payload =
      nearby::connections::mojom::PayloadTransferUpdate::New(
          kFilePayloadId, nearby::connections::mojom::PayloadStatus::kCanceled,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
  listener->OnStatusUpdate(std::move(payload),
                           /*upgraded_medium=*/std::nullopt);
  run_loop_failure.Run();

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  task_environment_.RunUntilIdle();

  std::optional<base::FilePath> file_path =
      fake_nearby_connections_manager_->GetRegisteredPayloadPath(
          kFilePayloadId);
  ASSERT_TRUE(file_path);
  EXPECT_FALSE(FileExists(*file_path));

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, RejectInvalidShareTarget) {
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

TEST_P(NearbySharingServiceImplTest, RejectValidShareTarget) {
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
      nearby::sharing::service::proto::ConnectionResponseFrame::REJECT);

  task_environment_.FastForwardBy(kIncomingRejectionDelay + kDelta);
  EXPECT_TRUE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_KeyVerificationRunnerStatusUnable) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);
  SetUpIntroductionFrameDecoder();

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
        EXPECT_EQ(1u, share_target.wifi_credentials_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);
        EXPECT_FALSE(share_target.for_self_share);

        EXPECT_EQ(kFourDigitToken, metadata.token());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kUnable);
  SetUpForegroundReceiveSurface(callback);

  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
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

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_KeyVerificationRunnerStatusUnable_LowPower) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);
  SetUpIntroductionFrameDecoder();

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
        EXPECT_EQ(1u, share_target.wifi_credentials_attachments.size());
        EXPECT_EQ(kDeviceName, share_target.device_name);
        EXPECT_EQ(GURL(kTestMetadataIconUrl), share_target.image_url);
        EXPECT_EQ(kDeviceType, share_target.type);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(kEndpointId, share_target.device_id);
        EXPECT_EQ(kTestMetadataFullName, share_target.full_name);
        EXPECT_FALSE(share_target.for_self_share);

        EXPECT_EQ(kFourDigitToken, metadata.token());
        run_loop.Quit();
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       sharing::mojom::PairedKeyResultFrame_Status::kUnable);

  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
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

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_KeyVerificationRunnerStatusFail) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              kToken);
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

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

  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  // Ensure that the messages sent by ProcessLatestPublicCertificateDecryption
  // are processed prior to checking if connection is closed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest,
       IncomingConnection_EmptyAuthToken_KeyVerificationRunnerStatusFail) {
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/1u);

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  NiceMock<MockTransferUpdateCallback> callback;

  SetUpForegroundReceiveSurface(callback);

  // Ensures that introduction is never received for empty auth token.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  EXPECT_CALL(mock_decoder_, DecodeFrame(testing::Eq(bytes), testing::_))
      .Times(0);
  connection_.AppendReadableData(bytes);

  service_->OnIncomingConnectionAccepted(kEndpointId, kValidV1EndpointInfo,
                                         &connection_);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  EXPECT_TRUE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, RegisterReceiveSurfaceWhileSending) {
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

TEST_P(NearbySharingServiceImplTest, RegisterReceiveSurfaceAlreadyReceiving) {
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(callback);
  EXPECT_FALSE(connection_.IsClosed());

  EXPECT_EQ(
      NearbySharingService::StatusCodes::kTransferAlreadyInProgress,
      service_->RegisterReceiveSurface(
          &callback, NearbySharingService::ReceiveSurfaceState::kForeground));
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, RegisterReceiveSurfaceWhileDiscovering) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  NearbySharingService::StatusCodes result = service_->RegisterReceiveSurface(
      &transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kTransferAlreadyInProgress);
}

TEST_P(NearbySharingServiceImplTest, SendAttachments_WithoutAttachments) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kError,
            service_->SendAttachments(target, /*attachments=*/{}));
  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_P(NearbySharingServiceImplTest, SendText_AlreadySending) {
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

TEST_P(NearbySharingServiceImplTest, SendText_WithoutScanning) {
  ShareTarget target;
  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kError,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
}

TEST_P(NearbySharingServiceImplTest, SendText_UnknownTarget) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  DiscoverShareTarget(transfer_callback, discovery_callback);

  ShareTarget target;
  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kError,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_P(NearbySharingServiceImplTest, SendText_FailedCreateEndpointInfo) {
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

TEST_P(NearbySharingServiceImplTest, SendText_FailedToConnect) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  // Call DiscoverShareTarget() instead of SetUpOutgoingShareTarget() as we want
  // to fail before key verification is done.
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  base::RunLoop run_loop;
  ExpectTransferUpdates(
      transfer_callback, target,
      {TransferMetadata::Status::kConnecting,
       TransferMetadata::Status::kFailedToInitiateOutgoingConnection},
      run_loop.QuitClosure());

  EXPECT_EQ(
      NearbySharingServiceImpl::StatusCodes::kOk,
      service_->SendAttachments(target, CreateTextAttachments({kTextPayload})));
  run_loop.Run();

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_P(NearbySharingServiceImplTest, SendText_FailedKeyVerification) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  base::RunLoop run_loop;
  ExpectTransferUpdates(
      transfer_callback, target,
      {TransferMetadata::Status::kConnecting,
       TransferMetadata::Status::kPairedKeyVerificationFailed},
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

TEST_P(NearbySharingServiceImplTest, SendText_UnableToVerifyKey) {
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
  ExpectTransferUpdates(transfer_callback, target,
                        {std::get<0>(GetParam()).expected_status},
                        reject_run_loop.QuitClosure());

  // Cancel the transfer by rejecting it.
  SendConnectionResponse(std::get<0>(GetParam()).response_status);
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
  ExpectTransferUpdates(transfer_callback, target,
                        {std::get<0>(GetParam()).expected_status},
                        reject_run_loop.QuitClosure());

  // Cancel the transfer by rejecting it.
  SendConnectionResponse(std::get<0>(GetParam()).response_status);
  reject_run_loop.Run();

  EXPECT_TRUE(connection_.IsClosed());

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

INSTANTIATE_TEST_SUITE_P(
    NearbySharingServiceImplSendFailureTest,
    NearbySharingServiceImplSendFailureTest,
    testing::Combine(testing::ValuesIn(kSendFailureTestData),
                     testing::Range<size_t>(0, 1 << kTestFeatures.size())));

TEST_P(NearbySharingServiceImplTest, SendText_Success) {
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
  EXPECT_EQ(nearby::sharing::service::proto::TextMetadata_Type_TEXT,
            meta.type());

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

TEST_P(NearbySharingServiceImplTest, SendText_SuccessClosedConnection) {
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

TEST_P(NearbySharingServiceImplTest, SendFiles_Success) {
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
  EXPECT_EQ(nearby::sharing::service::proto::FileMetadata_Type_UNKNOWN,
            meta.type());

  // Expect the file payload to be sent in the end.
  base::RunLoop payload_run_loop;
  fake_nearby_connections_manager_->set_send_payload_callback(
      base::BindLambdaForTesting(
          [&](NearbyConnectionsManager::PayloadPtr payload,
              base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener>
                  listener) {
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

TEST_P(NearbySharingServiceImplTest, Cancel_Sender_Initiator) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);
  target = SetUpOutgoingConnectionUntilAccept(transfer_callback, target);
  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target);

  // After we stop scanning, we check back in after kInvalidateDelay
  // milliseconds to make sure that we stopped in order to send a file and not
  // because the user left the page. We have to fast forward here, otherwise, we
  // will hit this callback when trying to fastfoward by kInitiatorCancelDelay
  // below.
  task_environment_.FastForwardBy(kInvalidateDelay);

  base::RunLoop run_loop;
  EXPECT_CALL(transfer_callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_EQ(target.id, share_target.id);
            EXPECT_EQ(TransferMetadata::Status::kCancelled, metadata.status());
          }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));
  // The initiator of the cancellation explicitly calls Cancel().
  service_->Cancel(target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop.Quit();
                       }));
  run_loop.Run();
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));

  // After the TransferMetadata::Status::kCancelled update, we expect other
  // classes to unregister the send surface.
  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);

  // The initiator of the cancel should send a cancel frame to the other device,
  // then wait a few seconds before disconnecting to allow for processing on the
  // other device.
  ExpectCancelFrame();
  EXPECT_FALSE(connection_.IsClosed());
  task_environment_.FastForwardBy(kInitiatorCancelDelay);
  EXPECT_TRUE(connection_.IsClosed());
}

TEST_P(NearbySharingServiceImplTest, Cancel_Sender_Noninitiator) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ShareTarget target =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);
  target = SetUpOutgoingConnectionUntilAccept(transfer_callback, target);
  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target);

  base::RunLoop run_loop;
  EXPECT_CALL(transfer_callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_EQ(target.id, share_target.id);
            EXPECT_EQ(TransferMetadata::Status::kCancelled, metadata.status());
            run_loop.Quit();
          }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));
  // The non-initiator of the cancellation processes a cancellation frame from
  // the initiator.
  SendCancel();
  run_loop.Run();
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));

  // The non-initiator should close the connection immediately
  EXPECT_TRUE(connection_.IsClosed());
}

TEST_P(NearbySharingServiceImplTest, Cancel_Receiver_Initiator) {
  NiceMock<MockTransferUpdateCallback> transfer_callback;
  ShareTarget target = SetUpIncomingConnection(transfer_callback);
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();

  base::RunLoop run_loop;
  EXPECT_CALL(transfer_callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_EQ(target.id, share_target.id);
            EXPECT_EQ(TransferMetadata::Status::kCancelled, metadata.status());
          }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));
  // The initiator of the cancellation explicitly calls Cancel().
  service_->Cancel(target,
                   base::BindLambdaForTesting(
                       [&](NearbySharingServiceImpl::StatusCodes status_code) {
                         EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk,
                                   status_code);
                         run_loop.Quit();
                       }));
  run_loop.Run();
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));

  // After the TransferMetadata::Status::kCancelled update, we expect other
  // classes to unregister the receive surface.
  service_->UnregisterReceiveSurface(&transfer_callback);

  // The initiator of the cancel should send a cancel frame to the other device,
  // then wait a few seconds before disconnecting to allow for processing on the
  // other device.
  ExpectCancelFrame();
  EXPECT_FALSE(connection_.IsClosed());
  task_environment_.FastForwardBy(kInitiatorCancelDelay);
  EXPECT_TRUE(connection_.IsClosed());
}

TEST_P(NearbySharingServiceImplTest, Cancel_Receiver_Noninitiator) {
  NiceMock<MockTransferUpdateCallback> transfer_callback;
  ShareTarget target = SetUpIncomingConnection(transfer_callback);
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();

  base::RunLoop run_loop;
  EXPECT_CALL(transfer_callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_EQ(target.id, share_target.id);
            EXPECT_EQ(TransferMetadata::Status::kCancelled, metadata.status());
            run_loop.Quit();
          }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));
  // The non-initiator of the cancellation processes a cancellation frame from
  // the initiator.
  SendCancel();
  run_loop.Run();
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));

  // The non-initiator should close the connection immediately
  EXPECT_TRUE(connection_.IsClosed());
}

TEST_P(NearbySharingServiceImplTest,
       RegisterForegroundReceiveSurfaceEntersHighVisibility) {
  TestObserver observer(service_.get());
  NiceMock<MockTransferUpdateCallback> callback;

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  local_device_data_manager()->SetDeviceName(kDeviceName);

  // To start, we should not be in high visibility state.
  EXPECT_FALSE(service_->IsInHighVisibility());
  EXPECT_FALSE(observer.on_start_advertising_failure_called_);

  // If we register a foreground surface we should end up in high visibility
  // state.
  SetUpForegroundReceiveSurface(callback);

  // At this point we should have a new high visibility state and the observer
  // should have been called as well.
  EXPECT_TRUE(service_->IsInHighVisibility());
  EXPECT_TRUE(observer.in_high_visibility_);
  EXPECT_FALSE(observer.on_start_advertising_failure_called_);

  // If we unregister the foreground receive surface we should no longer be in
  // high visibility and the observer should be notified.
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            service_->UnregisterReceiveSurface(&callback));
  EXPECT_FALSE(service_->IsInHighVisibility());
  EXPECT_FALSE(observer.in_high_visibility_);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
}

TEST_P(NearbySharingServiceImplTest, ProcessStoppedCallsObservers) {
  TestObserver observer(service_.get());
  NiceMock<MockTransferUpdateCallback> callback;

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  local_device_data_manager()->SetDeviceName(kDeviceName);

  // If we register a foreground surface we should end up in high visibility
  // state.
  SetUpForegroundReceiveSurface(callback);
  EXPECT_TRUE(service_->IsInHighVisibility());

  // Signal a process crash, check that the observer is called and high
  // visibility is stopped.
  fake_nearby_connections_manager_->CleanupForProcessStopped();
  std::move(process_stopped_callback_)
      .Run(ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason::
               kCrash);
  EXPECT_TRUE(observer.process_stopped_called_);
  EXPECT_FALSE(service_->IsInHighVisibility());

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
}

TEST_P(NearbySharingServiceImplTest, ShutdownCallsObservers) {
  TestObserver observer(service_.get());

  EXPECT_FALSE(observer.shutdown_called_);

  service_->Shutdown();

  EXPECT_TRUE(observer.shutdown_called_);

  // Prevent a double shutdown.
  service_.reset();
}

TEST_P(NearbySharingServiceImplTest, SendPayloadWithArcCallback) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  FakeArcNearbyShareSession arc_session;

  service_->SetArcTransferCleanupCallback(
      base::BindOnce(&FakeArcNearbyShareSession::OnCleanupCallbackStub,
                     base::Unretained(&arc_session)));
  EXPECT_FALSE(arc_session.CleanupCallbackCalled());

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
  EXPECT_EQ(nearby::sharing::service::proto::TextMetadata_Type_TEXT,
            meta.type());

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

  // ARC cleanup runs in a ThreadPool.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(arc_session.CleanupCallbackCalled());

  // Expect to be disconnected now.
  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId));

  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
}

TEST_P(NearbySharingServiceImplTest, ShutdownCallsObserversWithArcCallback) {
  TestObserver observer(service_.get());
  FakeArcNearbyShareSession arc_session;

  service_->SetArcTransferCleanupCallback(
      base::BindOnce(&FakeArcNearbyShareSession::OnCleanupCallbackStub,
                     base::Unretained(&arc_session)));

  EXPECT_FALSE(arc_session.CleanupCallbackCalled());
  EXPECT_FALSE(observer.shutdown_called_);

  service_->Shutdown();

  // ARC cleanup runs in a ThreadPool.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(observer.shutdown_called_);
  EXPECT_TRUE(arc_session.CleanupCallbackCalled());

  // Prevent a double shutdown.
  service_.reset();
}

TEST_P(NearbySharingServiceImplTest, RotateBackgroundAdvertisement_Periodic) {
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
  task_environment_.FastForwardBy(base::Seconds(870));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_rotated =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  EXPECT_NE(endpoint_info_initial, endpoint_info_rotated);
}

TEST_P(NearbySharingServiceImplTest,
       RotateBackgroundAdvertisement_PrivateCertificatesChange) {
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
  certificate_manager()->NotifyPrivateCertificatesChanged();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_rotated =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  EXPECT_NE(endpoint_info_initial, endpoint_info_rotated);
}

TEST_P(NearbySharingServiceImplTest, OrderedEndpointDiscoveryEvents) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;

  // Start discovering, to ensure a discovery listener is registered.
  EXPECT_EQ(
      NearbySharingService::StatusCodes::kOk,
      service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                    SendSurfaceState::kForeground));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Ensure that the endpoint discovered and lost event are process
  // sequentially. This is particularly importatnt due to the asynchronous
  // operations needed to handle endpoint discovery.
  //
  // Order of events:
  //   - Nearby Connections discovers endpoint 1
  //   - Nearby Connections loses endpoint 1
  //   - Nearby Share processes these two events in order.
  //   - Nearby Connections discovers endpoint 2
  //   - Nearby Connections discovers endpoint 3
  //   - Nearby Connections loses endpoint 3
  //   - Nearby Connections loses endpoint 2
  //   - Nearby Share processes these four events in order.

  // Expect the advertisement decoder  to be invoked once for each discovery.
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/false,
                            /*expected_number_of_calls=*/3u);
  {
    base::RunLoop run_loop;
    FindEndpoint(/*endpoint_id=*/"1");
    LoseEndpoint(/*endpoint_id=*/"1");
    ::testing::InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered);
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([&run_loop](ShareTarget share_target) { run_loop.Quit(); });

    // Needed for discovery processing.
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    FindEndpoint(/*endpoint_id=*/"2");
    FindEndpoint(/*endpoint_id=*/"3");
    LoseEndpoint(/*endpoint_id=*/"3");
    LoseEndpoint(/*endpoint_id=*/"2");
    ::testing::InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([](ShareTarget share_target) {
          EXPECT_EQ("2", share_target.device_id);
        });
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([](ShareTarget share_target) {
          EXPECT_EQ("3", share_target.device_id);
        });
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([](ShareTarget share_target) {
          EXPECT_EQ("3", share_target.device_id);
        });
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([&run_loop](ShareTarget share_target) {
          EXPECT_EQ("2", share_target.device_id);
          run_loop.Quit();
        });
    // Needed for discovery processing. Fail, then the ShareTarget device ID is
    // set to the endpoint ID, which we use above to verify the correct endpoint
    // ID processing order.
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/false);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/3,
                                             /*success=*/false);

    run_loop.Run();
  }
}

TEST_P(NearbySharingServiceImplTest,
       RetryDiscoveredEndpoints_NoDownloadIfDecryption) {
  // Start discovery.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground);
  EXPECT_EQ(1u,
            certificate_manager()->num_download_public_certificates_calls());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/true,
                            /*expected_number_of_calls=*/1u);
  // Order of events:
  // - Discover endpoint 1 --> decrypts public certificate
  // - Fire certificate download timer --> no download because no cached
  //                                       advertisements
  {
    base::RunLoop run_loop;
    FindEndpoint(/*endpoint_id=*/"1");
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([&run_loop](ShareTarget share_target) { run_loop.Quit(); });
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    run_loop.Run();
  }
  task_environment_.FastForwardBy(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(1u,
            certificate_manager()->num_download_public_certificates_calls());

  EXPECT_CALL(discovery_callback, OnShareTargetLost);
  service_->Shutdown();
  service_.reset();
}

TEST_P(NearbySharingServiceImplTest,
       RetryDiscoveredEndpoints_DownloadCertsAndRetryDecryption) {
  // Start discovery.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground);
  EXPECT_EQ(1u,
            certificate_manager()->num_download_public_certificates_calls());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/true,
                            /*expected_number_of_calls=*/6u);
  // Order of events:
  // - Discover endpoint 1 --> decrypts public certificate
  // - Discover endpoint 2 --> cannot decrypt public certificate
  // - Discover endpoint 3 --> decrypts public certificate
  // - Discover endpoint 4 --> cannot decrypt public certificate
  // - Lose endpoint 3
  // - Fire certificate download timer --> certificates downloaded
  // - (Re)discover endpoints 2 and 4
  {
    base::RunLoop run_loop;
    FindEndpoint(/*endpoint_id=*/"1");
    FindEndpoint(/*endpoint_id=*/"2");
    FindEndpoint(/*endpoint_id=*/"3");
    FindEndpoint(/*endpoint_id=*/"4");
    LoseEndpoint(/*endpoint_id=*/"3");
    ::testing::InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered).Times(2);
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([&run_loop](ShareTarget share_target) { run_loop.Quit(); });
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/false);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/3,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/4,
                                             /*success=*/false);
    run_loop.Run();
  }
  task_environment_.FastForwardBy(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(2u,
            certificate_manager()->num_download_public_certificates_calls());
  certificate_manager()->NotifyPublicCertificatesDownloaded();
  {
    base::RunLoop run_loop;
    ::testing::InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered);
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([&run_loop](ShareTarget share_target) { run_loop.Quit(); });
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/5,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/6,
                                             /*success=*/true);
    run_loop.Run();
  }
  EXPECT_CALL(discovery_callback, OnShareTargetLost).Times(3);
  service_->Shutdown();
  service_.reset();
}

TEST_P(NearbySharingServiceImplTest,
       RetryDiscoveredEndpoints_DiscoveryRestartClearsCache) {
  // Start discovery.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground);
  EXPECT_EQ(1u,
            certificate_manager()->num_download_public_certificates_calls());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/true,
                            /*expected_number_of_calls=*/1u);
  // Order of events:
  // - Discover endpoint 1 --> cannot decrypt public certificate
  // - Stop discovery
  // - Certificate download timer not running; not discovering
  // - Start discovery
  // - Fire certificate download timer --> certificates not downloaded; cached
  //                                       advertisement map has been cleared
  FindEndpoint(/*endpoint_id=*/"1");
  ::testing::InSequence s;
  EXPECT_CALL(discovery_callback, OnShareTargetDiscovered).Times(0);
  EXPECT_CALL(discovery_callback, OnShareTargetLost).Times(0);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
  task_environment_.FastForwardBy(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(1u,
            certificate_manager()->num_download_public_certificates_calls());
  service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground);
  // Note: Certificate downloads are also requested in RegisterSendSurface; this
  // is not related to the retry timer.
  EXPECT_EQ(2u,
            certificate_manager()->num_download_public_certificates_calls());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  task_environment_.FastForwardBy(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(2u,
            certificate_manager()->num_download_public_certificates_calls());
  service_->Shutdown();
  service_.reset();
}

TEST_P(NearbySharingServiceImplTest, RetryDiscoveredEndpoints_DownloadLimit) {
  // Start discovery.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground);
  EXPECT_EQ(1u,
            certificate_manager()->num_download_public_certificates_calls());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  SetUpAdvertisementDecoder(kValidV1EndpointInfo,
                            /*return_empty_advertisement=*/false,
                            /*return_empty_device_name=*/true,
                            /*expected_number_of_calls=*/2u +
                                kMaxCertificateDownloadsDuringDiscovery);
  // Order of events:
  // - x3:
  //   - (Re)discover endpoint 1 --> cannot decrypt public certificate
  //   - Fire certificate download timer --> certificates downloaded
  // - Rediscover endpoint 1 --> cannot decrypt public certificate
  // - Fire certificate download timer --> no download; limit reached
  // - Restart discovery which resets limit counter
  FindEndpoint(/*endpoint_id=*/"1");
  for (size_t i = 1; i <= kMaxCertificateDownloadsDuringDiscovery; ++i) {
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/i,
                                             /*success=*/false);
    task_environment_.FastForwardBy(kCertificateDownloadDuringDiscoveryPeriod);
    EXPECT_EQ(1u + i,
              certificate_manager()->num_download_public_certificates_calls());
    certificate_manager()->NotifyPublicCertificatesDownloaded();
  }
  task_environment_.FastForwardBy(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(1u + kMaxCertificateDownloadsDuringDiscovery,
            certificate_manager()->num_download_public_certificates_calls());
  service_->UnregisterSendSurface(&transfer_callback, &discovery_callback);
  service_->RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground);
  // Note: Certificate downloads are also requested in RegisterSendSurface; this
  // is not related to the retry timer.
  EXPECT_EQ(2u + kMaxCertificateDownloadsDuringDiscovery,
            certificate_manager()->num_download_public_certificates_calls());
  FindEndpoint(/*endpoint_id=*/"1");
  ProcessLatestPublicCertificateDecryption(
      /*expected_num_calls=*/1u + kMaxCertificateDownloadsDuringDiscovery,
      /*success=*/false);
  task_environment_.FastForwardBy(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(3u + kMaxCertificateDownloadsDuringDiscovery,
            certificate_manager()->num_download_public_certificates_calls());

  service_->Shutdown();
  service_.reset();
}

TEST_P(NearbySharingServiceImplTest, NotBoundToProcessIfDisabled) {
  SetIsEnabled(false);
  EXPECT_FALSE(IsBoundToProcess());
}

TEST_P(NearbySharingServiceImplTest, UnbindsFromProcessWhenDisabled) {
  SetIsEnabled(true);
  EXPECT_TRUE(IsBoundToProcess());
  SetIsEnabled(false);
  EXPECT_FALSE(IsBoundToProcess());
}

TEST_P(NearbySharingServiceImplTest, BindsProcessWhenReenabled) {
  SetIsEnabled(true);
  EXPECT_TRUE(IsBoundToProcess());
  SetIsEnabled(false);
  EXPECT_FALSE(IsBoundToProcess());
  SetIsEnabled(true);
  EXPECT_TRUE(IsBoundToProcess());
}

// Parameters are |is_enabled|, |shutdown_reason|, |recent_shutdown_count|, and
// |feature_mask|.
using ServiceRestartTestParams =
    std::tuple<bool, NearbyProcessShutdownReason, int, size_t>;

class NearbySharingServiceImplRestartTest
    : public NearbySharingServiceImplTestBase,
      public testing::WithParamInterface<ServiceRestartTestParams> {
 public:
  NearbySharingServiceImplRestartTest()
      : NearbySharingServiceImplTestBase(
            /*feature_mask=*/std::get<3>(GetParam())) {}
};

TEST_P(NearbySharingServiceImplRestartTest, RestartsServiceWhenAppropriate) {
  bool is_enabled = std::get<0>(GetParam());
  NearbyProcessShutdownReason shutdown_reason = std::get<1>(GetParam());
  int recent_shutdown_count = std::get<2>(GetParam());

  SetIsEnabled(is_enabled);
  SetRecentNearbyProcessShutdownCount(recent_shutdown_count);

  bool expected_to_restart;

  // Important:  Remember to update testing::Values used in
  // INSTANTIATE_TEST_SUITE_P when adding cases to this switch statement.
  switch (shutdown_reason) {
    case NearbyProcessShutdownReason::kNormal:
      expected_to_restart = false;
      break;

    case NearbyProcessShutdownReason::kCrash:
    case NearbyProcessShutdownReason::kConnectionsMojoPipeDisconnection:
    case NearbyProcessShutdownReason::kPresenceMojoPipeDisconnection:
    case NearbyProcessShutdownReason::kDecoderMojoPipeDisconnection:
      expected_to_restart =
          is_enabled && recent_shutdown_count <=
                            NearbySharingServiceImpl::
                                kMaxRecentNearbyProcessUnexpectedShutdownCount;
      break;
  }

  EXPECT_CALL(mock_nearby_process_manager(), GetNearbyProcessReference)
      .Times(expected_to_restart ? 1 : 0);

  // If the feature is disabled, the saved process_stopped_callback_ is invalid
  // and shouldn't be called.
  if (is_enabled) {
    ShutdownNearbyProcess(shutdown_reason);
  }
}

INSTANTIATE_TEST_SUITE_P(
    NearbySharingServiceImplTest,
    NearbySharingServiceImplRestartTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(
            NearbyProcessShutdownReason::kNormal,
            NearbyProcessShutdownReason::kCrash,
            NearbyProcessShutdownReason::kConnectionsMojoPipeDisconnection,
            NearbyProcessShutdownReason::kPresenceMojoPipeDisconnection,
            NearbyProcessShutdownReason::kDecoderMojoPipeDisconnection),
        testing::Values(0,
                        NearbySharingServiceImpl::
                                kMaxRecentNearbyProcessUnexpectedShutdownCount -
                            1,
                        NearbySharingServiceImpl::
                            kMaxRecentNearbyProcessUnexpectedShutdownCount),
        testing::Range<size_t>(0, 1 << kTestFeatures.size())));

TEST_P(NearbySharingServiceImplTest, ProcessShutdownTimerDoesNotRestart) {
  // Set visibility to Hidden to deactivate advertising to start the process
  // shutdown timer.
  SetVisibility(nearby_share::mojom::Visibility::kNoOne);

  EXPECT_TRUE(IsBoundToProcess());
  EXPECT_TRUE(IsProcessShutdownTimerRunning());

  // Registering a receive surface should cancel the timer.
  NiceMock<MockTransferUpdateCallback> callback;
  SetUpForegroundReceiveSurface(callback);
  EXPECT_TRUE(IsBoundToProcess());
  EXPECT_FALSE(IsProcessShutdownTimerRunning());

  // Unregistering the receive surface should start the timer.
  service_->UnregisterReceiveSurface(&callback);
  EXPECT_TRUE(IsBoundToProcess());
  EXPECT_TRUE(IsProcessShutdownTimerRunning());

  // Run the timer down a bit.
  task_environment_.FastForwardBy(base::Seconds(10));

  // Unregister a receive surface again and make sure the timer did not restart.
  service_->UnregisterReceiveSurface(&callback);
  EXPECT_TRUE(IsBoundToProcess());
  EXPECT_TRUE(IsProcessShutdownTimerRunning());
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(IsBoundToProcess());
  EXPECT_FALSE(IsProcessShutdownTimerRunning());
}

TEST_P(NearbySharingServiceImplTest, NoShutdownTimerWithoutProcessRef) {
  FireProcessShutdownIfRunning();
  // Reset process reference.
  SetIsEnabled(false);
  EXPECT_FALSE(IsBoundToProcess());
  EXPECT_FALSE(IsProcessShutdownTimerRunning());

  // Unregister a receive surface and make sure the timer does not start.
  NiceMock<MockTransferUpdateCallback> callback;
  service_->UnregisterReceiveSurface(&callback);
  EXPECT_FALSE(IsBoundToProcess());
  EXPECT_FALSE(IsProcessShutdownTimerRunning());
}

TEST_P(NearbySharingServiceImplTest, FastInitiationScanning_StartAndStop) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(0u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  // Trigger a call to StopFastInitiationScanning().
  SetBluetoothIsPowered(false);
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  // Trigger a call to StartFastInitiationScanning().
  SetBluetoothIsPowered(true);
  EXPECT_EQ(2u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());
}

TEST_P(NearbySharingServiceImplTest,
       FastInitiationScanning_DisallowedByPolicy) {
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(0u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  SetManagedEnabled(false);
  base::RunLoop().RunUntilIdle();
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);

  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());
}

TEST_P(NearbySharingServiceImplTest,
       FastInitiationScanning_OnFastInitiationNotificationStateChanged) {
  // Fast init notifications are enabled by default so a scanner is created on
  // initialization of the service.
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(0u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  // The existing scanner is destroyed when fast init notifications are turned
  // off.
  SetFastInitiationNotificationState(
      nearby_share::mojom::FastInitiationNotificationState::kDisabledByUser);
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  SetFastInitiationNotificationState(
      nearby_share::mojom::FastInitiationNotificationState::kEnabled);
  EXPECT_EQ(2u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());
}

TEST_P(NearbySharingServiceImplTest,
       FastInitiationScanning_MultipleReceiveSurfaces) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(0u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  // Registering a background receive surface should not create a scanner since
  // we're already scanning.
  MockTransferUpdateCallback callback;
  service_->RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(0u, fast_initiation_scanner_factory_->scanner_destroyed_count());
}

TEST_P(NearbySharingServiceImplTest, FastInitiationScanning_NotifyObservers) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);
  TestObserver observer(service_.get());
  ASSERT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());

  FakeFastInitiationScanner* scanner =
      fast_initiation_scanner_factory_->last_fake_fast_initiation_scanner();
  scanner->DevicesDetected();
  EXPECT_TRUE(observer.devices_detected_called_);
  scanner->DevicesNotDetected();
  EXPECT_TRUE(observer.devices_not_detected_called_);
  scanner->ScannerInvalidated();
  EXPECT_TRUE(observer.scanning_stopped_called_);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
}

TEST_P(NearbySharingServiceImplTest, FastInitiationScanning_NoHardwareSupport) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);

  // Hardware support is enabled by default in these tests, so we expect that a
  // scanner has been created.
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(0u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  fast_initiation_scanner_factory_->SetHardwareSupportAvailable(false);
  // Changes in HardwareSupportState should trigger InvalidateSurfaceState
  SetHardwareSupportState(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kNotSupported);

  // Make sure we stopped scanning and didn't restart.
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());
}

TEST_P(NearbySharingServiceImplTest,
       FastInitiationScanning_PostTransferCooldown) {
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);

  // Make sure we started scanning once
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(0u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  SuccessfullyReceiveTransfer();

  // Make sure we stopped scanning and didn't restart... yet.
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());

  // Fast forward 10s to pass through the cooldown period.
  task_environment_.FastForwardBy(base::Seconds(10));

  // Make sure we restarted Fast Initiation scanning.
  EXPECT_EQ(2u, fast_initiation_scanner_factory_->scanner_created_count());
  EXPECT_EQ(1u, fast_initiation_scanner_factory_->scanner_destroyed_count());
}

TEST_P(NearbySharingServiceImplTest, VisibilityReminderTimerIsTriggered) {
  constexpr base::TimeDelta kTestDelay = base::Minutes(3);
  service_->set_visibility_reminder_timer_delay_for_testing(kTestDelay);
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  task_environment_.FastForwardBy(kTestDelay);
  std::vector<message_center::Notification> notifications =
      notification_tester_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::NEARBY_SHARE);

  // Visibility reminder notification should display after 3 Minutes.
  EXPECT_EQ(1u, notifications.size());
}

TEST_P(NearbySharingServiceImplTest, CreateShareTarget) {
  sharing::mojom::AdvertisementPtr advertisement =
      sharing::mojom::Advertisement::New(
          GetNearbyShareTestEncryptedMetadataKey().salt(),
          GetNearbyShareTestEncryptedMetadataKey().encrypted_key(), kDeviceType,
          kDeviceName);

  // Flip |for_self_share| to true to ensure the resulting ShareTarget picks
  // this up.
  nearby::sharing::proto::PublicCertificate certificate_proto =
      GetNearbyShareTestPublicCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  certificate_proto.set_for_self_share(true);

  std::optional<NearbyShareDecryptedPublicCertificate> certificate =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          certificate_proto, GetNearbyShareTestEncryptedMetadataKey());
  ASSERT_TRUE(certificate.has_value());
  ASSERT_EQ(certificate_proto.for_self_share(), certificate->for_self_share());

  std::optional<ShareTarget> share_target =
      CreateShareTarget(advertisement, certificate);
  ASSERT_TRUE(share_target.has_value());
  EXPECT_EQ(kDeviceName, share_target->device_name);
  EXPECT_EQ(kDeviceType, share_target->type);
  EXPECT_EQ(certificate_proto.for_self_share(), share_target->for_self_share);

  // Test when |certificate| is null.
  share_target = CreateShareTarget(advertisement, /*certificate=*/std::nullopt);
  ASSERT_TRUE(share_target.has_value());
  EXPECT_EQ(kDeviceName, share_target->device_name);
  EXPECT_EQ(kDeviceType, share_target->type);
  EXPECT_FALSE(share_target->for_self_share);
}

TEST_P(NearbySharingServiceImplTest, SelfShareAutoAccept) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, nearby::connections::mojom::Status::kSuccess);
  }

  // We create an incoming connection corresponding to a certificate where the
  // |for_self_share| field is set to 'true'. This value will be propagated to
  // the ShareTarget, which will be used as a signal for the service to
  // automatically accept the transfer when Self Share is enabled. This is
  // similar to other tests (see "AcceptValidShareTarget") but without the
  // explicit call to service_->Accept(). Wi-Fi credentials is an edge case for
  // Self Share that we test separately, so |no_wifi_credentials| is true.
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(
      callback, /*for_self_share=*/true, /*wifi_credentials_metadata_count=*/0);

  base::RunLoop run_loop;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& share_target, TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kAwaitingRemoteAcceptance,
                      metadata.status());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectConnectionResponseFrame(
      nearby::sharing::service::proto::ConnectionResponseFrame::ACCEPT);

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest, SelfShareAutoAccept_WiFiCredentials) {
  for (int64_t payload_id : kValidIntroductionFramePayloadIds) {
    fake_nearby_connections_manager_->SetPayloadPathStatus(
        payload_id, nearby::connections::mojom::Status::kSuccess);
  }

  // We create an incoming connection corresponding to a certificate where the
  // |for_self_share| field is set to 'true'. This value will be propagated to
  // the ShareTarget, which will be used as a signal for the service to
  // automatically accept the transfer when Self Share is enabled. However, we
  // don't auto-accept Wi-Fi credentials and thus we
  NiceMock<MockTransferUpdateCallback> callback;
  ShareTarget share_target = SetUpIncomingConnection(
      callback, /*for_self_share=*/true, /*wifi_credentials_metadata_count=*/1);

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
      nearby::sharing::service::proto::ConnectionResponseFrame::ACCEPT);

  EXPECT_FALSE(connection_.IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  service_->UnregisterReceiveSurface(&callback);
}

TEST_P(NearbySharingServiceImplTest,
       SelfShareEnabled_YourDevicesVisibilityOnScreenLock) {
  const std::set<std::string> contacts = {"1", "2"};
  SetVisibility(nearby_share::mojom::Visibility::kAllContacts);
  contact_manager()->SetAllowedContacts(contacts);

  // Lock screen, expect Your Devices visibility.
  session_controller_->SetScreenLocked(true);
  EXPECT_EQ(nearby_share::mojom::Visibility::kYourDevices, GetVisibility());
  EXPECT_EQ(std::set<std::string>(), contact_manager()->GetAllowedContacts());

  // Unlock screen, expect visibility to return to All Contacts.
  session_controller_->SetScreenLocked(false);
  EXPECT_EQ(nearby_share::mojom::Visibility::kAllContacts, GetVisibility());
  EXPECT_EQ(contacts, contact_manager()->GetAllowedContacts());
}

TEST_P(
    NearbySharingServiceImplTest,
    SelfShareEnabled_YourDevicesVisibilityOnScreenLock_DefaultSelectedContactsVisibility) {
  const std::set<std::string> contacts = {"1", "2"};
  SetVisibility(nearby_share::mojom::Visibility::kSelectedContacts);
  contact_manager()->SetAllowedContacts(contacts);

  // Lock screen, expect Your Devices visibility.
  session_controller_->SetScreenLocked(true);
  EXPECT_EQ(nearby_share::mojom::Visibility::kYourDevices, GetVisibility());
  EXPECT_EQ(std::set<std::string>(), contact_manager()->GetAllowedContacts());

  // Unlock screen, expect visibility to return to All Contacts.
  session_controller_->SetScreenLocked(false);
  EXPECT_EQ(nearby_share::mojom::Visibility::kSelectedContacts,
            GetVisibility());
  EXPECT_EQ(contacts, contact_manager()->GetAllowedContacts());
}

TEST_P(NearbySharingServiceImplTest,
       UserSelectsHiddenVisibility_HiddenVisibilityOnScreenLock) {
  SetVisibility(nearby_share::mojom::Visibility::kNoOne);
  std::set<std::string> empty_set;
  contact_manager()->SetAllowedContacts(empty_set);

  // Lock screen, expect Hidden visibility.
  session_controller_->SetScreenLocked(true);
  EXPECT_EQ(nearby_share::mojom::Visibility::kNoOne, GetVisibility());
  EXPECT_EQ(empty_set, contact_manager()->GetAllowedContacts());

  // Unlock screen, expect visibility to still be Hidden.
  session_controller_->SetScreenLocked(false);
  EXPECT_EQ(nearby_share::mojom::Visibility::kNoOne, GetVisibility());
  EXPECT_EQ(empty_set, contact_manager()->GetAllowedContacts());
}

INSTANTIATE_TEST_SUITE_P(NearbySharingServiceImplTest,
                         NearbySharingServiceImplTest,
                         testing::Range<size_t>(0, 1 << kTestFeatures.size()));

// The boolean variable represents is service enabled. Visibility represents
// different visibility selections.
using VisibilityReminderTestParams =
    std::tuple<bool, nearby_share::mojom::Visibility, size_t>;

class NearbySharingServiceVisibilityReminderTest
    : public NearbySharingServiceImplTestBase,
      public testing::WithParamInterface<VisibilityReminderTestParams> {
 public:
  NearbySharingServiceVisibilityReminderTest()
      : NearbySharingServiceImplTestBase(std::get<2>(GetParam())) {}
};

TEST_P(NearbySharingServiceVisibilityReminderTest,
       StartOrStopVisibilityReminderTimerAppropriatelyWhenEnableOrDisable) {
  bool is_enabled = std::get<0>(GetParam());
  nearby_share::mojom::Visibility visibility = std::get<1>(GetParam());

  SetVisibility(visibility);
  SetIsEnabled(is_enabled);
  if (is_enabled) {
    switch (visibility) {
      case nearby_share::mojom::Visibility::kAllContacts:
      case nearby_share::mojom::Visibility::kSelectedContacts:
        EXPECT_TRUE(IsVisibilityReminderTimerRunning());
        break;
      case nearby_share::mojom::Visibility::kNoOne:
        EXPECT_FALSE(IsVisibilityReminderTimerRunning());
        break;
      default:
        break;
    }
  } else {
    EXPECT_FALSE(IsVisibilityReminderTimerRunning());
  }
}

TEST_P(NearbySharingServiceVisibilityReminderTest,
       StartOrStopVisibilityReminderTimerAppropriatelyWhenChangeVisibility) {
  bool is_enabled = std::get<0>(GetParam());
  nearby_share::mojom::Visibility visibility = std::get<1>(GetParam());

  SetIsEnabled(is_enabled);
  if (is_enabled) {
    SetVisibility(visibility);
    switch (visibility) {
      case nearby_share::mojom::Visibility::kAllContacts:
      case nearby_share::mojom::Visibility::kSelectedContacts:
        EXPECT_TRUE(IsVisibilityReminderTimerRunning());
        break;
      case nearby_share::mojom::Visibility::kNoOne:
        EXPECT_FALSE(IsVisibilityReminderTimerRunning());
        break;
      default:
        break;
    }
  } else {
    EXPECT_FALSE(IsVisibilityReminderTimerRunning());
  }
}

INSTANTIATE_TEST_SUITE_P(
    NearbySharingServiceImplTest,
    NearbySharingServiceVisibilityReminderTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(nearby_share::mojom::Visibility::kAllContacts,
                        nearby_share::mojom::Visibility::kSelectedContacts,
                        nearby_share::mojom::Visibility::kNoOne),
        testing::Range<size_t>(0, 1 << kTestFeatures.size())));

}  // namespace NearbySharingServiceUnitTests
