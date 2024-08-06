// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/focus_mode/focus_mode_untrusted_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/url_constants.h"
#include "ash/webui/grit/ash_focus_mode_player_resources.h"
#include "ash/webui/grit/ash_focus_mode_player_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

FocusModeUntrustedUI::FocusModeUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  // Set up the chrome://focus-mode-media source. Note that for the untrusted
  // page, we need to pass the *URL* as second parameter, and it must include a
  // terminating slash, otherwise the data source won't be found.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIFocusModePlayerURL);

  // Add the content. We don't need to set up a default ("") path since the
  // trusted page will refer directly to player.html.
  source->AddResourcePaths(base::make_span(kAshFocusModePlayerResources,
                                           kAshFocusModePlayerResourcesSize));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'self';");
  // Enables the page to actually load media.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc, "media-src *;");
  // Enables the page to load images. The page is restricted to only loading
  // images from data URLs passed to the page.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src data:;");
  // Enables the page to be loaded as an iframe by the trusted page.
  source->AddFrameAncestor(GURL(chrome::kChromeUIFocusModeMediaURL));
}

FocusModeUntrustedUI::~FocusModeUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(FocusModeUntrustedUI)

FocusModeUntrustedUIConfig::FocusModeUntrustedUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         chrome::kChromeUIFocusModePlayerHost) {}

bool FocusModeUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsFocusModeEnabled();
}

}  // namespace ash
