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
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_WEBUI_HISTORY)
// These headers and their corresponding Mojo targets are only added as
// dependencies of //chrome/browser:core when `enable_webui_history` is true
// (e.g. on desktop Android). Because this file is compiled for all Android
// targets, `gn check` must ignore them when the buildflag is disabled.
#include "chrome/browser/ui/webui/history/history_ui.h"  // nogncheck
#include "components/page_image_service/mojom/page_image_service.mojom.h"  // nogncheck
#include "components/user_education/webui/user_education.mojom.h"  // nogncheck
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom.h"  // nogncheck
#include "ui/webui/resources/cr_components/history/history.mojom.h"  // nogncheck
#include "ui/webui/resources/cr_components/history/history_cross_device_signin_promo.mojom.h"  // nogncheck
#endif

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

void PopulateChromeWebUIFrameBindersPartsAndroid(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<
      chrome_finds_internals::mojom::PageHandlerFactory,
      chrome_finds_internals::ChromeFindsInternalsUI>(map);
#if BUILDFLAG(ENABLE_WEBUI_HISTORY)
  RegisterWebUIControllerInterfaceBinder<history::mojom::PageHandler,
                                         HistoryUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      history::mojom::ForeignSessionPageHandlerFactory, HistoryUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      history_cross_device_signin_promo::mojom::
          HistoryCrossDeviceSigninPromoHandler,
      HistoryUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      user_education::mojom::UserEducationMixedTrustHandlerFactory, HistoryUI>(
      map);
  RegisterWebUIControllerInterfaceBinder<
      page_image_service::mojom::PageImageServiceHandler, HistoryUI>(map);
#endif
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
