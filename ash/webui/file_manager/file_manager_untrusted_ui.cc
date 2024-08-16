// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/file_manager/file_manager_untrusted_ui.h"

#include "ash/webui/file_manager/untrusted_resources/grit/file_manager_untrusted_resources.h"
#include "ash/webui/file_manager/untrusted_resources/grit/file_manager_untrusted_resources_map.h"
#include "ash/webui/file_manager/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ash {
namespace file_manager {

FileManagerUntrustedUIConfig::FileManagerUntrustedUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         kChromeUIFileManagerUntrustedHost) {}

FileManagerUntrustedUIConfig::~FileManagerUntrustedUIConfig() = default;

FileManagerUntrustedUI::FileManagerUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIFileManagerUntrustedURL);

  untrusted_source->AddResourcePaths(base::make_span(
      kFileManagerUntrustedResources, kFileManagerUntrustedResourcesSize));

  untrusted_source->AddFrameAncestor(GURL(kChromeUIFileManagerURL));

  // By default, prevent all network access. Allow framing blob: URLs for
  // browsable content.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src blob: 'self';");

  // Allow <img>, <audio>, <video> to handle blob: and data: URLs.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc,
      "default-src blob: data: 'self';");

  // Allow inline styling.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'unsafe-inline' 'self';");
}

FileManagerUntrustedUI::~FileManagerUntrustedUI() = default;

}  // namespace file_manager
}  // namespace ash
