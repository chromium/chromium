// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/media_app/mahi_media_app_client.h"

#include "base/check_deref.h"
#include "base/i18n/break_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
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
    const std::string& file_name)
    : client_id_(base::UnguessableToken::Create()),
      media_app_pdf_file_(std::move(page)),
      file_name_(file_name) {
  // Registers self to `MahiMediaAppContentManager` as a client.
  chromeos::MahiMediaAppContentManager::Get()->AddClient(client_id_, this);
}

MahiMediaAppClient::~MahiMediaAppClient() {
  // Manually calling `RemoveClient()` when disconnecting.
  chromeos::MahiMediaAppContentManager::Get()->RemoveClient(client_id_);
}

void MahiMediaAppClient::OnPdfContextMenuShow(const ::gfx::RectF& anchor) {
  chromeos::MahiMediaAppEventsProxy::Get()->OnPdfContextMenuShown(
      client_id_, ::gfx::ToEnclosingRect(anchor));
}

void MahiMediaAppClient::OnPdfContextMenuHide() {
  chromeos::MahiMediaAppEventsProxy::Get()->OnPdfContextMenuHide();
}

void MahiMediaAppClient::OnPdfGetFocus() {
  chromeos::MahiMediaAppEventsProxy::Get()->OnPdfGetFocus(client_id_);
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

}  // namespace ash
