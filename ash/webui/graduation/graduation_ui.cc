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
#include "ash/edusumer/graduation_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_graduation_resources.h"
#include "ash/webui/grit/ash_graduation_resources_map.h"
#include "base/containers/span.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::graduation {

namespace {
void AddResources(content::WebUIDataSource* source) {
  source->SetDefaultResource(IDR_ASH_GRADUATION_INDEX_HTML);
  source->AddResourcePaths(
      base::make_span(kAshGraduationResources, kAshGraduationResourcesSize));
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"backButtonLabel", IDS_GRADUATION_APP_BACK_BUTTON_LABEL},
      {"doneButtonLabel", IDS_GRADUATION_APP_DONE_BUTTON_LABEL},
      {"webviewLoadingMessage", IDS_GRADUATION_APP_WEBVIEW_LOADING_MESSAGE}};

  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddString("webviewUrl", kTakeoutTransferURL);

  // Set up test resources used in browser tests.
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}
}  // namespace

bool GraduationUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsGraduationEnabled() &&
         IsEligibleForGraduation(Shell::Get()
                                     ->session_controller()
                                     ->GetLastActiveUserPrefService());
}

GraduationUI::GraduationUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, false) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIGraduationAppURL));
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(kChromeUIGraduationAppHost));

  // Enable test resources.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  ash::EnableTrustedTypesCSP(source);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  AddResources(source);
}

GraduationUI::~GraduationUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(GraduationUI)

}  // namespace ash::graduation
