// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_

#include <set>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace base {
class Value;
}  // namespace base

namespace ash {

class AnnotatorMessageHandler;
struct AnnotatorTool;
struct ProjectorScreencastVideo;
struct NewScreencastPrecondition;

struct PendingScreencast {
  PendingScreencast();
  explicit PendingScreencast(const base::FilePath& container_dir);
  PendingScreencast(const base::FilePath& container_dir,
                    const std::string& name,
                    int64_t total_size_in_bytes,
                    int64_t bytes_transferred);
  PendingScreencast(const PendingScreencast&);
  PendingScreencast& operator=(const PendingScreencast&);
  ~PendingScreencast();

  base::Value ToValue() const;
  bool operator==(const PendingScreencast& rhs) const;

  // The container path of the screencast. It's a relative path of drive, looks
  // like "/root/projector_data/abc".
  base::FilePath container_dir;
  // The display name of screencast. If `container_dir` is
  // "/root/projector_data/abc", the `name` is "abc".
  std::string name;
  // The total size of a screencast in bytes, including the media file and the
  // metadata file under `container_dir`.
  int64_t total_size_in_bytes = 0;
  // The bytes have been transferred to drive.
  int64_t bytes_transferred = 0;

  // The media file created time.
  base::Time created_time;

  // True after observing drivefs::mojom::DriveError::kCantUploadStorageFull for
  // the first time. Screencast's status might go through error -> uploading ->
  // error -> ... -> uploaded, but it will display the error state until
  // successfully uploaded to avoid over commnucation with user.
  bool upload_failed = false;
};

struct PendingScreencastSetComparator {
  bool operator()(const PendingScreencast& a, const PendingScreencast& b) const;
};

// The set to store pending screencasts.
using PendingScreencastSet =
    std::set<PendingScreencast, PendingScreencastSetComparator>;

// Defines interface to access Browser side functionalities for the
// ProjectorApp.
class ProjectorAppClient {
 public:
  // The callback used by the GetVideo() API.
  using OnGetVideoCallback =
      base::OnceCallback<void(std::unique_ptr<ProjectorScreencastVideo> video,
                              const std::string& error_message)>;

  // Interface for observing events on the ProjectorAppClient.
  class Observer : public base::CheckedObserver {
   public:
    // Used to notify the Projector SWA app on whether it can start a new
    // screencast session.
    virtual void OnNewScreencastPreconditionChanged(
        const NewScreencastPrecondition& precondition) = 0;

    // Observes the pending screencast state change events.
    virtual void OnScreencastsPendingStatusChanged(
        const PendingScreencastSet& pending_screencast) = 0;

    // Notifies the observer the SODA binary and language pack download and
    // installation progress.
    virtual void OnSodaProgress(int combined_progress) = 0;

    // Notifies the observer that an error occurred during installation.
    virtual void OnSodaError() = 0;

    // Notifies the observer that installation of SODA binary and at least one
    // language pack has finished.
    virtual void OnSodaInstalled() = 0;
  };

  ProjectorAppClient(const ProjectorAppClient&) = delete;
  ProjectorAppClient& operator=(const ProjectorAppClient&) = delete;

  static ProjectorAppClient* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the IdentityManager for the active user profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the URLLoaderFactory for the primary user profile.
  virtual network::mojom::URLLoaderFactory* GetUrlLoaderFactory() = 0;

  // Used to notify the Projector SWA app on whether it can start a new
  // screencast session.
  virtual void OnNewScreencastPreconditionChanged(
      const NewScreencastPrecondition& precondition) = 0;

  // Returns pending screencast uploaded by primary user.
  virtual const PendingScreencastSet& GetPendingScreencasts() const = 0;

  // Checks if device is eligible to trigger SODA installer.
  virtual bool ShouldDownloadSoda() const = 0;

  // Triggers the installation of SODA (Speech On-Device API) binary and the
  // corresponding language pack for projector.
  virtual void InstallSoda() = 0;

  // Notifies the client the SODA binary and language pack download and
  // installation progress.
  virtual void OnSodaInstallProgress(int combined_progress) = 0;

  // Notifies the client that an error occurred during installation.
  virtual void OnSodaInstallError() = 0;

  // Notifies the client that installation of SODA binary and at least one
  // language pack has finished.
  virtual void OnSodaInstalled() = 0;

  // Triggers the opening of the Chrome feedback dialog.
  virtual void OpenFeedbackDialog() const = 0;

  // Launches the given DriveFS video file with `video_file_id` into the
  // Projector app. The `resource_key` is an additional security token needed to
  // gain access to link-shared files. Since the `resource_key` is currently
  // only used by Googlers, the `resource_key` might be empty.
  virtual void GetVideo(const std::string& video_file_id,
                        const std::string& resource_key,
                        OnGetVideoCallback callback) const = 0;

  // Registers the AnnotatorMessageHandler that is owned by the WebUI that
  // contains the Projector annotator.
  virtual void SetAnnotatorMessageHandler(AnnotatorMessageHandler* handler) = 0;

  // Resets the stored AnnotatorMessageHandler if it matches the one that is
  // passed in.
  virtual void ResetAnnotatorMessageHandler(
      AnnotatorMessageHandler* handler) = 0;

  // Sets the tool inside the annotator WebUI.
  virtual void SetTool(const AnnotatorTool& tool) = 0;

  // Clears the contents of the annotator canvas.
  virtual void Clear() = 0;

  // Called with true by the initiation and false by the destruction of
  // projector trusted UI .
  virtual void NotifyAppUIActive(bool active) = 0;

  // Toggles to suppress/resume the system notification for `screencast_paths`.
  virtual void ToggleFileSyncingNotificationForPaths(
      const std::vector<base::FilePath>& screencast_paths,
      bool suppress) = 0;

 protected:
  ProjectorAppClient();
  virtual ~ProjectorAppClient();
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
