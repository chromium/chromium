// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/camera_roll_manager_impl.h"

#include <memory>
#include <utility>

#include "ash/components/phonehub/camera_roll_download_manager.h"
#include "ash/components/phonehub/camera_roll_item.h"
#include "ash/components/phonehub/camera_roll_thumbnail_decoder_impl.h"
#include "ash/components/phonehub/message_receiver.h"
#include "ash/components/phonehub/message_sender.h"
#include "ash/components/phonehub/pref_names.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ash/components/phonehub/util/histogram_util.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace phonehub {

namespace {

// TODO(https://crbug.com/1164001): remove after migrating to ash.
namespace multidevice_setup = ::chromeos::multidevice_setup;
namespace secure_channel = ::chromeos::secure_channel;

constexpr int kMaxCameraRollItemCount = 4;

}  // namespace

// static
void CameraRollManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kHasDismissedCameraRollOnboardingUi,
                                false);
}

CameraRollManagerImpl::CameraRollManagerImpl(
    PrefService* pref_service,
    MessageReceiver* message_receiver,
    MessageSender* message_sender,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::ConnectionManager* connection_manager,
    std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager)
    : pref_service_(pref_service),
      message_receiver_(message_receiver),
      message_sender_(message_sender),
      multidevice_setup_client_(multidevice_setup_client),
      connection_manager_(connection_manager),
      camera_roll_download_manager_(std::move(camera_roll_download_manager)),
      thumbnail_decoder_(std::make_unique<CameraRollThumbnailDecoderImpl>()) {
  message_receiver->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
}

CameraRollManagerImpl::~CameraRollManagerImpl() {
  message_receiver_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

void CameraRollManagerImpl::DownloadItem(
    const proto::CameraRollItemMetadata& item_metadata) {
  proto::FetchCameraRollItemDataRequest request;
  *request.mutable_metadata() = item_metadata;
  message_sender_->SendFetchCameraRollItemDataRequest(request);
}

void CameraRollManagerImpl::OnFetchCameraRollItemDataResponseReceived(
    const proto::FetchCameraRollItemDataResponse& response) {
  if (response.file_availability() !=
      proto::FetchCameraRollItemDataResponse::AVAILABLE) {
    NotifyCameraRollDownloadError(
        CameraRollManager::Observer::DownloadErrorType::kGenericError,
        response.metadata());
    return;
  }

  camera_roll_download_manager_->CreatePayloadFiles(
      response.payload_id(), response.metadata(),
      base::BindOnce(&CameraRollManagerImpl::OnPayloadFilesCreated,
                     weak_ptr_factory_.GetWeakPtr(), response));
}

void CameraRollManagerImpl::OnPayloadFilesCreated(
    const proto::FetchCameraRollItemDataResponse& response,
    CameraRollDownloadManager::CreatePayloadFilesResult result,
    absl::optional<chromeos::secure_channel::mojom::PayloadFilesPtr>
        payload_files) {
  switch (result) {
    case CameraRollDownloadManager::CreatePayloadFilesResult::kSuccess:
      connection_manager_->RegisterPayloadFile(
          response.payload_id(), std::move(payload_files.value()),
          base::BindRepeating(&CameraRollManagerImpl::OnFileTransferUpdate,
                              weak_ptr_factory_.GetWeakPtr(),
                              response.metadata()),
          base::BindOnce(&CameraRollManagerImpl::OnPayloadFileRegistered,
                         weak_ptr_factory_.GetWeakPtr(), response.metadata(),
                         response.payload_id()));
      break;
    case CameraRollDownloadManager::CreatePayloadFilesResult::
        kInsufficientDiskSpace:
      PA_LOG(WARNING) << "CreatePayloadFilesResult: "
                      << static_cast<int>(result);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kInsufficientStorage,
          response.metadata());
      break;
    default:
      PA_LOG(WARNING) << "CreatePayloadFilesResult: "
                      << static_cast<int>(result);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kGenericError,
          response.metadata());
      break;
  }
}

void CameraRollManagerImpl::OnPayloadFileRegistered(
    const proto::CameraRollItemMetadata& metadata,
    int64_t payload_id,
    bool success) {
  if (!success) {
    camera_roll_download_manager_->DeleteFile(payload_id);
    NotifyCameraRollDownloadError(
        CameraRollManager::Observer::DownloadErrorType::kGenericError,
        metadata);
    return;
  }

  proto::InitiateCameraRollItemTransferRequest request;
  *request.mutable_metadata() = metadata;
  request.set_payload_id(payload_id);
  message_sender_->SendInitiateCameraRollItemTransferRequest(request);
}

void CameraRollManagerImpl::OnFileTransferUpdate(
    const proto::CameraRollItemMetadata& metadata,
    chromeos::secure_channel::mojom::FileTransferUpdatePtr update) {
  if (update->status ==
          chromeos::secure_channel::mojom::FileTransferStatus::kFailure ||
      update->status ==
          chromeos::secure_channel::mojom::FileTransferStatus::kCanceled) {
    NotifyCameraRollDownloadError(
        CameraRollManager::Observer::DownloadErrorType::kNetworkConnection,
        metadata);
  }
  camera_roll_download_manager_->UpdateDownloadProgress(std::move(update));
}

void CameraRollManagerImpl::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  UpdateCameraRollAccessStateAndNotifyIfNeeded(
      phone_status_snapshot.properties().camera_roll_access_state());
  if (!is_android_storage_granted_ || !IsCameraRollSettingEnabled()) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
    resetViewRefreshingFlagIfNeeded();
    return;
  }

  SendFetchCameraRollItemsRequest();
}

