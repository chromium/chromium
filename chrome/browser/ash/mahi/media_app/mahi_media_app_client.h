// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CLIENT_H_
#define CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CLIENT_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

class MahiMediaAppClient
    : public media_app_ui::mojom::MahiUntrustedPageHandler {
 public:
  using GetContentCallback =
      base::OnceCallback<void(crosapi::mojom::MahiPageContentPtr)>;

  MahiMediaAppClient(
      mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
      const std::string& file_name);
  MahiMediaAppClient(const MahiMediaAppClient&) = delete;
  MahiMediaAppClient& operator=(const MahiMediaAppClient&) = delete;
  ~MahiMediaAppClient() override;

  // media_app_ui::mojom::MahiUntrustedPageHandler:
  void OnPdfContextMenuShow(const ::gfx::RectF& anchor) override;
  void OnPdfContextMenuHide() override;

  // Notifies Mahi that a media app pdf window is focused.
  // This is driven by native window focus events, instead of media app.
  void OnPdfGetFocus();

  // Exposes media_app_ui::mojom::MahiUntrustedPage interfaces:
  void GetPdfContent(GetContentCallback callback);
  void HideMediaAppContextMenu();

  const std::string& file_name() const { return file_name_; }

 private:
  // Unique id associated with this client. It is used by the
  // `MahiBrowserDelegate` to identify clients.
  const base::UnguessableToken client_id_;

  mojo::Remote<media_app_ui::mojom::MahiUntrustedPage> media_app_pdf_file_;
  std::string file_name_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CLIENT_H_
