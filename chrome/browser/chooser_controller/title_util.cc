// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif

std::u16string CreateChooserTitle(content::RenderFrameHost* render_frame_host,
                                  int title_string_id_origin,
                                  int title_string_id_extension) {
  if (!render_frame_host)
    return u"";

  url::Origin origin = render_frame_host->GetLastCommittedOrigin();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(browser_context);
    if (extension_registry) {
      const extensions::Extension* extension =
          extension_registry->enabled_extensions().GetByID(origin.host());
      if (extension) {
        return l10n_util::GetStringFUTF16(title_string_id_extension,
                                          base::UTF8ToUTF16(extension->name()));
      }
    }
  }
#endif

  return l10n_util::GetStringFUTF16(
      title_string_id_origin,
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}
