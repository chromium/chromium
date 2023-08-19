// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_CLIENT_H_

#include <map>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "content/public/browser/web_contents.h"
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace video_conference {

class VideoConferenceMediaListener;
class VideoConferenceWebApp;
struct VideoConferencePermissions;

// VideoConferenceManagerClientImpl is the client class for CrOS
// videoconferencing. It is shared code between lacros-chrome and ash-chrome.
// The client is responsible for two things:
//    1. Tracking VC apps on the browser, recording changes in their permissions
//       and capturing statuses and notifying `VideoConferenceManagerAsh`.
//    2. Allowing the manager to query the client and perform actions by
//       implementing the VideoConferenceManagerClient crosapi interface.
class VideoConferenceManagerClientImpl
    : public crosapi::mojom::VideoConferenceManagerClient {
 public:
  VideoConferenceManagerClientImpl();

  VideoConferenceManagerClientImpl(const VideoConferenceManagerClientImpl&) =
      delete;
  VideoConferenceManagerClientImpl& operator=(
      const VideoConferenceManagerClientImpl&) = delete;

  ~VideoConferenceManagerClientImpl() override;

  // Removes the `VideoConferenceWebApp` associated with the `id` from the
  // client.
  void RemoveMediaApp(const base::UnguessableToken& id);

  // Creates a `VideoConferenceWebApp` for this `web_contents`, registers it on
  // the client and returns a pointer to it.
  VideoConferenceWebApp* CreateVideoConferenceWebApp(
      content::WebContents* web_contents);

  // Calculates a new `crosapi::mojom::VideoConferenceMediaUsageStatus` from all
  // current VC apps and notifies the manager if a field has changed.
  void HandleMediaUsageUpdate();

  // Notifies VCManager of media device usage while the device is system
  // disabled.
  void HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device,
      const std::u16string& app_name);

  // crosapi::mojom::VideoConferenceManagerClient overrides
  void GetMediaApps(GetMediaAppsCallback callback) override;
  void ReturnToApp(const base::UnguessableToken& id,
                   ReturnToAppCallback callback) override;
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override;
  void StopAllScreenShare() override;

 protected:
  // Sends VcManager the updated `VideoConferenceMediaUsageStatus`. Can be
  // overridden by test clients.
  virtual void NotifyManager(
      crosapi::mojom::VideoConferenceMediaUsageStatusPtr status);

 private:
  friend class FakeVideoConferenceManagerClient;

  // Returns the aggregated camera and microphone permissions granted status
  // from all VC apps on the client.
  VideoConferencePermissions GetAggregatedPermissions();

  // Sends a new client update to the VC Manager. Uses mojo for lacros-chrome
  // clients.
  void SendClientUpdate(crosapi::mojom::VideoConferenceClientUpdatePtr update);

  // Unique id associated with this client. It is used by the VcManager to
  // identify clients.
  const base::UnguessableToken client_id_;

  // Previous status
  crosapi::mojom::VideoConferenceMediaUsageStatusPtr status_;

  std::unique_ptr<VideoConferenceMediaListener> media_listener_;

  // Only `WebContents` associated with `VideoConferenceWebApp`s are stored in
  // this map. Entries are only added to this map when `VideoConferenceWebApp`s
  // are created and removed when:
  //    1. The `WebContents` is destroyed.
  //    2. A `PrimaryPageChanged` event occurs for that webcontents.
  std::map<base::UnguessableToken, raw_ptr<content::WebContents>>
      id_to_webcontents_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  mojo::Remote<crosapi::mojom::VideoConferenceManager> remote_;
  mojo::Receiver<crosapi::mojom::VideoConferenceManagerClient> receiver_{this};
#endif

  // Any `VideoConferenceWebApp` created by the client gets passed a callback
  // bound to `RemoveMediaApp`. In order to guard against situations where that
  // callback is run after the client has been destroyed, we pass a weak ptr
  // instead. This if fine as the only purpose of `RemoveMediaApp` is to remove
  // the entry corresponding to the passed `id` from `id_to_webcontents_`.
  base::WeakPtrFactory<VideoConferenceManagerClientImpl> weak_ptr_factory_{
      this};
};

}  // namespace video_conference

#endif  // CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_CLIENT_H_
