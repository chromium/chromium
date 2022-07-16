// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/projector/projector_metadata_controller.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
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
    "Screencast %d-%02d-%02d %02d.%02d.%02d";

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
      ui_controller_(std::make_unique<ash::ProjectorUiController>(this)),
      metadata_controller_(
          std::make_unique<ash::ProjectorMetadataController>()) {
  projector_session_->AddObserver(this);
}

ProjectorControllerImpl::~ProjectorControllerImpl() {
  projector_session_->RemoveObserver(this);
}

// static
ProjectorControllerImpl* ProjectorControllerImpl::Get() {
  return static_cast<ProjectorControllerImpl*>(ProjectorController::Get());
}

void ProjectorControllerImpl::StartProjectorSession(
    const std::string& storage_dir) {
  DCHECK(CanStartNewSession());

  auto* controller = CaptureModeController::Get();
  if (!controller->is_recording_in_progress()) {
    // A capture mode session can be blocked by many factors, such as policy,
    // DLP, ... etc. We don't start a Projector session until we're sure a
    // capture session started.
    controller->Start(CaptureModeEntryType::kProjector);
    if (controller->IsActive()) {
      projector_session_->Start(storage_dir);
    }
  }
}

void ProjectorControllerImpl::CreateScreencastContainerFolder(
    CreateScreencastContainerFolderCallback callback) {
  base::FilePath mounted_path;
  if (!client_->GetDriveFsMountPointPath(&mounted_path)) {
    LOG(ERROR) << "Failed to get DriveFs mounted point path.";
    ProjectorUiController::ShowFailureNotification(
        IDS_ASH_PROJECTOR_FAILURE_MESSAGE_DRIVEFS);
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

void ProjectorControllerImpl::OnSpeechRecognitionAvailable(bool available) {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled())
    return;

  if (available == is_speech_recognition_available_)
    return;

  is_speech_recognition_available_ = available;

  OnNewScreencastPreconditionChanged();
}

void ProjectorControllerImpl::OnTranscription(
    const media::SpeechRecognitionResult& result) {
  // Render transcription.
  if (is_caption_on_) {
    ui_controller_->OnTranscription(result.transcription, result.is_final);
  }

  if (result.is_final && result.timing_information.has_value()) {
    // Records final transcript.
    metadata_controller_->RecordTranscription(result);
  }
}

void ProjectorControllerImpl::OnTranscriptionError() {
  CaptureModeController::Get()->EndVideoRecording(
      EndRecordingReason::kProjectorTranscriptionError);
}

bool ProjectorControllerImpl::IsEligible() const {
  return is_speech_recognition_available_ ||
         ProjectorController::AreExtendedProjectorFeaturesDisabled();
}

bool ProjectorControllerImpl::CanStartNewSession() const {
  // TODO(crbug.com/1165435) Add other pre-conditions to starting a new
  // projector session.
  return IsEligible() && !projector_session_->is_active() &&
         client_->IsDriveFsMounted();
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

void ProjectorControllerImpl::SetCaptionBubbleState(bool is_on) {
  ui_controller_->SetCaptionBubbleState(is_on);
}

void ProjectorControllerImpl::OnCaptionBubbleModelStateChanged(bool is_on) {
  is_caption_on_ = is_on;
}

void ProjectorControllerImpl::MarkKeyIdea() {
  metadata_controller_->RecordKeyIdea();
  ui_controller_->OnKeyIdeaMarked();
}

void ProjectorControllerImpl::OnRecordingStarted() {
  ui_controller_->ShowToolbar();
  StartSpeechRecognition();
  ui_controller_->OnRecordingStateChanged(true /* started */);
  metadata_controller_->OnRecordingStarted();
}

void ProjectorControllerImpl::OnRecordingEnded() {
  DCHECK(projector_session_->is_active());

  StopSpeechRecognition();
  ui_controller_->OnRecordingStateChanged(false /* started */);

  // TODO(b/197152209): move closing selfie cam to ProjectorUiController.
  if (client_->IsSelfieCamVisible())
    client_->CloseSelfieCam();
  // Close Projector toolbar.
  ui_controller_->CloseToolbar();

  if (projector_session_->screencast_container_path()) {
    // Finish saving the screencast if the container is available. The container
    // might be unavailable if fail in creating the directory.
    SaveScreencast();
  }

  projector_session_->Stop();

  // At this point, the screencast might not synced to Drive yet.  Open
  // Projector App which showing the Gallery view by default.
  client_->OpenProjectorApp();
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
}

void ProjectorControllerImpl::OnLaserPointerPressed() {
  ui_controller_->OnLaserPointerPressed();
}

void ProjectorControllerImpl::OnMarkerPressed() {
  ui_controller_->OnMarkerPressed();
}

void ProjectorControllerImpl::OnClearAllMarkersPressed() {
  ui_controller_->OnClearAllMarkersPressed();
}

void ProjectorControllerImpl::OnUndoPressed() {
  ui_controller_->OnUndoPressed();
}

void ProjectorControllerImpl::OnSelfieCamPressed(bool enabled) {
  ui_controller_->OnSelfieCamPressed(enabled);

  DCHECK_NE(client_, nullptr);
  if (enabled == client_->IsSelfieCamVisible())
    return;

  if (enabled) {
    client_->ShowSelfieCam();
    return;
  }
  client_->CloseSelfieCam();
}

void ProjectorControllerImpl::OnMagnifierButtonPressed(bool enabled) {
  ui_controller_->OnMagnifierButtonPressed(enabled);
}

void ProjectorControllerImpl::OnChangeMarkerColorPressed(SkColor new_color) {
  ui_controller_->OnChangeMarkerColorPressed(new_color);
}

void ProjectorControllerImpl::OnNewScreencastPreconditionChanged() {
  client_->OnNewScreencastPreconditionChanged(CanStartNewSession());
}

void ProjectorControllerImpl::SetProjectorUiControllerForTest(
    std::unique_ptr<ProjectorUiController> ui_controller) {
  ui_controller_ = std::move(ui_controller);
}

void ProjectorControllerImpl::SetProjectorMetadataControllerForTest(
    std::unique_ptr<ProjectorMetadataController> metadata_controller) {
  metadata_controller_ = std::move(metadata_controller);
}

void ProjectorControllerImpl::OnProjectorSessionActiveStateChanged(
    bool active) {
  OnNewScreencastPreconditionChanged();
}

void ProjectorControllerImpl::StartSpeechRecognition() {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled())
    return;

  DCHECK(is_speech_recognition_available_);
  DCHECK(!is_speech_recognition_on_);
  DCHECK_NE(client_, nullptr);
  client_->StartSpeechRecognition();
  is_speech_recognition_on_ = true;
}

void ProjectorControllerImpl::StopSpeechRecognition() {
  if (ProjectorController::AreExtendedProjectorFeaturesDisabled())
    return;

  DCHECK(is_speech_recognition_available_);
  DCHECK(is_speech_recognition_on_);
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
    ProjectorUiController::ShowFailureNotification(
        IDS_ASH_PROJECTOR_FAILURE_MESSAGE_SAVE_SCREENCAST);
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
