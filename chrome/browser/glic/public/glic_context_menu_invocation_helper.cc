// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/public/glic_context_menu_invocation_helper.h"

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
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
#include "mojo/public/cpp/base/big_buffer.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

// static
void GlicContextMenuInvocationHelper::HandleContextualMenuClick(
    tabs::TabInterface* tab, const std::u16string& selection_text,
    content::GlobalRenderFrameHostId rfh_id) {
  if (!tab || !tab->GetContents()) {
    return;
  }

  auto* browser_context = tab->GetContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!glic::GlicEnabling::IsContextualMenuItemEnabled(profile,
                                                       selection_text)) {
    return;
  }

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(browser_context);
  if (glic_service) {
    glic::GlicInvokeOptions options(
        glic::Target(*tab),
        glic::mojom::InvocationSource::kWebContentsContextMenu);

    std::u16string_view trimmed_selection =
        base::TrimWhitespace(selection_text, base::TRIM_ALL);
    if (base::FeatureList::IsEnabled(features::kGlicTextSelectionContextMenu) &&
        !trimmed_selection.empty()) {
      auto context = glic::mojom::AdditionalContext::New();
      context->source = glic::mojom::AdditionalContextSource::kTextSelection;
      context->tab_id = tab->GetHandle().raw_value();

      auto data = glic::mojom::ContextData::New();
      data->mime_type = kMimeTypeGlicSelection;

      std::string utf8_text = base::UTF16ToUTF8(trimmed_selection);
      data->data = mojo_base::BigBuffer(base::as_byte_span(utf8_text));

      auto part = glic::mojom::AdditionalContextPart::NewData(std::move(data));
      context->parts.push_back(std::move(part));

      options.additional_context = glic::AdditionalTabContext(
          std::move(context), rfh_id, glic::PolicyCheck::kClipboard);
      options.fre_override = glic::mojom::FreOverride::kTrustFirstClick;

      glic_service->Invoke(std::move(options));
      return;
    }

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
