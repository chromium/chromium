// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/chooser_title_util.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
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
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());

  if (origin.scheme() == extensions::kExtensionScheme) {
    if (auto* extension_registry =
            extensions::ExtensionRegistry::Get(profile)) {
      if (const extensions::Extension* extension =
              extension_registry->enabled_extensions().GetByID(origin.host())) {
        return l10n_util::GetStringFUTF16(title_string_id_extension,
                                          base::UTF8ToUTF16(extension->name()));
      }
    }
  }

  // Isolated Web Apps should show the app's name instead of the origin.
  Browser* browser = chrome::FindBrowserWithWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host));
  if (browser && browser->app_controller() &&
      browser->app_controller()->IsIsolatedWebApp()) {
    return l10n_util::GetStringFUTF16(
        title_string_id_extension,
        browser->app_controller()->GetAppShortName());
  }
#endif

  return permissions::CreateChooserTitle(render_frame_host,
                                         title_string_id_origin);
}
