// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"

#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals.mojom.h"
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals_ui.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals.mojom.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/notebooks/internals/webui/notebooks_internals.mojom.h"
#include "components/notebooks/internals/webui/notebooks_internals_ui.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

void PopulateChromeWebUIFrameBindersPartsAndroid(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<
      chrome_finds_internals::mojom::PageHandlerFactory,
      chrome_finds_internals::ChromeFindsInternalsUI>(map);
  RegisterWebUIControllerInterfaceBinder<feed_internals::mojom::PageHandler,
                                         FeedInternalsUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      notifications_internals::mojom::PageHandler, NotificationsInternalsUI>(
      map);
  RegisterWebUIControllerInterfaceBinder<
      notebooks_internals::mojom::PageHandlerFactory,
      notebooks::NotebooksInternalsUI>(map);
}

}  // namespace chrome::internal
