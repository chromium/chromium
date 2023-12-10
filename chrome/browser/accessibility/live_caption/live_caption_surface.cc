// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_surface.h"

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"

namespace captions {

WEB_CONTENTS_USER_DATA_KEY_IMPL(LiveCaptionSurface);

// static
LiveCaptionSurface* LiveCaptionSurface::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  // No-op if a surface is already attached.
  CreateForWebContents(web_contents);

  return FromWebContents(web_contents);
}

LiveCaptionSurface::LiveCaptionSurface(content::WebContents* web_contents)
    : content::WebContentsUserData<LiveCaptionSurface>(*web_contents),
      content::WebContentsObserver(web_contents),
      session_id_(base::UnguessableToken::Create()) {}

LiveCaptionSurface::~LiveCaptionSurface() = default;

void LiveCaptionSurface::BindToSurfaceClient(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSurface> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSurfaceClient> remote) {
  receivers_.Add(this, std::move(receiver));
  clients_.Add(std::move(remote));
}

void LiveCaptionSurface::Activate() {
  if (!web_contents()) {
    return;
  }

  // Activate the web contents and the browser window that the web contents is
  // in. Order matters: web contents needs to be active in order for the widget
  // getter to work.
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser) {
    return;
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  DCHECK(tab_strip_model);

  const int index = tab_strip_model->GetIndexOfWebContents(web_contents());
  if (index == TabStripModel::kNoTab) {
    return;
  }

  tab_strip_model->ActivateTabAt(index);
  views::Widget* context_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents()->GetNativeView());
  if (context_widget) {
    context_widget->Activate();
  }
}

void LiveCaptionSurface::GetBounds(GetBoundsCallback callback) {
  if (!web_contents()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  views::Widget* context_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents()->GetNativeView());
  if (!context_widget) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(context_widget->GetClientAreaBoundsInScreen());
}

void LiveCaptionSurface::MediaEffectivelyFullscreenChanged(
    bool /*is_fullscreen*/) {
  for (const auto& client : clients_) {
    client->OnFullscreenToggled();
  }
}

void LiveCaptionSurface::PrimaryPageChanged(content::Page& /*page*/) {
  for (const auto& client : clients_) {
    client->OnSessionEnded();
  }
}

base::UnguessableToken LiveCaptionSurface::session_id() const {
  return session_id_;
}

}  // namespace captions
