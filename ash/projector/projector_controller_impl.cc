// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include <vector>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/projector/projector_metadata_controller.h"
#include "ash/projector/projector_metrics.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/gfx/image/image.h"

namespace ash {

namespace {

constexpr base::TimeDelta kForceEndRecognitionSessionTimer = base::Seconds(90);

// Create directory. Returns true if saving succeeded, or false otherwise.
bool CreateDirectory(const base::FilePath& path) {
  DCHECK(!base::CurrentUIThread::IsSet());
  DCHECK(!path.empty());

  // The path is constructed from datetime which should be unique for most
  // cases. In case it is already exist, returns false.
  if (base::PathExists(path)) {
    LOG(ERROR) << "Path has already existed: " << path;
    return false;
  }

  if (!base::CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create path: " << path;
    return false;
  }

  return true;
}

// Writes the given `data` in a file with `path`. Returns true if saving
// succeeded, or false otherwise.
bool SaveFile(scoped_refptr<base::RefCountedMemory> data,
              const base::FilePath& path) {
  // `data` could be empty in unit tests.
  if (!data)
    return false;
  const int size = static_cast<int>(data->size());
  if (!size)
    return false;

  if (!base::WriteFile(path, *data)) {
    LOG(ERROR) << "Failed to save file: " << path;
    return false;
  }

  return true;
}

scoped_refptr<base::RefCountedMemory> EncodeImage(
    const gfx::ImageSkia& image_skia) {
  return gfx::Image(image_skia).As1xPNGBytes();
}

NewScreencastPrecondition OnDeviceRecognitionAvailabilityToPrecondition(
    OnDeviceRecognitionAvailability availability) {
  NewScreencastPrecondition result;
  switch (availability) {
    case OnDeviceRecognitionAvailability::kAvailable:
      result.state = NewScreencastPreconditionState::kEnabled;
      result.reasons = {NewScreencastPreconditionReason::kEnabledBySoda};
      return result;
    case OnDeviceRecognitionAvailability::kSodaNotAvailable:
      result.state = NewScreencastPreconditionState::kDisabled;
      result.reasons = {NewScreencastPreconditionReason::
                            kOnDeviceSpeechRecognitionNotSupported};
      return result;
    case OnDeviceRecognitionAvailability::kUserLanguageNotAvailable:
      result.state = NewScreencastPreconditionState::kDisabled;
      result.reasons = {
          NewScreencastPreconditionReason::kUserLocaleNotSupported};
      return result;

    // We will attempt to install SODA.
    case OnDeviceRecognitionAvailability::kSodaNotInstalled:
    case OnDeviceRecognitionAvailability::kSodaInstalling:
      result.state = NewScreencastPreconditionState::kDisabled;
      result.reasons = {
          NewScreencastPreconditionReason::kSodaDownloadInProgress};
      return result;
    case OnDeviceRecognitionAvailability::kSodaInstallationErrorUnspecified:
      result.state = NewScreencastPreconditionState::kDisabled;
      result.reasons = {
          NewScreencastPreconditionReason::kSodaInstallationErrorUnspecified};
      return result;
    case OnDeviceRecognitionAvailability::kSodaInstallationErrorNeedsReboot:
      result.state = NewScreencastPreconditionState::kDisabled;
      result.reasons = {
          NewScreencastPreconditionReason::kSodaInstallationErrorNeedsReboot};
      return result;
  }
}

NewScreencastPrecondition ServerBasedRecognitionAvailabilityToPrecondition(
    ServerBasedRecognitionAvailability availability) {
  NewScreencastPrecondition result;
  switch (availability) {
    case ServerBasedRecognitionAvailability::kAvailable:
      result.state = NewScreencastPreconditionState::kEnabled;
      result.reasons = {NewScreencastPreconditionReason::
                            kEnabledByServerSideSpeechRecognition};
      return result;
    case ServerBasedRecognitionAvailability::kUserLanguageNotAvailable:
      result.state = NewScreencastPreconditionState::kDisabled;
      result.reasons = {
          NewScreencastPreconditionReason::kUserLocaleNotSupported};
      return result;
    case ServerBasedRecognitionAvailability::
        kServerBasedRecognitionNotAvailable:
      result.state = NewScreencastPreconditionState::kDisabled;
      // TODO(b:245613717): Add a precondition reason for server based not
      // available.
      result.reasons = {NewScreencastPreconditionReason::kOthers};
      return result;
  }
}

const base::FilePath::StringPieceType getMetadataFileExtension() {
  return ash::kProjectorV2MetadataFileExtension;
}

}  // namespace

ProjectorControllerImpl::ProjectorControllerImpl()
    : projector_session_(std::make_unique<ash::ProjectorSessionImpl>()),
      metadata_controller_(
          std::make_unique<ash::ProjectorMetadataController>()) {
  ui_controller_ = std::make_unique<ash::ProjectorUiController>();

  projector_session_->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
  CaptureModeController::Get()->AddObserver(this);
}

ProjectorControllerImpl::~ProjectorControllerImpl() {
  CaptureModeController::Get()->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  projector_session_->RemoveObserver(this);
}

// static
ProjectorControllerImpl* ProjectorControllerImpl::Get() {
  return static_cast<ProjectorControllerImpl*>(ProjectorController::Get());
}

// static
void ProjectorControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(
      prefs::kProjectorAnnotatorLastUsedMarkerColor, 0u,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void ProjectorControllerImpl::StartProjectorSession(
    const base::SafeBaseName& storage_dir) {
  CHECK_EQ(GetNewScreencastPrecondition().state,
           NewScreencastPreconditionState::kEnabled);

  auto* controller = CaptureModeController::Get();
  if (controller->can_start_new_recording()) {
    // A capture mode session can be blocked by many factors, such as policy,
    // DLP, ... etc. We don't start a Projector session until we're sure a
    // capture session started.
    controller->Start(
        CaptureModeEntryType::kProjector,
        base::BindOnce(&ProjectorControllerImpl::OnSessionStartAttempted,
                       weak_factory_.GetWeakPtr(), storage_dir));

    dlp_restriction_checked_completed_ = false;
  }
}

void ProjectorControllerImpl::SetClient(ProjectorClient* client) {
  client_ = client;
}

void ProjectorControllerImpl::OnSpeechRecognitionAvailabilityChanged() {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled())
    return;

  OnNewScreencastPreconditionChanged();
}

void ProjectorControllerImpl::OnTranscription(
    const media::SpeechRecognitionResult& result) {
  if (result.is_final && result.timing_information.has_value()) {
    // Records final transcript.
    metadata_controller_->RecordTranscription(result);
  }
}

void ProjectorControllerImpl::OnTranscriptionError() {
  const auto end_state =
      speech_recognition_state_ == SpeechRecognitionState::kRecognitionStopping
          ? SpeechRecognitionEndState::
                kSpeechRecognitionEncounteredErrorWhileStopping
          : SpeechRecognitionEndState::kSpeechRecognitionEnounteredError;
  RecordSpeechRecognitionEndState(end_state, use_on_device_speech_recognition);

  force_stop_recognition_timer_.AbandonAndStop();

  // TODO(b/261093550) Investigate the real reason why
  // we get a speech recognition error after we notify it to
  // stop.
  if (speech_recognition_state_ !=
      SpeechRecognitionState::kRecognitionStopping) {
    ProjectorUiController::ShowFailureNotification(
        IDS_ASH_PROJECTOR_FAILURE_MESSAGE_TRANSCRIPTION);
  }

  speech_recognition_state_ = SpeechRecognitionState::kRecognitionError;
  metadata_controller_->SetSpeechRecognitionStatus(RecognitionStatus::kError);

  auto* capture_mode_controller = CaptureModeController::Get();
  if (capture_mode_controller->is_recording_in_progress()) {
    capture_mode_controller->EndVideoRecording(
        EndRecordingReason::kProjectorTranscriptionError);
  } else {
    MaybeWrapUpRecording();
  }
}

void ProjectorControllerImpl::OnSpeechRecognitionStopped(bool forced) {
  const auto end_state =
      forced ? SpeechRecognitionEndState::kSpeechRecognitionForcedStopped
             : SpeechRecognitionEndState::kSpeechRecognitionSuccessfullyStopped;
  RecordSpeechRecognitionEndState(end_state, use_on_device_speech_recognition);

  speech_recognition_state_ = SpeechRecognitionState::kRecognitionNotStarted;

  const auto metadata_recognition_status =
      forced ? RecognitionStatus::kIncomplete : RecognitionStatus::kComplete;
  metadata_controller_->SetSpeechRecognitionStatus(metadata_recognition_status);

  // Try to wrap up recording. This can be no-op if DLP check is not completed.
  MaybeWrapUpRecording();
  force_stop_recognition_timer_.AbandonAndStop();
}

NewScreencastPrecondition
ProjectorControllerImpl::GetNewScreencastPrecondition() const {
  NewScreencastPrecondition result;
  // Make the default reason to be `kEnabledBySoda`.
  result.reasons = {NewScreencastPreconditionReason::kEnabledBySoda};

  // For development purposes on the x11 simulator, on-device speech recognition
  // and DriveFS are not supported.
  if (!ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    const auto availability = client_->GetSpeechRecognitionAvailability();
    if (availability.use_on_device) {
      result = OnDeviceRecognitionAvailabilityToPrecondition(
          availability.on_device_availability);
    } else {
      result = ServerBasedRecognitionAvailabilityToPrecondition(
          availability.server_based_availability);
    }

    if (result.state != NewScreencastPreconditionState::kEnabled)
      return result;

    if (!client_->IsDriveFsMounted()) {
      result.state = NewScreencastPreconditionState::kDisabled;
      result.reasons = {
          client_->IsDriveFsMountFailed()
              ? NewScreencastPreconditionReason::kDriveFsMountFailed
              : NewScreencastPreconditionReason::kDriveFsUnmounted};
      return result;
    }
  }

  if (projector_session_->is_active()) {
    result.state = NewScreencastPreconditionState::kDisabled;
    result.reasons = {NewScreencastPreconditionReason::kInProjectorSession};
    return result;
  }

  auto* capture_mode_controller = CaptureModeController::Get();
  if (!capture_mode_controller->can_start_new_recording()) {
    result.state = NewScreencastPreconditionState::kDisabled;
    result.reasons = {
        NewScreencastPreconditionReason::kScreenRecordingInProgress};
    return result;
  }

  if (capture_mode_controller->IsAudioCaptureDisabledByPolicy()) {
    result.state = NewScreencastPreconditionState::kDisabled;
    result.reasons = {
        NewScreencastPreconditionReason::kAudioCaptureDisabledByPolicy};
    return result;
  }

  if (!IsInputDeviceAvailable()) {
    result.state = NewScreencastPreconditionState::kDisabled;
    result.reasons = {NewScreencastPreconditionReason::kNoMic};
    return result;
  }

  result.state = NewScreencastPreconditionState::kEnabled;
  return result;
}

void ProjectorControllerImpl::CreateScreencastContainerFolder(
    CreateScreencastContainerFolderCallback callback) {
  base::FilePath mounted_path;
  if (!client_->GetBaseStoragePath(&mounted_path)) {
    LOG(ERROR) << "Failed to get DriveFs mounted point path.";
    ProjectorUiController::ShowSaveFailureNotification();
    std::move(callback).Run(base::FilePath());
    return;
  }

  auto path = mounted_path.Append("root")
                  .Append(projector_session_->storage_dir())
                  .Append(projector_session_->screencast_name());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateDirectory, path),
      base::BindOnce(&ProjectorControllerImpl::OnContainerFolderCreated,
                     weak_factory_.GetWeakPtr(), path, std::move(callback)));
}

