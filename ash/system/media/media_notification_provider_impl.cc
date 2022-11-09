// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_notification_provider_impl.h"

#include "ash/system/media/media_notification_provider.h"
#include "ash/system/media/media_notification_provider_observer.h"
#include "base/metrics/histogram_functions.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "ui/views/view.h"

namespace ash {

MediaNotificationProviderImpl::MediaNotificationProviderImpl(
    media_session::MediaSessionService* service)
    : item_manager_(global_media_controls::MediaItemManager::Create()) {
  DCHECK_EQ(nullptr, MediaNotificationProvider::Get());
  MediaNotificationProvider::Set(this);

  item_manager_->AddObserver(this);

  if (!service)
    return;

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
          item_manager_.get(), /*source_id=*/absl::nullopt);

  item_manager_->AddItemProducer(media_session_item_producer_.get());
}

MediaNotificationProviderImpl::~MediaNotificationProviderImpl() {
  DCHECK_EQ(this, MediaNotificationProvider::Get());
  MediaNotificationProvider::Set(nullptr);

  item_manager_->RemoveObserver(this);
}

void MediaNotificationProviderImpl::AddObserver(
    ash::MediaNotificationProviderObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationProviderImpl::RemoveObserver(
    ash::MediaNotificationProviderObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool MediaNotificationProviderImpl::HasActiveNotifications() {
  if (!item_manager_)
    return false;
  return item_manager_->HasActiveItems();
}

bool MediaNotificationProviderImpl::HasFrozenNotifications() {
  if (!item_manager_)
    return false;
  return item_manager_->HasFrozenItems();
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetMediaNotificationListView(
    int separator_thickness) {
  DCHECK(item_manager_);
  DCHECK(color_theme_);
  auto notification_list_view =
      std::make_unique<global_media_controls::MediaItemUIListView>(
          global_media_controls::MediaItemUIListView::SeparatorStyle(
              color_theme_->separator_color, separator_thickness));
  active_session_view_ = notification_list_view->GetWeakPtr();
  item_manager_->SetDialogDelegate(this);
  base::UmaHistogramEnumeration(
      "Media.GlobalMediaControls.EntryPoint",
      global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray);
  return notification_list_view;
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetActiveMediaNotificationView() {
  return std::make_unique<views::View>();
}

void MediaNotificationProviderImpl::OnBubbleClosing() {
  item_manager_->SetDialogDelegate(nullptr);
}

void MediaNotificationProviderImpl::SetColorTheme(
    const media_message_center::NotificationTheme& color_theme) {
  color_theme_ = color_theme;
}

global_media_controls::MediaItemUI*
MediaNotificationProviderImpl::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (!active_session_view_)
    return nullptr;

  auto item_ui = std::make_unique<global_media_controls::MediaItemUIView>(
      id, item, /*footer_view=*/nullptr, /*device_selector_view=*/nullptr,
      color_theme_);
  auto* item_ui_ptr = item_ui.get();
  item_ui_observer_set_.Observe(id, item_ui_ptr);

  active_session_view_->ShowItem(id, std::move(item_ui));
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();

  return item_ui_ptr;
}

void MediaNotificationProviderImpl::HideMediaItem(const std::string& id) {
  if (!active_session_view_)
    return;

  active_session_view_->HideItem(id);
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();
}

void MediaNotificationProviderImpl::OnItemListChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationProviderImpl::OnMediaItemUISizeChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();
}

void MediaNotificationProviderImpl::OnMediaItemUIDestroyed(
    const std::string& id) {
  item_ui_observer_set_.StopObserving(id);
}

}  // namespace ash
