// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/media_app/mahi_media_app_client.h"

#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/i18n/break_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "ui/aura/client/focus_client.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {
// We truncate PDF files that have thousands of pages and tremendous amount of
// text with this limit, to save timecost of GetPdfContent, and to respect the
// capacity of the server side model.
constexpr int32_t kContentByteSizeLimit = 5 * 1000 * 1000;
constexpr int32_t kContentWordCountThreshold = 50;

using ::base::i18n::BreakIterator;

// Checks the content word count meets the `threshold`.
bool ContentsWordCountSatisfied(std::u16string_view contents,
                                int32_t threshold) {
  int32_t word_count = 0;
  BreakIterator break_iter(contents, BreakIterator::BREAK_WORD);
  if (!break_iter.Init()) {
    return false;
  }

  while (break_iter.Advance()) {
    if (break_iter.IsWord()) {
      ++word_count;
      if (word_count >= threshold) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace

MahiMediaAppClient::MahiMediaAppClient(
    mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
    const std::string& file_name,
    aura::Window* media_app_window)
    : client_id_(base::UnguessableToken::Create()),
      media_app_pdf_file_(std::move(page)),
      file_name_(file_name),
      media_app_window_(media_app_window) {
  if (!ash::Shell::HasInstance()) {
    return;
  }
  CHECK(media_app_window_);

  // Registers self to `MahiMediaAppContentManager` as a client.
  chromeos::MahiMediaAppContentManager::Get()->AddClient(client_id_, this);

  // Starts to observe media_app_window_ events.
  window_observation_.Observe(media_app_window_);
}

MahiMediaAppClient::~MahiMediaAppClient() {
  // If `media_app_window_` is null, it means the media app window is closed and
  // `OnWindowDestroying` is already called, the following part is done at that
  // time.
  if (media_app_window_ == nullptr) {
    return;
  }
  // Broadcasts the PDF closed event.
  chromeos::MahiMediaAppEventsProxy::Get()->OnPdfClosed(client_id_);

  // Manually calls `RemoveClient()` when disconnecting.
  chromeos::MahiMediaAppContentManager::Get()->RemoveClient(client_id_);
}

void MahiMediaAppClient::OnPdfLoaded() {
  if (!ash::Shell::HasInstance()) {
    return;
  }

  // On PDF loaded, the client starts to observe focus events and notify Mahi
  // system when the media app window has focus.
  focus_observation_.Reset();
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_observation_.Observe(focus_client);

  // Checks the current focused window.
  OnWindowFocused(focus_client->GetFocusedWindow(), nullptr);
}

void MahiMediaAppClient::OnPdfFileNameUpdated(const std::string& new_name) {
  if (file_name_ == new_name) {
    return;
  }
  file_name_ = new_name;
  CHECK(focus_observation_.IsObserving());

  // Notifies this change if the media app window has focus.
  OnWindowFocused(focus_observation_.GetSource()->GetFocusedWindow(), nullptr);
}

void MahiMediaAppClient::OnPdfContextMenuShow(const ::gfx::RectF& anchor) {
  chromeos::MahiMediaAppEventsProxy::Get()->OnPdfContextMenuShown(
      client_id_, ::gfx::ToEnclosingRect(anchor));
}

void MahiMediaAppClient::OnPdfContextMenuHide() {
  chromeos::MahiMediaAppEventsProxy::Get()->OnPdfContextMenuHide();
}

void MahiMediaAppClient::GetPdfContent(GetContentCallback callback) {
  media_app_pdf_file_->GetPdfContent(
      kContentByteSizeLimit,
      base::BindOnce(
          [](GetContentCallback callback, base::UnguessableToken client_id,
             const std::optional<std::string>& content) {
            if (!content.has_value()) {
              // TODO(b/335741382): UMA metric for this case.
              std::move(callback).Run(nullptr);
              return;
            }

            const std::u16string u16content =
                base::UTF8ToUTF16(content.value());
            if (!ContentsWordCountSatisfied(u16content,
                                            kContentWordCountThreshold)) {
              // TODO(b/335741382): UMA metric for this case.
              std::move(callback).Run(nullptr);
              return;
            }

            std::move(callback).Run(crosapi::mojom::MahiPageContent::New(
                client_id,
                /*page_id=*/client_id, std::move(u16content)));
          },
          std::move(callback), client_id_));
}

void MahiMediaAppClient::HideMediaAppContextMenu() {
  media_app_pdf_file_->HidePdfContextMenu();
}

void MahiMediaAppClient::OnWindowFocused(aura::Window* gained_focus,
                                         aura::Window* lost_focus) {
  if (gained_focus == nullptr || gained_focus == lost_focus) {
    return;
  }

  if (gained_focus == media_app_window_ ||
      gained_focus->GetToplevelWindow() == media_app_window_) {
    // Observed media app window get focus.
    chromeos::MahiMediaAppEventsProxy::Get()->OnPdfGetFocus(client_id_);
  }
}

void MahiMediaAppClient::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (window == media_app_window_) {
    // Any changes to the window bounds (from moving the window or resizing)
    // might affect the context menu position. Instead of updating the Mahi card
    // to follow the context menu, make the Media app hide the context menu.
    media_app_pdf_file_->HidePdfContextMenu();
  }
}

void MahiMediaAppClient::OnWindowDestroying(aura::Window* window) {
  if (window == media_app_window_) {
    // Broadcasts the PDF closed event.
    chromeos::MahiMediaAppEventsProxy::Get()->OnPdfClosed(client_id_);
    // Manually calls `RemoveClient()` when window destories.
    chromeos::MahiMediaAppContentManager::Get()->RemoveClient(client_id_);
    media_app_window_ = nullptr;
    window_observation_.Reset();
  }
}

}  // namespace ash
