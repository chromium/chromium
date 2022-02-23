// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/permissions/chooser_title_util.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif

std::u16string CreateExtensionAwareChooserTitle(
    content::RenderFrameHost* render_frame_host,
    int title_string_id_origin,
    int title_string_id_extension) {
  if (!render_frame_host)
    return u"";
  // Ensure the permission request is attributed to the main frame.
  render_frame_host = render_frame_host->GetMainFrame();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  url::Origin origin = render_frame_host->GetLastCommittedOrigin();
  if (origin.scheme() == extensions::kExtensionScheme) {
    content::BrowserContext* browser_context =
        render_frame_host->GetBrowserContext();
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

  return permissions::CreateChooserTitle(render_frame_host,
                                         title_string_id_origin);
}
