// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/chooser_controller.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
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

namespace {

base::string16 CreateTitle(content::RenderFrameHost* render_frame_host,
                           int title_string_id_origin,
                           int title_string_id_extension) {
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

}  // namespace

ChooserController::ChooserController(content::RenderFrameHost* owner,
                                     int title_string_id_origin,
                                     int title_string_id_extension) {
  if (owner) {
    title_ =
        CreateTitle(owner, title_string_id_origin, title_string_id_extension);
  }
}

ChooserController::~ChooserController() {}

base::string16 ChooserController::GetTitle() const {
  return title_;
}

bool ChooserController::ShouldShowIconBeforeText() const {
  return false;
}

bool ChooserController::ShouldShowHelpButton() const {
  return true;
}

bool ChooserController::ShouldShowReScanButton() const {
  return false;
}

bool ChooserController::AllowMultipleSelection() const {
  return false;
}

base::string16 ChooserController::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_CANCEL_BUTTON_TEXT);
}

bool ChooserController::BothButtonsAlwaysEnabled() const {
  return false;
}

bool ChooserController::TableViewAlwaysDisabled() const {
  return false;
}

int ChooserController::GetSignalStrengthLevel(size_t index) const {
  return -1;
}

bool ChooserController::IsConnected(size_t index) const {
  return false;
}

bool ChooserController::IsPaired(size_t index) const {
  return false;
}

void ChooserController::RefreshOptions() {
  NOTREACHED();
}

base::string16 ChooserController::GetStatus() const {
  return base::string16();
}

void ChooserController::OpenAdapterOffHelpUrl() const {
  NOTREACHED();
}
