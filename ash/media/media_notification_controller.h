// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "ui/message_center/message_center.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace ash {

// MediaNotificationController will show/hide a media notification when a media
// session is active. This notification will show metadata and playback
// controls.
class ASH_EXPORT MediaNotificationController
    : public media_session::mojom::AudioFocusObserver {
 public:
  explicit MediaNotificationController(service_manager::Connector* connector);
  ~MediaNotificationController() override;

  // AudioFocusObserver implementation.
  void OnFocusGained(media_session::mojom::MediaSessionInfoPtr media_session,
                     media_session::mojom::AudioFocusType type) override;
  void OnFocusLost(
      media_session::mojom::MediaSessionInfoPtr media_session) override;

 private:
  void OnNotificationClicked(base::Optional<int> button_id);

  mojo::Binding<media_session::mojom::AudioFocusObserver> binding_{this};

  base::WeakPtrFactory<MediaNotificationController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationController);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
