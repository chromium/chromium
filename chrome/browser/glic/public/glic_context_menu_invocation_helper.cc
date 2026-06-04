// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/public/glic_context_menu_invocation_helper.h"

#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

// static
void GlicContextMenuInvocationHelper::HandleContextualMenuClick(
    tabs::TabInterface* tab) {
  if (!tab || !tab->GetContents()) {
    return;
  }

  auto* browser_context = tab->GetContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!glic::GlicEnabling::IsContextualMenuItemEnabled(profile)) {
    return;
  }

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(browser_context);
  if (glic_service) {
    glic::GlicInvokeOptions options(
        glic::Target(tab),
        glic::mojom::InvocationSource::kWebContentsContextMenu);
    std::string arm = features::kGlicContextMenuArm.Get();
    if (arm == "arm3") {
      options.fre_override = glic::mojom::FreOverride::kTrustFirstClick;
    } else {
      options.fre_override = glic::mojom::FreOverride::kTrustFirstInline;
    }
    if (arm == "arm2") {
      options.prompts.push_back(
          l10n_util::GetStringUTF8(IDS_GLIC_SUMMARIZE_PAGE_PROMPT));
      glic_service->InvokeWithAutoSubmit(
          glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
          std::move(options));
    } else {
      glic_service->Invoke(std::move(options));
    }
  }
}

}  // namespace glic
