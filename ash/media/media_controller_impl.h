// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_CONTROLLER_IMPL_H_
#define ASH_MEDIA_MEDIA_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/media_controller.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

class PrefRegistrySimple;

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace ash {

class MediaClient;

// Forwards notifications from the MediaController to observers.
class MediaCaptureObserver {
 public:
  // Called when media capture state has changed.
  virtual void OnMediaCaptureChanged(
      const base::flat_map<AccountId, MediaCaptureState>& capture_states) = 0;

 protected:
  virtual ~MediaCaptureObserver() {}
};

// Provides the MediaController interface to the outside world. This lets a
// consumer of ash provide a MediaClient, which we will dispatch to if one has
// been provided to us.
class ASH_EXPORT MediaControllerImpl
    : public MediaController,
      public media_session::mojom::MediaControllerObserver {
 public:
  // |connector| can be null in tests.
  explicit MediaControllerImpl(service_manager::Connector* connector);
  ~MediaControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Determine if lock screen media keys are enabled.
  bool AreLockScreenMediaKeysEnabled() const;
  void SetMediaControlsDismissed(bool media_controls_dismissed);

  void AddObserver(MediaCaptureObserver* observer);
  void RemoveObserver(MediaCaptureObserver* observer);

  // MediaController:
  void SetClient(MediaClient* client) override;
  void SetForceMediaClientKeyHandling(bool enabled) override;
  void NotifyCaptureState(const base::flat_map<AccountId, MediaCaptureState>&
                              capture_states) override;

  // If media session accelerators are enabled then these methods will use the
  // media session service to control playback. Otherwise it will forward to
  // |client_|.
  void HandleMediaPlayPause();
  void HandleMediaNextTrack();
  void HandleMediaPrevTrack();

  // Methods that forward to |client_|.
  void RequestCaptureState();
  void SuspendMediaSessions();

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

 private:
  friend class MediaControllerTest;
  friend class MediaSessionAcceleratorTest;
  friend class MultiProfileMediaTrayItemTest;
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_NextTrack);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_Play);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_Pause);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_PrevTrack);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_UpdateAction_Disable);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_UpdateAction_Enable);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_UpdateForceKeyHandling);

  void SetMediaSessionControllerForTest(
      mojo::Remote<media_session::mojom::MediaController> controller);

  void FlushForTesting();

  // Returns a pointer to the active media session controller.
  media_session::mojom::MediaController* GetMediaSessionController();

  void OnMediaSessionControllerError();

  void BindMediaControllerObserver();

  // Returns true if we should use the media session service for key handling.
  bool ShouldUseMediaSession();

  void ResetForceMediaClientKeyHandling();

  // Whether the active media session currently supports any action that has a
  // media key.
  bool supported_media_session_action_ = false;

  // The info about the current media session. It will be null if there is not
  // a current session.
  media_session::mojom::MediaSessionInfoPtr media_session_info_;

  // If true then the media keys should be forwarded to the client instead of
  // being handled in ash.
  bool force_media_client_key_handling_ = false;

  // Whether the lock screen media controls are dismissed.
  bool media_controls_dismissed_ = false;

  // Mojo pointer to the active media session controller.
  mojo::Remote<media_session::mojom::MediaController>
      media_session_controller_remote_;

  service_manager::Connector* const connector_;

  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  MediaClient* client_ = nullptr;

  base::ObserverList<MediaCaptureObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaControllerImpl);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_CONTROLLER_IMPL_H_