void ProjectorControllerImpl::OnNewScreencastPreconditionChanged() {
  // `client_` could be not available in unit tests.
  if (client_) {
    client_->OnNewScreencastPreconditionChanged(GetNewScreencastPrecondition());
  }
}

void ProjectorControllerImpl::SetProjectorMetadataControllerForTest(
    std::unique_ptr<ProjectorMetadataController> metadata_controller) {
  metadata_controller_ = std::move(metadata_controller);
}

void ProjectorControllerImpl::SetOnPathDeletedCallbackForTest(
    OnPathDeletedCallback callback) {
  on_path_deleted_callback_ = std::move(callback);
}

void ProjectorControllerImpl::SetOnFileSavedCallbackForTest(
    OnFileSavedCallback callback) {
  on_file_saved_callback_ = std::move(callback);
}

void ProjectorControllerImpl::OnAudioNodesChanged() {
  OnNewScreencastPreconditionChanged();
}

void ProjectorControllerImpl::OnRecordingStarted(aura::Window* current_root) {
  if (!projector_session_->is_active()) {
    OnNewScreencastPreconditionChanged();
    return;
  }

  StartSpeechRecognition();
  metadata_controller_->OnRecordingStarted();

  RecordCreationFlowMetrics(ProjectorCreationFlow::kRecordingStarted);
}

