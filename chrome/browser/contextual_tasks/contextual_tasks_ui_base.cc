// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_base.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"
#include "chrome/grit/webui_toolbar_shared_resources.h"
#include "chrome/grit/webui_toolbar_shared_resources_map.h"
#endif

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/grit/guest_view_shared_resources_map.h"  // nogncheck
#endif

namespace contextual_tasks {

ContextualTasksUIBase::ContextualTasksUIBase(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              /*enable_chrome_send=*/true,
                              /*enable_chrome_histograms=*/true) {}

ContextualTasksUIBase::~ContextualTasksUIBase() = default;

Profile* ContextualTasksUIBase::GetProfile() {
  return Profile::FromWebUI(web_ui());
}

content::WebUIDataSource* ContextualTasksUIBase::RegisterWebUIDataSource(
    Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIContextualTasksHost);
  webui::SetupWebUIDataSource(source, kContextualTasksResources,
                              IDR_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' https://*.google.com;");

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  source->AddResourcePaths(kGuestViewSharedResources);
#endif

#if !BUILDFLAG(IS_ANDROID)
  source->AddResourcePaths(kWebuiToolbarSharedResources);
  WebUIToolbarLayoutCssHelper::SetAsRequestFilter(source);
#endif

  source->AddResourcePath(
      "internals",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);
  source->AddResourcePath(
      "internals/",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);

  source->AddLocalizedStrings(GetContextualTasksLoadTimeData(profile));

  return source;
}

base::DictValue ContextualTasksUIBase::GetContextualTasksLoadTimeData(
    Profile* profile) {
  base::DictValue dict;
  return dict;
}

void ContextualTasksUIBase::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks_toolbar::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks_toolbar::mojom::PageHandler>
        page_handler) {
  toolbar_page_.reset();
  toolbar_page_handler_receiver_.reset();
  toolbar_page_.Bind(std::move(page));
  toolbar_page_handler_receiver_.Bind(std::move(page_handler));
}

void ContextualTasksUIBase::BindInterface(
    mojo::PendingReceiver<contextual_tasks_toolbar::mojom::PageHandlerFactory>
        pending_receiver) {
  toolbar_page_factory_receiver_.reset();
  toolbar_page_factory_receiver_.Bind(std::move(pending_receiver));
}

}  // namespace contextual_tasks
