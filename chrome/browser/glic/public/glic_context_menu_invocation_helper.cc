// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/public/glic_context_menu_invocation_helper.h"

#include "base/strings/string_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/selection/selection_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/glic_enums.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#endif

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
    std::u16string_view trimmed_selection =
        base::TrimWhitespace(selection_text, base::TRIM_ALL);
    bool is_page_eligible = false;
    if (base::FeatureList::IsEnabled(features::kGlicTextSelectionContextMenu) &&
        !trimmed_selection.empty()) {
#if !BUILDFLAG(IS_ANDROID)
      tabs::PageContextEligibilityHelper* helper =
          tab ? tabs::PageContextEligibilityHelper::From(tab) : nullptr;
      is_page_eligible =
          helper &&
          helper->IsPageContextEligible() ==
              optimization_guide::PageContextEligibilityStatus::kEligible;
#endif
    }

    // Desktop has no link-specific item, so clicks without a selection report
    // Page, even on a link.
    glic::GlicInvokeOptions options(
        glic::Target(*tab),
        trimmed_selection.empty()
            ? mojom::InvocationSource::kWebContentsContextMenuPage
            : mojom::InvocationSource::kWebContentsContextMenuTextSelection);

    if (is_page_eligible) {
      options.additional_context = glic::AdditionalTabContext(
          CreateTextSelectionContext(tab->GetContents(),
                                     std::u16string(trimmed_selection),
                                     SelectionTextLength::kFull),
          rfh_id, glic::PolicyCheck::kClipboard);
      options.fre_override =
          features::kGlicTextSelectionContextMenuMessageFirstFre.Get()
              ? glic::mojom::FreOverride::kTrustFirstInline
              : glic::mojom::FreOverride::kTrustFirstClick;

#if !BUILDFLAG(IS_ANDROID)
      if (auto* observer = GlicSelectionObserver::From(tab)) {
        observer->UpdateSelectionStateFromContextMenu(
            std::u16string(trimmed_selection));
      }
#endif

      if (features::kGlicTextSelectionContextMenuAutoSubmit.Get()) {
        // The selection itself is sent as additional context, so the prompt
        // only provides the accompanying user message.
        options.prompts.push_back(l10n_util::GetStringUTF8(
            IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_TELL_ME));
        glic_service->InvokeWithAutoSubmit(
            glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
            std::move(options));
      } else {
        glic_service->Invoke(std::move(options));
      }
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

// static
void GlicContextMenuInvocationHelper::HandleSmartSuggestionClick(
    content::RenderFrameHost& frame,
    const content::ContextMenuParams& params) {
#if !BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&frame);
  if (web_contents) {
    web_contents = web_contents->GetResponsibleWebContents();
  }
  if (!web_contents ||
      !GlicEnabling::IsEnabledForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    return;
  }
  // `ShowWithSelection()` checks page context eligibility.
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  if (!controller) {
    return;
  }

  // A text selection takes precedence over an image or element.
  // Selections in password fields are ignored.
  gfx::Rect selection_bounds;
  auto interaction_options = selection::InteractionOptions::New();
  ::selection::EntryPoint entry_point =
      ::selection::EntryPoint::kContextMenuPage;
  const bool is_password_field =
      params.form_control_type == blink::mojom::FormControlType::kInputPassword;
  std::u16string_view trimmed_selection =
      base::TrimWhitespace(params.selection_text, base::TRIM_ALL);
  if (!is_password_field && !trimmed_selection.empty()) {
    entry_point = ::selection::EntryPoint::kContextMenuText;
    if (auto* observer = GlicSelectionObserver::From(
            tabs::TabInterface::MaybeGetFromContents(web_contents))) {
      observer->UpdateSelectionStateFromSmartSuggestionContextMenu(
          std::u16string(trimmed_selection));
    }
    selection_bounds =
        web_contents->GetTextSelectionBounds(&frame).value_or(gfx::Rect());
    if (!selection_bounds.IsEmpty()) {
      interaction_options->hide_handles = true;
      interaction_options->skip_intro_animation = true;
    }
  } else {
    const bool is_image =
        params.media_type == blink::mojom::ContextMenuDataMediaType::kImage;
    if (is_image && params.has_image_contents) {
      entry_point = ::selection::EntryPoint::kContextMenuImage;
    }
    const bool is_broken_image = is_image && !params.has_image_contents;
    if (!is_broken_image && !params.node_bounds.IsEmpty()) {
      if (content::RenderWidgetHostView* view = frame.GetView()) {
        gfx::Rect view_bounds = view->GetViewBounds();
        selection_bounds = params.node_bounds;
        selection_bounds.Intersect(gfx::Rect(view_bounds.size()));
        selection_bounds.Offset(view_bounds.OffsetFromOrigin());
        selection_bounds.Intersect(web_contents->GetViewBounds());
      }
      if (!selection_bounds.IsEmpty()) {
        interaction_options->hide_handles = false;
        interaction_options->skip_intro_animation = true;
      }
    }
  }
  controller->ShowWithSelection(&frame, selection_bounds,
                                std::move(interaction_options), entry_point);
#endif
}

}  // namespace glic