void ProjectorControllerImpl::OnRecordingEnded() {
  if (!projector_session_->is_active()) {
    return;
  }

  MaybeStopSpeechRecognition();

  RecordCreationFlowMetrics(ProjectorCreationFlow::kRecordingEnded);
}

void ProjectorControllerImpl::OnVideoFileFinalized(
    bool user_deleted_video_file,
    const gfx::ImageSkia& thumbnail) {
  if (!projector_session_->is_active()) {
    OnNewScreencastPreconditionChanged();
    return;
  }

  dlp_restriction_checked_completed_ = true;
  user_deleted_video_file_ = user_deleted_video_file;

  if (user_deleted_video_file) {
    CleanupContainerFolder();
  } else {
    SaveThumbnailFile(thumbnail);
  }

  // Try to wrap up recording.
  MaybeWrapUpRecording();

  // At this point, the screencast might not synced to Drive yet. Open
  // Projector App which shows the Gallery view by default.
  if (client_) {
    client_->OpenProjectorApp();
  }
}

void ProjectorControllerImpl::OnRecordedWindowChangingRoot(
    aura::Window* new_root) {}

void ProjectorControllerImpl::OnRecordingStartAborted() {
  if (!projector_session_->is_active()) {
    OnNewScreencastPreconditionChanged();
    return;
  }

  // Delete the DriveFS path that might have been created for this aborted
  // session if any.
  CleanupContainerFolder();

  projector_session_->Stop();

  if (CaptureModeController::Get()->IsAudioCaptureDisabledByPolicy()) {
    ui_controller_->ShowFailureNotification(
        IDS_ASH_PROJECTOR_ABORT_BY_AUDIO_POLICY_TEXT,
        IDS_ASH_PROJECTOR_ABORT_BY_AUDIO_POLICY_TITLE);
  }

  if (client_) {
    client_->OpenProjectorApp();
  }

  RecordCreationFlowMetrics(ProjectorCreationFlow::kRecordingAborted);
}

