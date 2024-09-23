// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CLIENT_H_
#define CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CLIENT_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

// A full-duplex Mojo connection between Mahi and media app.
// Its lifetime is bound to the Mojo connection and inherently the PDF file
// opened in the media app, i.e. it gets destructed when the media app window
// opens a new file, or the media app window closes.
class MahiMediaAppClient : public media_app_ui::mojom::MahiUntrustedPageHandler,
                           public aura::client::FocusChangeObserver,
                           public aura::WindowObserver {
 public:
  using GetContentCallback =
      base::OnceCallback<void(crosapi::mojom::MahiPageContentPtr)>;

  MahiMediaAppClient(
      mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
      const std::string& file_name,
      aura::Window* media_app_window);
  MahiMediaAppClient(const MahiMediaAppClient&) = delete;
  MahiMediaAppClient& operator=(const MahiMediaAppClient&) = delete;
  ~MahiMediaAppClient() override;

  // media_app_ui::mojom::MahiUntrustedPageHandler:
  void OnPdfLoaded() override;
  void OnPdfFileNameUpdated(const std::string& new_name) override;
  void OnPdfContextMenuShow(const ::gfx::RectF& anchor) override;
  void OnPdfContextMenuHide() override;

  // Exposes media_app_ui::mojom::MahiUntrustedPage interfaces:
  void GetPdfContent(GetContentCallback callback);
  void HideMediaAppContextMenu();

  // aura::client::FocusChangeObserver:
  // Compares `media_app_window_` against `gained_focus` to deduce whether it's
  // the focused window, and notifies Mahi if so.
  // This is driven by native window focus events, instead of media app.
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // aura::WindowObserver:
  // Called when window bounds have changed from moving the window or
  // resizing. Useful for hiding any opened PDF context menus.
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // When the associated media app closes, resets `media_app_window_` to avoid
  // dangling raw_ptr.
  void OnWindowDestroying(aura::Window* window) override;

  const std::string& file_name() const { return file_name_; }
  aura::Window* media_app_window() const { return media_app_window_; }

 private:
  // Unique id associated with this client. It is used by the
  // `MahiBrowserDelegate` to identify clients.
  const base::UnguessableToken client_id_;

  mojo::Remote<media_app_ui::mojom::MahiUntrustedPage> media_app_pdf_file_;
  std::string file_name_;
  // Not owned. The window this client is associated with, whose address is used
  // in checking focus status.
  raw_ptr<aura::Window> media_app_window_;
  base::ScopedObservation<aura::client::FocusClient,
                          aura::client::FocusChangeObserver>
      focus_observation_{this};
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CLIENT_H_