void CameraRollManagerImpl::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  UpdateCameraRollAccessStateAndNotifyIfNeeded(
      phone_status_update.properties().camera_roll_access_state());
  if (!is_android_storage_granted_ || !IsCameraRollSettingEnabled()) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
    resetViewRefreshingFlagIfNeeded();
    return;
  }

  if (phone_status_update.has_camera_roll_updates()) {
    SendFetchCameraRollItemsRequest();
  }
}

void CameraRollManagerImpl::OnFetchCameraRollItemsResponseReceived(
    const proto::FetchCameraRollItemsResponse& response) {
  thumbnail_decoder_->BatchDecode(
      response, current_items(),
      base::BindOnce(&CameraRollManagerImpl::OnItemThumbnailsDecoded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CameraRollManagerImpl::SendFetchCameraRollItemsRequest() {
  // Clears pending thumbnail decode requests to avoid changing the current item
  // set after sending it with the |FetchCameraRollItemsRequest|. These pending
  // thumbnails will be invalidated anyway when the new response is received.
  CancelPendingThumbnailRequests();

  proto::FetchCameraRollItemsRequest request;
  request.set_max_item_count(kMaxCameraRollItemCount);
  for (const CameraRollItem& current_item : current_items()) {
    *request.add_current_item_metadata() = current_item.metadata();
  }
  message_sender_->SendFetchCameraRollItemsRequest(request);
}

void CameraRollManagerImpl::OnItemThumbnailsDecoded(
    CameraRollThumbnailDecoder::BatchDecodeResult result,
    const std::vector<CameraRollItem>& items) {
  resetViewRefreshingFlagIfNeeded();
  if (result == CameraRollThumbnailDecoder::BatchDecodeResult::kCompleted) {
    SetCurrentItems(items);
  }
}

void CameraRollManagerImpl::CancelPendingThumbnailRequests() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void CameraRollManagerImpl::EnableCameraRollFeatureInSystemSetting() {
  multidevice_setup_client_->SetFeatureEnabledState(
      chromeos::multidevice_setup::mojom::Feature::kPhoneHubCameraRoll,
      /*enabled=*/true, /*auth_token=*/absl::nullopt, base::DoNothing());
  is_refreshing_after_user_opt_in_ = true;
  // Re-compute and update ui immediately instead of waiting for the callback to
  // finish would hide the view on user's tap action, giving a indicator for the
  // user that the action is performed. When camera items are received, camera
  // roll view would be visible again.
  ComputeAndUpdateUiState();
}

bool CameraRollManagerImpl::IsCameraRollSettingEnabled() {
  chromeos::multidevice_setup::mojom::FeatureState camera_roll_feature_state =
      multidevice_setup_client_->GetFeatureState(
          chromeos::multidevice_setup::mojom::Feature::kPhoneHubCameraRoll);
  return camera_roll_feature_state ==
         chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

void CameraRollManagerImpl::resetViewRefreshingFlagIfNeeded() {
  is_refreshing_after_user_opt_in_ = false;
}

void CameraRollManagerImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  if (!IsCameraRollSettingEnabled()) {
    // ClearCurrentItems() would also call ComputeAndUpdateUiState()
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
  } else {
    ComputeAndUpdateUiState();
  }
}

void CameraRollManagerImpl::UpdateCameraRollAccessStateAndNotifyIfNeeded(
    const proto::CameraRollAccessState& access_state) {
  bool updated_storage_granted = access_state.storage_permission_granted();
  if (is_android_storage_granted_ != updated_storage_granted) {
    is_android_storage_granted_ = updated_storage_granted;

    util::LogCameraRollAndroidHasStorageAccessPermission(
        is_android_storage_granted_);
    ComputeAndUpdateUiState();
  }
}

void CameraRollManagerImpl::OnCameraRollOnboardingUiDismissed() {
  pref_service_->SetBoolean(prefs::kHasDismissedCameraRollOnboardingUi, true);
  // Force the observing views to refresh
  ComputeAndUpdateUiState();
}

void CameraRollManagerImpl::ComputeAndUpdateUiState() {
  if (!is_android_storage_granted_) {
    ui_state_ = CameraRollUiState::NO_STORAGE_PERMISSION;
    NotifyCameraRollViewUiStateUpdated();
    return;
  }

  chromeos::multidevice_setup::mojom::FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(
          chromeos::multidevice_setup::mojom::Feature::kPhoneHubCameraRoll);
  switch (feature_state) {
    case chromeos::multidevice_setup::mojom::FeatureState::kDisabledByUser:
      ui_state_ =
          pref_service_->GetBoolean(prefs::kHasDismissedCameraRollOnboardingUi)
              ? CameraRollUiState::SHOULD_HIDE
              : CameraRollUiState::CAN_OPT_IN;
      break;
    case chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser:
      if (is_refreshing_after_user_opt_in_) {
        ui_state_ = CameraRollUiState::LOADING_VIEW;
      } else if (current_items().empty()) {
        ui_state_ = CameraRollUiState::SHOULD_HIDE;
      } else {
        ui_state_ = CameraRollUiState::ITEMS_VISIBLE;
      }
      break;
    default:
      ui_state_ = CameraRollUiState::SHOULD_HIDE;
      break;
  }
  NotifyCameraRollViewUiStateUpdated();
}

}  // namespace phonehub
}  // namespace ash
