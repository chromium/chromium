// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/media_string_view.h"

#include <memory>
#include <string>

#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/media_session/public/mojom/media_session_service.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Typography.
constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr char kMiddleDotSeparator[] = " \u00B7 ";
constexpr char kPreceedingEighthNoteSymbol[] = "\u266A ";
constexpr int kDefaultFontSizeDip = 64;
constexpr int kMediaStringFontSizeDip = 16;

// Returns true if we should show media string for ambient mode on lock-screen
// based on user pref. We should keep the same user policy here as the
// lock-screen media controls to avoid exposing user data on lock-screen without
// consent.
bool ShouldShowOnLockScreen() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(pref);

  return pref->GetBoolean(prefs::kLockScreenMediaControlsEnabled);
}

}  // namespace

MediaStringView::MediaStringView() {
  SetID(AssistantViewID::kAmbientMediaStringView);
  InitLayout();
}

MediaStringView::~MediaStringView() = default;

const char* MediaStringView::GetClassName() const {
  return "MediaStringView";
}

void MediaStringView::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock) &&
      !ShouldShowOnLockScreen()) {
    return;
  }

  // Don't show the media string if session info is unavailable, or the active
  // session is marked as sensitive.
  if (!session_info || session_info->is_sensitive) {
    SetVisible(false);
    return;
  }

  bool is_paused = session_info->playback_state ==
                   media_session::mojom::MediaPlaybackState::kPaused;

  // Don't show the media string if paused.
  SetVisible(!is_paused);
}

void MediaStringView::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  media_session::MediaMetadata session_metadata =
      metadata.value_or(media_session::MediaMetadata());

  base::string16 media_string;
  if (!session_metadata.title.empty() && !session_metadata.artist.empty()) {
    media_string = session_metadata.title +
                   base::UTF8ToUTF16(kMiddleDotSeparator) +
                   session_metadata.artist;
  } else if (!session_metadata.title.empty()) {
    media_string = session_metadata.title;
  } else {
    media_string = session_metadata.artist;
  }

  // Formats the media string with a preceding music eighth note.
  SetText(base::UTF8ToUTF16(kPreceedingEighthNoteSymbol) + media_string);
}

void MediaStringView::InitLayout() {
  // This view will be drawn on its own layer instead of the layer of
  // |PhotoView| which has a solid black background.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Defines the appearance.
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColor(kTextColor);
  SetFontList(ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
      kMediaStringFontSizeDip - kDefaultFontSizeDip));
  SetShadows(ambient::util::GetTextShadowValues());

  BindMediaControllerObserver();
}

void MediaStringView::BindMediaControllerObserver() {
  media_session::mojom::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  // Service might be unavailable under some test environments.
  if (!service)
    return;

  // Binds to the MediaControllerManager and create a MediaController for the
  // current active media session so that we can observe it.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes.
  media_controller_remote_->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());
}

}  // namespace ash
