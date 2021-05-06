// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/os_feedback_ui/os_feedback_ui.h"

#include "ash/components/os_feedback_ui/url_constants.h"
#include "ash/grit/ash_os_feedback_resources.h"
#include "ash/grit/ash_os_feedback_resources_map.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
}

}  // namespace

OSFeedbackUI::OSFeedbackUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIOSFeedbackHost));

  const auto resources =
      base::make_span(kAshOsFeedbackResources, kAshOsFeedbackResourcesSize);
  SetUpWebUIDataSource(source.get(), resources, IDR_ASH_OS_FEEDBACK_INDEX_HTML);

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

OSFeedbackUI::~OSFeedbackUI() = default;

}  // namespace ash
