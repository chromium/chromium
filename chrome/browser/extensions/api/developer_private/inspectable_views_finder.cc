// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"

#include <set>

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "url/gurl.h"

namespace extensions {

InspectableViewsFinder::InspectableViewsFinder(Profile* profile)
    : profile_(profile) {
}

InspectableViewsFinder::~InspectableViewsFinder() {
}

api::developer_private::ViewType ConvertViewType(const mojom::ViewType type) {
  api::developer_private::ViewType developer_private_type =
      api::developer_private::ViewType::kNone;
  switch (type) {
    case mojom::ViewType::kAppWindow:
      developer_private_type = api::developer_private::ViewType::kAppWindow;
      break;
    case mojom::ViewType::kBackgroundContents:
      developer_private_type =
          api::developer_private::ViewType::kBackgroundContents;
      break;
    case mojom::ViewType::kComponent:
      developer_private_type = api::developer_private::ViewType::kComponent;
      break;
    case mojom::ViewType::kExtensionBackgroundPage:
      developer_private_type =
          api::developer_private::ViewType::kExtensionBackgroundPage;
      break;
    case mojom::ViewType::kExtensionGuest:
      developer_private_type =
          api::developer_private::ViewType::kExtensionGuest;
      break;
    case mojom::ViewType::kExtensionPopup:
      developer_private_type =
          api::developer_private::ViewType::kExtensionPopup;
      break;
    case mojom::ViewType::kTabContents:
      developer_private_type = api::developer_private::ViewType::kTabContents;
      break;
    case mojom::ViewType::kOffscreenDocument:
      developer_private_type =
          api::developer_private::ViewType::kOffscreenDocument;
      break;
    case mojom::ViewType::kExtensionSidePanel:
      developer_private_type =
          api::developer_private::ViewType::kExtensionSidePanel;
      break;
    case mojom::ViewType::kDeveloperTools:
      developer_private_type =
          api::developer_private::ViewType::kDeveloperTools;
      break;
    case mojom::ViewType::kInvalid:
      NOTREACHED();
  }
  DCHECK(developer_private_type != api::developer_private::ViewType::kNone);
  return developer_private_type;
}

// static
InspectableViewsFinder::View InspectableViewsFinder::ConstructView(
    const GURL& url,
    int render_process_id,
    int render_frame_id,
    bool incognito,
    bool is_iframe,
    api::developer_private::ViewType type) {
  api::developer_private::ExtensionView view;
  view.url = url.spec();
  view.render_process_id = render_process_id;
  // NOTE(devlin): This is called "render_view_id" in the api for legacy
  // reasons, but it's not a high priority to change.
  view.render_view_id = render_frame_id;
  view.incognito = incognito;
  view.is_iframe = is_iframe;
  view.type = type;
  return view;
}

InspectableViewsFinder::ViewList InspectableViewsFinder::GetViewsForExtension(
    const Extension& extension,
    bool is_enabled) {
  ViewList result;
  if (!ChromeDevToolsManagerDelegate::AllowInspection(profile_, &extension))
    return result;
  GetViewsForExtensionForProfile(
      extension, profile_, is_enabled, false, &result);
  if (profile_->HasPrimaryOTRProfile()) {
    GetViewsForExtensionForProfile(
        extension, profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        is_enabled, true, &result);
  }

  return result;
}

void InspectableViewsFinder::GetViewsForExtensionForProfile(
    const Extension& extension,
    Profile* profile,
    bool is_enabled,
    bool is_incognito,
    ViewList* result) {
  ProcessManager* process_manager = ProcessManager::Get(profile);
  // Get the extension process's active views.
  GetViewsForExtensionProcess(extension,
                              process_manager,
                              is_incognito,
                              result);
  // Get app window views, if not incognito.
  if (!is_incognito)
    GetAppWindowViewsForExtension(extension, result);
  // Include a link to start the lazy background page, if applicable.
  bool include_lazy_background = true;
  // Don't include the lazy background page for incognito if the extension isn't
  // enabled incognito or doesn't have a separate background page in incognito.
  if (is_incognito &&
      (!util::IsIncognitoEnabled(extension.id(), profile) ||
       !IncognitoInfo::IsSplitMode(&extension))) {
    include_lazy_background = false;
  }

  // Get inactive backgrounds.
  if (!include_lazy_background || !is_enabled)
    return;
  if (BackgroundInfo::HasLazyBackgroundPage(&extension) &&
      !process_manager->GetBackgroundHostForExtension(extension.id())) {
    result->push_back(ConstructView(
        BackgroundInfo::GetBackgroundURL(&extension), -1, -1, is_incognito,
        false, api::developer_private::ViewType::kExtensionBackgroundPage));
  }
  if (BackgroundInfo::IsServiceWorkerBased(&extension) &&
      process_manager->GetServiceWorkersForExtension(extension.id()).empty()) {
    result->push_back(ConstructView(
        extension.GetResourceURL(
            BackgroundInfo::GetBackgroundServiceWorkerScript(&extension)),
        -1, -1, is_incognito, false,
        api::developer_private::ViewType::kExtensionServiceWorkerBackground));
  }
}

void InspectableViewsFinder::GetViewsForExtensionProcess(
    const Extension& extension,
    ProcessManager* process_manager,
    bool is_incognito,
    ViewList* result) {
  const std::set<content::RenderFrameHost*>& hosts =
      process_manager->GetRenderFrameHostsForExtension(extension.id());
  for (content::RenderFrameHost* host : hosts) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(host);
    mojom::ViewType host_type = GetViewType(web_contents);
    if (host_type == mojom::ViewType::kInvalid ||
        host_type == mojom::ViewType::kExtensionPopup ||
        host_type == mojom::ViewType::kAppWindow) {
      continue;
    }

    GURL url = web_contents->GetURL();
    // If this is a background page that just opened, there might not be a
    // committed (or visible) url yet. In this case, use the initial url.
    if (url.is_empty()) {
      ExtensionHost* extension_host =
          process_manager->GetBackgroundHostForRenderFrameHost(host);
      if (extension_host)
        url = extension_host->initial_url();
    }

    content::RenderProcessHost* process = host->GetProcess();
    result->push_back(ConstructView(url, process->GetID(), host->GetRoutingID(),
                                    is_incognito, !host->IsInPrimaryMainFrame(),
                                    ConvertViewType(host_type)));
  }

  std::vector<WorkerId> service_worker_ids =
      process_manager->GetServiceWorkersForExtension(extension.id());
  for (const WorkerId& service_worker_id : service_worker_ids) {
    result->push_back(ConstructView(
        extension.GetResourceURL(
            BackgroundInfo::GetBackgroundServiceWorkerScript(&extension)),
        service_worker_id.render_process_id, -1, is_incognito, false,
        api::developer_private::ViewType::kExtensionServiceWorkerBackground));
  }
}

void InspectableViewsFinder::GetAppWindowViewsForExtension(
    const Extension& extension,
    ViewList* result) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(profile_);
  if (!registry)
    return;

  AppWindowRegistry::AppWindowList windows =
      registry->GetAppWindowsForApp(extension.id());

  for (const AppWindow* window : windows) {
    content::WebContents* web_contents = window->web_contents();

    // If the window just opened, there might not be a committed (or visible)
    // url yet. In this case, use the initial url.
    GURL url = web_contents->GetLastCommittedURL();
    if (url.is_empty())
      url = window->initial_url();

    content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    result->push_back(ConstructView(
        url, main_frame->GetProcess()->GetID(), main_frame->GetRoutingID(),
        false, false, ConvertViewType(GetViewType(web_contents))));
  }
}

}  // namespace extensions
