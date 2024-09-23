// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
#define ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_observer.h"
#include "ash/projector/model/projector_session_impl.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "base/files/safe_base_name.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "third_party/skia/include/core/SkColor.h"

class PrefRegistrySimple;

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class ProjectorClient;
class ProjectorUiController;
class ProjectorMetadataController;

// A controller to handle projector functionalities.
class ASH_EXPORT ProjectorControllerImpl
    : public ProjectorController,
      public ProjectorSessionObserver,
      public CrasAudioHandler::AudioObserver,
      public CaptureModeObserver {
 public:
  // Callback that should be executed when the screencast container directory is
  // created. `screencast_file_path_no_extension` is the path of screencast file
  // without extension. `screencast_file_path_no_extension` will be empty if
  // fail in creating the directory. The path will be used for generating the
  // screencast media file by appending the media file extension.
  using CreateScreencastContainerFolderCallback = base::OnceCallback<void(
      const base::FilePath& screencast_file_path_no_extension)>;

  // Callback that should be executed when the given `path` is deleted.
  using OnPathDeletedCallback =
      base::OnceCallback<void(const base::FilePath& path, bool success)>;

  // Callback that should be executed when the given file `path` is saved.
  using OnFileSavedCallback =
      base::OnceCallback<void(const base::FilePath& path, bool success)>;

  ProjectorControllerImpl();
  ProjectorControllerImpl(const ProjectorControllerImpl&) = delete;
  ProjectorControllerImpl& operator=(const ProjectorControllerImpl&) = delete;
  ~ProjectorControllerImpl() override;

  static ProjectorControllerImpl* Get();
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ProjectorController:
  void StartProjectorSession(const base::SafeBaseName& storage_dir) override;
  void SetClient(ProjectorClient* client) override;
  void OnSpeechRecognitionAvailabilityChanged() override;
  void OnTranscription(const media::SpeechRecognitionResult& result) override;
  void OnTranscriptionError() override;
  void OnSpeechRecognitionStopped(bool forced) override;
  NewScreencastPrecondition GetNewScreencastPrecondition() const override;

  // Create the screencast container directory. If there is an error, the
  // callback will be triggered with an empty FilePath.
  //
  // For now, Projector Screencasts are all uploaded to Drive. This method will
  // create the folder in DriveFS mounted path. Files saved in this path will
  // then be synced to Drive by DriveFS. DriveFS only supports primary account.
  void CreateScreencastContainerFolder(
      CreateScreencastContainerFolderCallback callback);

  // Notifies the ProjectorClient if the Projector SWA can trigger a
  // new Projector session. The preconditions are calculated in
  // `ProjectorControllerImpl::GetNewScreencastPrecondition`. The following are
  // preconditions that are checked:
  // 1. On device speech recognition availability changes.
  // 2. Screen recording state changed( whether an active recording is already
  // taking place or not).
  // 3. Whether DriveFS is mounted or not.
  void OnNewScreencastPreconditionChanged();

  void SetProjectorMetadataControllerForTest(
      std::unique_ptr<ProjectorMetadataController> metadata_controller);
  void SetOnPathDeletedCallbackForTest(OnPathDeletedCallback callback);
  void SetOnFileSavedCallbackForTest(OnFileSavedCallback callback);

  ProjectorUiController* ui_controller() { return ui_controller_.get(); }
  ProjectorSessionImpl* projector_session() { return projector_session_.get(); }

  // CrasAudioHandler::AudioObserver:
  void OnAudioNodesChanged() override;

  // CaptureModeObserver:
  void OnRecordingStarted(aura::Window* current_root) override;
  void OnRecordingEnded() override;
  void OnVideoFileFinalized(bool user_deleted_video_file,
                            const gfx::ImageSkia& thumbnail) override;
  void OnRecordedWindowChangingRoot(aura::Window* new_root) override;
  void OnRecordingStartAborted() override;

  base::OneShotTimer* get_timer_for_testing() {
    return &force_stop_recognition_timer_;
  }

 private:
  // Enum class representing the speech recognition status state.
  enum class SpeechRecognitionState {
    kRecognitionNotStarted = 0,
    kRecognitionStarted = 1,
    kRecognitionStopping = 2,
    kRecognitionError = 3,
  };

  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

  bool IsInputDeviceAvailable() const;

  // Starts or stops the speech recognition session.
  void StartSpeechRecognition();
  void MaybeStopSpeechRecognition();
  void ForceEndSpeechRecognition();

  // Called when the projector-initiated capture mode session initialization is
  // completed or returned to start the projector session with given
  // `storage_dir` if `success` is true.
  void OnSessionStartAttempted(const base::SafeBaseName& storage_dir,
                               bool success);

  // Triggered when finish creating the screencast container folder. This method
  // caches the the container folder path in `ProjectorSession` and triggers the
  // `CreateScreencastContainerFolderCallback' with the screencast file path
  // without file extension. This path will be used by screen capture to save
  // screencast media file after appending the media file extension.
  void OnContainerFolderCreated(
      const base::FilePath& path,
      CreateScreencastContainerFolderCallback callback,
      bool success);

  // Saves the screencast including metadata.
  void SaveScreencast();

  // Save the screencast thumbnail file.
  void SaveThumbnailFile(const gfx::ImageSkia& thumbnail);

  // Clean up the screencast container folder.
  void CleanupContainerFolder();

  // Wrap up recording by saving the metadata file and stop the projector
  // session. This is a no-op if DLP restriction check is not completed.
  // If speech recognition is not finished, this method will set a timer
  // for force end the speech recognition session.
  void MaybeWrapUpRecording();

  // Returns all file paths related to current recording. Paths are calculated
  // from the container folder.
  std::vector<base::FilePath> GetScreencastFilePaths() const;

  raw_ptr<ProjectorClient, DanglingUntriaged> client_ = nullptr;
  std::unique_ptr<ProjectorSessionImpl> projector_session_;
  std::unique_ptr<ProjectorUiController> ui_controller_;
  std::unique_ptr<ProjectorMetadataController> metadata_controller_;

  // Whether speech recognition is taking place or not.
  SpeechRecognitionState speech_recognition_state_ =
      SpeechRecognitionState::kRecognitionNotStarted;
  bool use_on_device_speech_recognition = false;

  // Whether DLP restriction check is completed.
  bool dlp_restriction_checked_completed_ = false;
  // Whether user deleted video file at DLP restriction check dialog.
  bool user_deleted_video_file_ = false;

  // Currently, these callbacks are used by unit tests to verify file saved and
  // directory deleted.
  OnPathDeletedCallback on_path_deleted_callback_;
  OnFileSavedCallback on_file_saved_callback_;

  // There is a delay on completing speech recognition session. We enforce a 90
  // second timeout from the recording stopped signal to force end the speech
  // recognition session.
  base::OneShotTimer force_stop_recognition_timer_;

  base::WeakPtrFactory<ProjectorControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
