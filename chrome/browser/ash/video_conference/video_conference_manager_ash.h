// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_ASH_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ash/system/video_conference/video_conference_common.h"
#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace base {
class UnguessableToken;
}  // namespace base

class CaptureModeVideoConferenceBrowserTests;

namespace ash {

class VideoConferenceClientWrapper;
class VideoConferenceTrayController;
struct VideoConferenceMediaState;

// VideoConferenceManagerAsh is the central hub responsible for:
// 1. Connecting VC clients to System UI components.
// 2. Being the single source of truth for VC parameters like
//    background blur, portrait relighting, apps using audio/camera, etc.
//    and providing this information to the UI as needed.
class VideoConferenceManagerAsh
    : public VideoConferenceManagerBase,
      public crosapi::mojom::VideoConferenceManager {
 public:
  VideoConferenceManagerAsh();

  VideoConferenceManagerAsh(const VideoConferenceManagerAsh&) = delete;
  VideoConferenceManagerAsh& operator=(const VideoConferenceManagerAsh&) =
      delete;

  ~VideoConferenceManagerAsh() override;

  // VideoConferenceManagerBase overrides.
  void GetMediaApps(base::OnceCallback<void(MediaApps)>) override;
  void ReturnToApp(const base::UnguessableToken& id) override;
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled) override;
  void StopAllScreenShare() override;
  void CreateBackgroundImage() override;

  // Registers an ash-browser client. Non-mojo clients need to manually call
  // |UnregisterClient|, e.g. inside their destructor.
  void RegisterCppClient(crosapi::mojom::VideoConferenceManagerClient* client,
                         const base::UnguessableToken& client_id);

  // Binds a pending receiver connected to a lacros mojo client to the manager.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::VideoConferenceManager> receiver);

  // crosapi::mojom::VideoConferenceManager overrides
  void NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatusPtr status,
      NotifyMediaUsageUpdateCallback callback) override;
  void RegisterMojoClient(
      mojo::PendingRemote<crosapi::mojom::VideoConferenceManagerClient> client,
      const base::UnguessableToken& client_id,
      RegisterMojoClientCallback callback) override;
  void NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device,
      const std::u16string& app_name,
      NotifyDeviceUsedWhileDisabledCallback callback) override;
  void NotifyClientUpdate(
      crosapi::mojom::VideoConferenceClientUpdatePtr update) override;

  // Removes entry corresponding to |client_id| from
  // |client_id_to_wrapper_|. Called by the destructor of
  // cpp clients (ash browser, ARC++) and by the disconnect handler on
  // |receiver| when the lacros mojo client disconnects.
  void UnregisterClient(const base::UnguessableToken& client_id);

 protected:
  // Returns aggregated |VideoConferenceMediaState| from all clients. Allows
  // test code to get the manager's aggregated state.
  VideoConferenceMediaState GetAggregatedState();

  // Aggregates and sends updated state to VcUiController. Allows mock classes
  // extending |VideoConferenceManagerAsh| to make assertions on data sent to
  // VcUiController by overriding this method.
  virtual void SendUpdatedState();

  // Returns the `VideoConferenceTrayController`.
  VideoConferenceTrayController* GetTrayController();

 private:
  friend class VideoConferenceAshfeatureClientTest;
  friend class VideoConferenceAppServiceClientTest;
  friend class ::CaptureModeVideoConferenceBrowserTests;

  // A (client_id, client_wrapper) entry is inserted into this map
  // whenever a new client is registered on the manager and deleted
  // upon destruction of the client.
  std::map<base::UnguessableToken, VideoConferenceClientWrapper>
      client_id_to_wrapper_;
  mojo::ReceiverSet<crosapi::mojom::VideoConferenceManager> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_ASH_H_
