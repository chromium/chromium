// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_CONTROLLER_H_
#define ASH_MEDIA_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/media.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/account_id/interfaces/account_id.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace ash {

// Forwards notifications from the MediaController to observers.
class MediaCaptureObserver {
 public:
  // Called when media capture state has changed.
  virtual void OnMediaCaptureChanged(
      const base::flat_map<AccountId, mojom::MediaCaptureState>&
          capture_states) = 0;

 protected:
  virtual ~MediaCaptureObserver() {}
};

// Provides the MediaController interface to the outside world. This lets a
// consumer of ash provide a MediaClient, which we will dispatch to if one has
// been provided to us.
class ASH_EXPORT MediaController : public mojom::MediaController {
 public:
  // |connector| can be null in tests.
  explicit MediaController(service_manager::Connector* connector);
  ~MediaController() override;

  void BindRequest(mojom::MediaControllerRequest request);

  void AddObserver(MediaCaptureObserver* observer);
  void RemoveObserver(MediaCaptureObserver* observer);

  // mojom::MediaController:
  void SetClient(mojom::MediaClientAssociatedPtrInfo client) override;
  void NotifyCaptureState(
      const base::flat_map<AccountId, mojom::MediaCaptureState>& capture_states)
      override;

  // If media session accelerators are enabled then these methods will use the
  // media session service to control playback. Otherwise it will forward to
  // |client_|.
  void HandleMediaPlayPause();
  void HandleMediaNextTrack();
  void HandleMediaPrevTrack();

  // Methods that forward to |client_|.
  void RequestCaptureState();
  void SuspendMediaSessions();

 private:
  friend class MediaSessionAcceleratorTest;
  friend class MultiProfileMediaTrayItemTest;
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_NextTrack);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_PlayPause);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionAcceleratorTest,
                           MediaGlobalAccelerators_PrevTrack);

  void SetMediaSessionControllerForTest(
      media_session::mojom::MediaControllerPtr controller);

  void FlushForTesting();

  // Returns a pointer to the active media session controller.
  media_session::mojom::MediaController* GetMediaSessionController();

  void OnMediaSessionControllerError();

  // Mojo pointer to the active media session controller.
  media_session::mojom::MediaControllerPtr media_session_controller_ptr_;

  service_manager::Connector* const connector_;

  // Bindings for users of the mojo interface.
  mojo::BindingSet<mojom::MediaController> bindings_;

  mojom::MediaClientAssociatedPtr client_;

  base::ObserverList<MediaCaptureObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaController);
};

}  // namespace ash

#endif  // ASH_MEDIA_CONTROLLER_H_
