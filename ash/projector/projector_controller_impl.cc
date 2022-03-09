// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/projector/projector_metadata_controller.h"
#include "ash/projector/projector_metrics.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

namespace ash {

namespace {

// String format of the screencast name.
constexpr char kScreencastPathFmtStr[] =
    "Recording %d-%02d-%02d %02d.%02d.%02d";

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

std::string GetScreencastName() {
  base::Time::Exploded exploded_time;
  base::Time::Now().LocalExplode(&exploded_time);
  return base::StringPrintf(kScreencastPathFmtStr, exploded_time.year,
                            exploded_time.month, exploded_time.day_of_month,
                            exploded_time.hour, exploded_time.minute,
                            exploded_time.second);
}

}  // namespace

ProjectorControllerImpl::ProjectorControllerImpl()
    : projector_session_(std::make_unique<ash::ProjectorSessionImpl>()),
      metadata_controller_(
          std::make_unique<ash::ProjectorMetadataController>()) {
  if (features::IsProjectorAnnotatorEnabled())
    ui_controller_ = std::make_unique<ash::ProjectorUiController>(this);

  projector_session_->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

ProjectorControllerImpl::~ProjectorControllerImpl() {
  projector_session_->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

// static
ProjectorControllerImpl* ProjectorControllerImpl::Get() {
  return static_cast<ProjectorControllerImpl*>(ProjectorController::Get());
}

void ProjectorControllerImpl::StartProjectorSession(
    const std::string& storage_dir) {
  DCHECK_EQ(GetNewScreencastPrecondition().state,
            NewScreencastPreconditionState::kEnabled);

  auto* controller = CaptureModeController::Get();
  if (!controller->is_recording_in_progress()) {
    controller->SetSource(CaptureModeSource::kFullscreen);
    // A capture mode session can be blocked by many factors, such as policy,
    // DLP, ... etc. We don't start a Projector session until we're sure a
    // capture session started.
    controller->Start(CaptureModeEntryType::kProjector);
    if (controller->IsActive()) {
      projector_session_->Start(storage_dir);
      client_->MinimizeProjectorApp();
    }
  }
}

void ProjectorControllerImpl::CreateScreencastContainerFolder(
    CreateScreencastContainerFolderCallback callback) {
  base::FilePath mounted_path;
  if (!client_->GetDriveFsMountPointPath(&mounted_path)) {
    LOG(ERROR) << "Failed to get DriveFs mounted point path.";
    ProjectorUiController::ShowSaveFailureNotification();
    std::move(callback).Run(base::FilePath());
    return;
  }

  auto path = mounted_path.Append("root")
                  .Append(projector_session_->storage_dir())
                  .Append(GetScreencastName());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateDirectory, path),
      base::BindOnce(&ProjectorControllerImpl::OnContainerFolderCreated,
                     weak_factory_.GetWeakPtr(), path, std::move(callback)));
}

void ProjectorControllerImpl::SetClient(ProjectorClient* client) {
  client_ = client;
}

void ProjectorControllerImpl::OnSpeechRecognitionAvailabilityChanged(
    SpeechRecognitionAvailability availability) {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled())
    return;

  if (availability == speech_recognition_availability_)
    return;

  speech_recognition_availability_ = availability;

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
  is_speech_recognition_on_ = false;

  ProjectorUiController::ShowFailureNotification(
      IDS_ASH_PROJECTOR_FAILURE_MESSAGE_TRANSCRIPTION);

  CaptureModeController::Get()->EndVideoRecording(
      EndRecordingReason::kProjectorTranscriptionError);
}

void ProjectorControllerImpl::OnSpeechRecognitionStopped() {
  if (projector_session_->screencast_container_path()) {
    // Finish saving the screencast if the container is available. The container
    // might be unavailable if fail in creating the directory.
    SaveScreencast();
  }

  is_speech_recognition_on_ = false;
  projector_session_->Stop();
}

bool ProjectorControllerImpl::IsEligible() const {
  return speech_recognition_availability_ ==
             SpeechRecognitionAvailability::kAvailable ||
         ProjectorController::AreExtendedProjectorFeaturesDisabled();
}

