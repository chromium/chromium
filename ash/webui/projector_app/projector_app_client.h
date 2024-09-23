// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_

#include <set>

#include "ash/webui/projector_app/pending_screencast.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/values.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

struct NewScreencastPrecondition;

// Defines interface to access Browser side functionalities for the
// ProjectorApp.
class ProjectorAppClient {
 public:
  // The callback used by the GetVideo() API.
  using OnGetVideoCallback =
      base::OnceCallback<void(projector::mojom::GetVideoResultPtr result)>;

  // Interface for observing events on the ProjectorAppClient.
  class Observer : public base::CheckedObserver {
   public:
    // Used to notify the Projector SWA app on whether it can start a new
    // screencast session.
    virtual void OnNewScreencastPreconditionChanged(
        const NewScreencastPrecondition& precondition) = 0;

    // Observes the pending screencast state change events.
    virtual void OnScreencastsPendingStatusChanged(
        const PendingScreencastContainerSet& pending_screencast_containers) = 0;

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
  virtual const PendingScreencastContainerSet& GetPendingScreencasts()
      const = 0;

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
                        const std::optional<std::string>& resource_key,
                        OnGetVideoCallback callback) const = 0;

  // Called with true by the initiation and false by the destruction of
  // projector trusted UI .
  virtual void NotifyAppUIActive(bool active) = 0;

  // Toggles to suppress/resume the system notification for `screencast_paths`.
  virtual void ToggleFileSyncingNotificationForPaths(
      const std::vector<base::FilePath>& screencast_paths,
      bool suppress) = 0;

  // Triggers reauth dialog for the given `email`.
  virtual void HandleAccountReauth(const std::string& email) = 0;

 protected:
  ProjectorAppClient();
  virtual ~ProjectorAppClient();
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
