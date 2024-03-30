// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view_controller.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/media/media_color_theme.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/media/quick_settings_media_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "ui/views/view.h"

using global_media_controls::GlobalMediaControlsEntryPoint;

namespace ash {

QuickSettingsMediaViewController::QuickSettingsMediaViewController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller),
      media_item_manager_(global_media_controls::MediaItemManager::Create()) {
  media_session::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();

  // Return if this is called from tests and there is no media service.
  if (!service) {
    return;
  }

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;

  // Connect to receive audio focus events.
  service->BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());

  media_session_item_producer_ =
      std::make_unique<global_media_controls::MediaSessionItemProducer>(
          std::move(audio_focus_remote), std::move(controller_manager_remote),
          media_item_manager_.get(), /*source_id=*/std::nullopt);

  media_item_manager_->AddObserver(this);
  media_item_manager_->AddItemProducer(media_session_item_producer_.get());

  CHECK(MediaNotificationProvider::Get());
  MediaNotificationProvider::Get()->AddMediaItemManagerToCastService(
      media_item_manager_.get());
}

QuickSettingsMediaViewController::~QuickSettingsMediaViewController() {
  media_item_manager_->SetDialogDelegate(nullptr);
  media_item_manager_->RemoveObserver(this);
  if (MediaNotificationProvider::Get()) {
    MediaNotificationProvider::Get()->RemoveMediaItemManagerFromCastService(
        media_item_manager_.get());
  }
}

///////////////////////////////////////////////////////////////////////////////
// global_media_controls::MediaDialogDelegate implementations:

global_media_controls::MediaItemUI*
QuickSettingsMediaViewController::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  CHECK(media_view_);
  CHECK(MediaNotificationProvider::Get());

  auto media_item_ui = std::make_unique<global_media_controls::MediaItemUIView>(
      id, item, MediaNotificationProvider::Get()->BuildFooterView(id, item),
      MediaNotificationProvider::Get()->BuildDeviceSelectorView(
          id, item,
          global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray),
      /*notification_theme=*/std::nullopt, GetCrosMediaColorTheme(),
      global_media_controls::MediaDisplayPage::kQuickSettingsMediaView);
  auto* media_item_ui_ptr = media_item_ui.get();
  media_item_ui_observer_set_.Observe(id, media_item_ui_ptr);
  media_view_->ShowItem(id, std::move(media_item_ui));
  return media_item_ui_ptr;
}

void QuickSettingsMediaViewController::HideMediaItem(const std::string& id) {
  // This can be called during MediaNotificationItem destruction when the media
  // view is already removed.
  if (media_view_) {
    media_view_->HideItem(id);
  }
}

///////////////////////////////////////////////////////////////////////////////
// global_media_controls::MediaItemUIObserver implementations:

void QuickSettingsMediaViewController::OnMediaItemUIClicked(
    const std::string& id,
    bool activate_original_media) {
  tray_controller_->ShowMediaControlsDetailedView(
      GlobalMediaControlsEntryPoint::kQuickSettingsMiniPlayer);
}

void QuickSettingsMediaViewController::OnMediaItemUIShowDevices(
    const std::string& id) {
  tray_controller_->ShowMediaControlsDetailedView(
      GlobalMediaControlsEntryPoint::kQuickSettingsMiniPlayerCastButton, id);
}

///////////////////////////////////////////////////////////////////////////////
// QuickSettingsMediaViewController implementations:

std::unique_ptr<views::View> QuickSettingsMediaViewController::CreateView() {
  auto media_view = std::make_unique<QuickSettingsMediaView>(this);
  media_view_ = media_view.get();
  media_item_manager_->SetDialogDelegate(this);
  return std::move(media_view);
}

void QuickSettingsMediaViewController::SetShowMediaView(bool show_media_view) {
  tray_controller_->SetShowMediaView(show_media_view);
}

void QuickSettingsMediaViewController::UpdateMediaItemOrder() {
  media_view_->UpdateItemOrder(media_item_manager_->GetActiveItemIds());
}

int QuickSettingsMediaViewController::GetMediaViewHeight() {
  return media_view_->GetMediaViewHeight();
}

}  // namespace ash
