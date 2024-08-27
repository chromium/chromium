// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/graduation/graduation_ui.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_graduation_resources.h"
#include "ash/webui/grit/ash_graduation_resources_map.h"
#include "base/containers/span.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::graduation {

namespace {
void AddResources(content::WebUIDataSource* source) {
  source->SetDefaultResource(IDR_ASH_GRADUATION_INDEX_HTML);
  source->AddResourcePaths(
      base::make_span(kAshGraduationResources, kAshGraduationResourcesSize));
}
}  // namespace

bool GraduationUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  // TODO(b/357883712): Check Graduation policy status
  return features::IsGraduationEnabled();
}

GraduationUI::GraduationUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, false) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIGraduationAppURL));
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(kChromeUIGraduationAppHost));
  ash::EnableTrustedTypesCSP(source);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  AddResources(source);
}

GraduationUI::~GraduationUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(GraduationUI)

}  // namespace ash::graduation