void ProjectorControllerImpl::OnProjectorSessionActiveStateChanged(
    bool active) {
  OnNewScreencastPreconditionChanged();
}

bool ProjectorControllerImpl::IsInputDeviceAvailable() const {
  uint64_t input_id = CrasAudioHandler::Get()->GetPrimaryActiveInputNode();
  const AudioDevice* input_device =
      CrasAudioHandler::Get()->GetDeviceFromId(input_id);
  return input_device != nullptr;
}

void ProjectorControllerImpl::StartSpeechRecognition() {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled() || !client_)
    return;

  const auto availability = client_->GetSpeechRecognitionAvailability();
  DCHECK(availability.IsAvailable());
  DCHECK(speech_recognition_state_ !=
         SpeechRecognitionState::kRecognitionStarted);

  client_->StartSpeechRecognition();
  speech_recognition_state_ = SpeechRecognitionState::kRecognitionStarted;
  use_on_device_speech_recognition = availability.use_on_device;
}

void ProjectorControllerImpl::MaybeStopSpeechRecognition() {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled() ||
      speech_recognition_state_ ==
          SpeechRecognitionState::kRecognitionNotStarted ||
      !client_) {
    OnSpeechRecognitionStopped(/*forced=*/false);
    return;
  }

  DCHECK(client_->GetSpeechRecognitionAvailability().IsAvailable());

  // We are already stopping.
  if (speech_recognition_state_ ==
      SpeechRecognitionState::kRecognitionStopping) {
    return;
  }

  speech_recognition_state_ = SpeechRecognitionState::kRecognitionStopping;
  client_->StopSpeechRecognition();

  force_stop_recognition_timer_.Start(
      FROM_HERE, kForceEndRecognitionSessionTimer,
      base::BindOnce(&ProjectorControllerImpl::ForceEndSpeechRecognition,
                     weak_factory_.GetWeakPtr()));
}

void ProjectorControllerImpl::ForceEndSpeechRecognition() {
  if (!client_) {
    return;
  }

  DCHECK_EQ(speech_recognition_state_,
            SpeechRecognitionState::kRecognitionStopping);

  client_->ForceEndSpeechRecognition();
}

