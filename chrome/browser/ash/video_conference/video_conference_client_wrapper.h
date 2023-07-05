// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_WRAPPER_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_WRAPPER_H_

#include <cstdint>

#include "ash/system/video_conference/video_conference_common.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class VideoConferenceManagerAsh;

// |VideoConferenceClientWrapper| abstracts away the details of communicating
// with VC Clients. This class implements all methods defined on
// crosapi::mojom::VideoConferenceClient and, depending on whether it's
// wrapping a mojo client or a non-mojo client, calls methods on a mojo
// remote connected to a client or directly on a client instance.
class VideoConferenceClientWrapper {
 public:
  VideoConferenceClientWrapper(
      mojo::PendingRemote<crosapi::mojom::VideoConferenceManagerClient> client,
      const base::UnguessableToken& client_id,
      VideoConferenceManagerAsh* vc_manager);

  VideoConferenceClientWrapper(
      crosapi::mojom::VideoConferenceManagerClient* client,
      VideoConferenceManagerAsh* vc_manager);

  ~VideoConferenceClientWrapper();

  // API mirroring the one in `crosapi::mojom::VideoConferenceManagerClient`
  void GetMediaApps(
      crosapi::mojom::VideoConferenceManagerClient::GetMediaAppsCallback
          callback);
  void ReturnToApp(
      const base::UnguessableToken& id,
      crosapi::mojom::VideoConferenceManagerClient::ReturnToAppCallback
          callback);
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      crosapi::mojom::VideoConferenceManagerClient::
          SetSystemMediaDeviceStatusCallback callback);

  void StopAllScreenShare();

  VideoConferenceMediaState& state();

 private:
  raw_ptr<VideoConferenceManagerAsh> vc_manager_;
  VideoConferenceMediaState state_;
  // Exactly one of the following is non-null.
  mojo::Remote<crosapi::mojom::VideoConferenceManagerClient> mojo_client_;
  raw_ptr<crosapi::mojom::VideoConferenceManagerClient> cpp_client_{nullptr};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_WRAPPER_H_
