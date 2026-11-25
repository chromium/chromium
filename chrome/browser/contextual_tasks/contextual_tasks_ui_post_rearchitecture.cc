// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_post_rearchitecture.h"

#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

bool IsSidePanelChromeUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == chrome::kChromeUIContextualTasksHost &&
         (url.path() == "/toolbar.html" || url.path() == "/ghost_loader.html");
}

}  // namespace

ContextualTasksUIPostRearchitecture::ContextualTasksUIPostRearchitecture(
    content::WebUI* web_ui)
    : ContextualTasksUIBase(web_ui) {
  RegisterWebUIDataSource(GetProfile());
}

ContextualTasksUIPostRearchitecture::~ContextualTasksUIPostRearchitecture() =
    default;

void ContextualTasksUIPostRearchitecture::WebUIPrimaryPageChanged(
    content::Page& page) {
  ContextualTasksUIBase::WebUIPrimaryPageChanged(page);
  content::WebContents* web_contents = web_ui()->GetWebContents();
  if (!web_contents || tabs::TabInterface::MaybeGetFromContents(web_contents) ||
      !IsSidePanelChromeUrl(page.GetMainDocument().GetLastCommittedURL())) {
    zoom_lock_.reset();
    return;
  }

  // Lock side panel chrome WebUIs (toolbar and ghost loader) to 100% zoom so
  // profile default zoom or legacy host zoom does not clip fixed-height chips.
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller) {
    zoom_controller = zoom::ZoomController::CreateForWebContents(web_contents);
  }
  zoom_lock_ = zoom_controller->CreateZoomDisableLock();
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::Get(web_contents->GetSiteInstance());
  zoom_map->SetTemporaryZoomLevel(page.GetMainDocument().GetGlobalId(), 0.0);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUIPostRearchitecture)

}  // namespace contextual_tasks