NewScreencastPrecondition
ProjectorControllerImpl::GetNewScreencastPrecondition() const {
  NewScreencastPrecondition result;

  // For development purposes on the x11 simulator, on-device speech recognition
  // and DriveFS are not supported.
  if (!ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    switch (speech_recognition_availability_) {
      case SpeechRecognitionAvailability::
          kOnDeviceSpeechRecognitionNotSupported:
        result.state = NewScreencastPreconditionState::kDisabled;
        result.reasons = {NewScreencastPreconditionReason::
                              kOnDeviceSpeechRecognitionNotSupported};
        return result;
      case SpeechRecognitionAvailability::kUserLanguageNotSupported:
        result.state = NewScreencastPreconditionState::kDisabled;
        result.reasons = {
            NewScreencastPreconditionReason::kUserLocaleNotSupported};
        return result;

      // We will attempt to install SODA.
      case SpeechRecognitionAvailability::kSodaNotInstalled:
      case SpeechRecognitionAvailability::kSodaInstalling:
        result.state = NewScreencastPreconditionState::kDisabled;
        result.reasons = {
            NewScreencastPreconditionReason::kSodaDownloadInProgress};
        return result;
      case SpeechRecognitionAvailability::kSodaInstallationError:
        result.state = NewScreencastPreconditionState::kDisabled;
        result.reasons = {
            NewScreencastPreconditionReason::kSodaInstallationError};
        return result;
      case SpeechRecognitionAvailability::kAvailable:
        break;
    }

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

  if (CaptureModeController::Get()->is_recording_in_progress()) {
    result.state = NewScreencastPreconditionState::kDisabled;
    result.reasons = {
        NewScreencastPreconditionReason::kScreenRecordingInProgress};
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

void ProjectorControllerImpl::OnToolSet(const AnnotatorTool& tool) {
  // TODO(b/198184362): Reflect the annotator tool changes on the Projector
  // toolbar.
}

void ProjectorControllerImpl::OnUndoRedoAvailabilityChanged(
    bool undo_available,
    bool redo_available) {
  // TODO(b/198184362): Reflect undo and redo buttons availability on the
  // Projector toolbar.
}

void ProjectorControllerImpl::OnRecordingStarted(bool is_in_projector_mode) {
  if (!is_in_projector_mode) {
    OnNewScreencastPreconditionChanged();
    return;
  }
  if (ui_controller_)
    ui_controller_->ShowToolbar();

  StartSpeechRecognition();
  metadata_controller_->OnRecordingStarted();

  RecordCreationFlowMetrics(ProjectorCreationFlow::kRecordingStarted);
}

void ProjectorControllerImpl::OnRecordingEnded(bool is_in_projector_mode) {
  if (!is_in_projector_mode) {
    OnNewScreencastPreconditionChanged();
    return;
  }

  DCHECK(projector_session_->is_active());

  // TODO(b/197152209): move closing selfie cam to ProjectorUiController.
  if (client_->IsSelfieCamVisible())
    client_->CloseSelfieCam();

  // Close Projector toolbar if ui controller is present.
  if (ui_controller_)
    ui_controller_->CloseToolbar();

  MaybeStopSpeechRecognition();

  // At this point, the screencast might not synced to Drive yet. Open
  // Projector App which shows the Gallery view by default.
  client_->OpenProjectorApp();

  RecordCreationFlowMetrics(ProjectorCreationFlow::kRecordingEnded);
}

void ProjectorControllerImpl::OnRecordingStartAborted() {
  DCHECK(projector_session_->is_active());

  // Delete the DriveFS path that might have been created for this aborted
  // session if any.
  if (projector_session_->screencast_container_path()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::GetDeletePathRecursivelyCallback(),
                       *projector_session_->screencast_container_path()));
  }

  projector_session_->Stop();

  client_->OpenProjectorApp();

  RecordCreationFlowMetrics(ProjectorCreationFlow::kRecordingAborted);
}

void ProjectorControllerImpl::OnMarkerPressed() {
  DCHECK(ui_controller_);
  ui_controller_->OnMarkerPressed();
}

void ProjectorControllerImpl::SetAnnotatorTool(const AnnotatorTool& tool) {
  DCHECK(ui_controller_);
  ui_controller_->SetAnnotatorTool(tool);
}

void ProjectorControllerImpl::ResetTools() {
  if (ui_controller_)
    ui_controller_->ResetTools();
}

bool ProjectorControllerImpl::IsAnnotatorEnabled() {
  return ui_controller_ && ui_controller_->is_annotator_enabled();
}

void ProjectorControllerImpl::OnNewScreencastPreconditionChanged() {
  // `client_` could be not available in unit tests.
  if (client_)
    client_->OnNewScreencastPreconditionChanged(GetNewScreencastPrecondition());
}

void ProjectorControllerImpl::SetProjectorUiControllerForTest(
    std::unique_ptr<ProjectorUiController> ui_controller) {
  ui_controller_ = std::move(ui_controller);
}

void ProjectorControllerImpl::SetProjectorMetadataControllerForTest(
    std::unique_ptr<ProjectorMetadataController> metadata_controller) {
  metadata_controller_ = std::move(metadata_controller);
}

void ProjectorControllerImpl::OnAudioNodesChanged() {
  OnNewScreencastPreconditionChanged();
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
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled())
    return;

  DCHECK(speech_recognition_availability_ ==
         SpeechRecognitionAvailability::kAvailable);
  DCHECK(!is_speech_recognition_on_);
  DCHECK_NE(client_, nullptr);
  client_->StartSpeechRecognition();
  is_speech_recognition_on_ = true;
}

void ProjectorControllerImpl::MaybeStopSpeechRecognition() {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled() ||
      !is_speech_recognition_on_) {
    OnSpeechRecognitionStopped();
    return;
  }

  DCHECK(speech_recognition_availability_ ==
         SpeechRecognitionAvailability::kAvailable);
  DCHECK_NE(client_, nullptr);
  client_->StopSpeechRecognition();
  is_speech_recognition_on_ = false;
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
  std::move(callback).Run(GetScreencastFilePathNoExtension());
}

void ProjectorControllerImpl::SaveScreencast() {
  metadata_controller_->SaveMetadata(GetScreencastFilePathNoExtension());
}

base::FilePath ProjectorControllerImpl::GetScreencastFilePathNoExtension()
    const {
  auto screencast_container_path =
      projector_session_->screencast_container_path();

  DCHECK(screencast_container_path.has_value());
  return screencast_container_path->Append(GetScreencastName());
}

}  // namespace ash