void ProjectorControllerImpl::OnSessionStartAttempted(
    const base::SafeBaseName& storage_dir,
    bool success) {
  if (success) {
    projector_session_->Start(storage_dir);
    client_->MinimizeProjectorApp();
  }
}

void ProjectorControllerImpl::OnContainerFolderCreated(
    const base::FilePath& path,
    CreateScreencastContainerFolderCallback callback,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to create screencast container path: "
               << path.DirName();
    ProjectorUiController::ShowSaveFailureNotification();
    std::move(callback).Run(base::FilePath());
    return;
  }

  projector_session_->set_screencast_container_path(path);
  // Suppresses system notification for media file, metadata file and thumbnail
  // even they haven't been saved yet. Once any file gets saved, syncing will
  // start immediately, we want to make sure the notifications are suppressed
  // before the sync.
  client_->ToggleFileSyncingNotificationForPaths(GetScreencastFilePaths(),
                                                 /*suppress=*/true);
  std::move(callback).Run(
      projector_session_->GetScreencastFilePathNoExtension());
}

void ProjectorControllerImpl::SaveScreencast() {
  metadata_controller_->SaveMetadata(
      projector_session_->GetScreencastFilePathNoExtension());
}

void ProjectorControllerImpl::MaybeWrapUpRecording() {
  // Speech recognition could stopped before DLP check is completed, only wrap
  // up the recording if DLP check is completed.
  if (!dlp_restriction_checked_completed_) {
    return;
  }

  // We reach this stage in the following scenarios:
  // 1. Recording has stopped but speech recognition is not yet complete.
  // 2. Both recording and speech recognition have completed.
  // In both cases, we save the screencast. However, we will end the session
  // when both speech recognition and recording have completed.
  if (!user_deleted_video_file_ &&
      projector_session_->screencast_container_path().has_value()) {
    // Finish saving the screencast if the container is available. The container
    // might be unavailable if fail in creating the directory or the folder is
    // deleted due to DLP.
    SaveScreencast();
  }

  if ((speech_recognition_state_ ==
           SpeechRecognitionState::kRecognitionNotStarted ||
       speech_recognition_state_ ==
           SpeechRecognitionState::kRecognitionError) &&
      projector_session_->is_active()) {
    projector_session_->Stop();
  }
}

void ProjectorControllerImpl::SaveThumbnailFile(
    const gfx::ImageSkia& thumbnail) {
  auto screencast_container_path =
      projector_session_->screencast_container_path();
  if (!screencast_container_path.has_value())
    return;

  auto path =
      screencast_container_path->Append(kScreencastDefaultThumbnailFileName);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SaveFile, EncodeImage(thumbnail), path),
      on_file_saved_callback_
          ? base::BindOnce(std::move(on_file_saved_callback_), path)
          : base::BindOnce([](bool success) {
              if (!success) {
                // Thumbnail is not a critical asset. Fail silently for now.
                LOG(ERROR) << "Failed to save the thumbnail file.";
              }
            }));
}

void ProjectorControllerImpl::CleanupContainerFolder() {
  auto screencast_container_path =
      projector_session_->screencast_container_path();

  if (!screencast_container_path.has_value())
    return;
  client_->ToggleFileSyncingNotificationForPaths(GetScreencastFilePaths(),
                                                 /*suppress=*/false);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::DeletePathRecursively, *screencast_container_path),
      on_path_deleted_callback_
          ? base::BindOnce(std::move(on_path_deleted_callback_),
                           *screencast_container_path)
          : base::BindOnce(
                [](const base::FilePath& path, bool success) {
                  if (!success)
                    LOG(ERROR) << "Failed to delete the folder: " << path;
                },
                *screencast_container_path));
}

std::vector<base::FilePath> ProjectorControllerImpl::GetScreencastFilePaths()
    const {
  const auto& container_folder =
      projector_session_->screencast_container_path();
  DCHECK(container_folder);
  const base::FilePath path_with_no_extension =
      projector_session_->GetScreencastFilePathNoExtension();
  const base::FilePath::StringPieceType metadata_file_extension =
      getMetadataFileExtension();
  return {path_with_no_extension.AddExtension(metadata_file_extension),
          path_with_no_extension.AddExtension(kProjectorMediaFileExtension),
          container_folder->Append(kScreencastDefaultThumbnailFileName)};
}

}  // namespace ash
