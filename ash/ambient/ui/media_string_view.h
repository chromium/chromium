// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_MEDIA_STRING_VIEW_H_
#define ASH_AMBIENT_UI_MEDIA_STRING_VIEW_H_

#include <memory>

#include "base/scoped_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

namespace {
class FadeoutLayerDelegate;
}

// Container for displaying ongoing media information, including the name of the
// media and the artist, formatted with a proceding music note symbol and a
// middle dot separator.
class MediaStringView : public views::View,
                        public views::ViewObserver,
                        public media_session::mojom::MediaControllerObserver,
                        public ui::ImplicitAnimationObserver {
 public:
  MediaStringView();
  MediaStringView(const MediaStringView&) = delete;
  MediaStringView& operator=(const MediaStringView&) = delete;
  ~MediaStringView() override;

  // views::View:
  const char* GetClassName() const override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  friend class AmbientAshTestBase;

  void InitLayout();

  void BindMediaControllerObserver();

  void UpdateMaskLayer();

  bool NeedToAnimate() const;

  // Get the transform of |media_text_| for scrolling animation.
  gfx::Transform GetMediaTextTransform(bool is_initial);
  void ScheduleScrolling(bool is_initial);
  void StartScrolling(bool is_initial);

  views::View* media_text_container_for_testing() {
    return media_text_container_;
  }

  views::Label* media_text_label_for_testing() { return media_text_; }

  // Music eighth note.
  views::Label* icon_ = nullptr;

  // Container of media info text.
  views::View* media_text_container_ = nullptr;

  // With an extra copy of media info text for scrolling animation.
  views::Label* media_text_ = nullptr;

  std::unique_ptr<FadeoutLayerDelegate> fadeout_layer_delegate_;

  // Used to receive updates to the active media controller.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_{this};

  ScopedObserver<views::View, views::ViewObserver> observed_view_{this};

  base::WeakPtrFactory<MediaStringView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_MEDIA_STRING_VIEW_H_
