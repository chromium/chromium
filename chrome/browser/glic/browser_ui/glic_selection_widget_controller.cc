// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller_delegate.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/inline_cue_blocklist_utils.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/selection/text_selection_context.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/glic_enums.mojom-shared.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

namespace glic {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicSelectionAction)
enum class GlicSelectionAction {
  // kNudgeShown = 0,  // Obsolete.
  kWidgetShown = 1,
  // kNudgeClicked = 2,  // Obsolete.
  kWidgetClicked = 3,
  kWidgetDismissedByButton = 4,
  kWidgetDismissedByClickOutside = 5,
  kMaxValue = kWidgetDismissedByClickOutside
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicSelectionAction)

size_t CountWords(std::u16string_view text) {
  size_t count = 0;
  base::StringView16Tokenizer tokenizer(
      text, u"", base::StringView16Tokenizer::WhitespacePolicy::kSkipOver);
  while (tokenizer.GetNext()) {
    ++count;
  }
  return count;
}

// Records the widget click and invokes Glic with `selected_text`. Only
// `GlicSelectionWidgetController` can get `auto_submit_passkey`, so it passes
// it in.
void InvokeGlicFromSelectionAffordance(
    const std::u16string& selected_text,
    content::WebContents& web_contents,
    InvokeWithAutoSubmitPasskey auto_submit_passkey) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  const char* histogram_suffix =
      GlicEnabling::HasConsentedForProfile(profile) ? ".PostFre" : ".PreFre";

  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Selection.Action", histogram_suffix}),
      GlicSelectionAction::kWidgetClicked);
  base::UmaHistogramCounts1000(
      base::StrCat(
          {"Glic.Selection.WidgetClicked.SelectionLength", histogram_suffix}),
      selected_text.length());
  base::UmaHistogramCounts1000(
      base::StrCat({"Glic.Selection.WidgetClicked.SelectionWordCount",
                    histogram_suffix}),
      CountWords(selected_text));

  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(&web_contents);
  if (!tab_interface || !tab_interface->GetBrowserWindowInterface()) {
    return;
  }
  auto* glic_keyed_service = GlicKeyedService::Get(profile);
  if (!glic_keyed_service) {
    return;
  }

  GlicInvokeOptions options(glic::Target(*tab_interface),
                            mojom::InvocationSource::kNudge);
  // The selected text flow doesn't support live mode, so leave a live
  // conversation alone rather than pulling it into the side panel.
  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  options.additional_context = AdditionalTabContext(
      CreateTextSelectionContext(&web_contents, selected_text),
      content::GlobalRenderFrameHostId(), PolicyCheck::kNone);
  if (features::kGlicSelectionAutoSendPrompt.Get()) {
    std::string cta = features::kGlicSelectionPromptCta.Get();
    std::string prompt =
        l10n_util::GetStringUTF8(IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_TELL_ME);
    if (cta == features::kGlicSelectionPromptCtaExplain) {
      prompt =
          l10n_util::GetStringUTF8(IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_EXPLAIN);
    }
    options.prompts.push_back(prompt);
    glic_keyed_service->InvokeWithAutoSubmit(auto_submit_passkey,
                                             std::move(options));
  } else {
    glic_keyed_service->Invoke(std::move(options));
  }
}

}  // namespace

GlicSelectionWidgetController::GlicSelectionWidgetController(
    content::WebContents* web_contents,
    GlicSelectionWidgetControllerDelegate& delegate)
    : web_contents_(web_contents->GetWeakPtr()), delegate_(delegate) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  glic_keyed_service_ = GlicKeyedService::Get(profile);

  if (profile) {
    if (auto* settings_map =
            HostContentSettingsMapFactory::GetForProfile(profile)) {
      content_settings_observation_.Observe(settings_map);
    }
  }
  UpdatePageBlockedState();
}

GlicSelectionWidgetController::~GlicSelectionWidgetController() = default;

void GlicSelectionWidgetController::OnAskGemini() {
  if (base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip)) {
    Dismiss(DismissReason::kActionTaken);
    ShowSelectionOverlay();
    return;
  }
  Dismiss(DismissReason::kActionTaken);
  InvokeGlicFromSelectionAffordance(
      delegate_->GetSelectedText(), *web_contents(),
      InvokeWithAutoSubmitPasskeyProvider::GetPassKey());
}

void GlicSelectionWidgetController::OnCopy() {
  Dismiss(DismissReason::kActionTaken);
  web_contents()->Copy();
}

void GlicSelectionWidgetController::OnHide() {
  is_hidden_on_current_page_ = true;

  Dismiss(DismissReason::kCloseButton);
  ShowHiddenToast(ToastId::kGlicSelectionHiddenForSite);
}

void GlicSelectionWidgetController::OnSettings() {
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (tab_interface) {
    BrowserWindowInterface* browser_window_interface =
        tab_interface->GetBrowserWindowInterface();
    if (browser_window_interface) {
      chrome::ShowContentSettingsExceptions(
          browser_window_interface, ContentSettingsType::INLINE_CUE_MENU);
    }
  }
}

void GlicSelectionWidgetController::OnWidgetClose() {
  if (widget_delegate_) {
    // Defer the destruction of the delegate to ensure the views::Widget is
    // destroyed first, and then the delegate. This is required under the
    // CLIENT_OWNS_WIDGET ownership model.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(widget_delegate_));
  }
}

gfx::Rect GlicSelectionWidgetController::GetContainerBounds() {
  content::WebContents* contents = web_contents();
  return contents ? contents->GetContainerBounds() : gfx::Rect();
}

void GlicSelectionWidgetController::Dismiss(DismissReason reason) {
  if (widget_delegate_) {
    if (!dismissal_recorded_ && reason != DismissReason::kActionTaken) {
      bool is_post_fre = false;
      if (web_contents()) {
        Profile* profile =
            Profile::FromBrowserContext(web_contents()->GetBrowserContext());
        is_post_fre = GlicEnabling::HasConsentedForProfile(profile);
      }
      const char* histogram_suffix = is_post_fre ? ".PostFre" : ".PreFre";
      GlicSelectionAction action =
          (reason == DismissReason::kCloseButton)
              ? GlicSelectionAction::kWidgetDismissedByButton
              : GlicSelectionAction::kWidgetDismissedByClickOutside;
      base::UmaHistogramEnumeration(
          base::StrCat({"Glic.Selection.Action", histogram_suffix}), action);
    }
    dismissal_recorded_ = true;
    widget_delegate_->CloseWidget();
  }
}

GlicSelectionWidgetController::ShowResult GlicSelectionWidgetController::Show(
    const std::u16string& selected_text) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return ShowResult::kSkipped;
  }
  bool is_post_fre = GlicEnabling::HasConsentedForProfile(profile);
  const char* histogram_suffix = is_post_fre ? ".PostFre" : ".PreFre";

  // Show selection widget
  if (!ShouldShowSelectionWidget()) {
    return ShowResult::kSkipped;
  }
  // Find the RenderFrameHost that has the selection.
  if (!delegate_->GetSelectedFrame()) {
    return ShowResult::kSkipped;
  }

  if (std::optional<gfx::Rect> bounds =
          delegate_->GetCurrentSelectionBounds()) {
    if (widget_delegate_) {
      widget_delegate_->CloseWidget();
    }

    base::UmaHistogramEnumeration(
        base::StrCat({"Glic.Selection.Action", histogram_suffix}),
        GlicSelectionAction::kWidgetShown);
    base::UmaHistogramCounts1000(
        base::StrCat(
            {"Glic.Selection.WidgetShown.SelectionLength", histogram_suffix}),
        selected_text.length());
    base::UmaHistogramCounts1000(
        base::StrCat({"Glic.Selection.WidgetShown.SelectionWordCount",
                      histogram_suffix}),
        CountWords(selected_text));

    widget_delegate_ = std::make_unique<GlicSelectionWidgetDelegate>(
        *this, *bounds, std::u16string(selected_text));
    widget_delegate_->set_parent_window(platform_util::GetViewForWindow(
        web_contents()->GetTopLevelNativeWindow()));
    dismissal_recorded_ = false;
    widget_delegate_->ShowWidget();
    return ShowResult::kShown;
  }
  return ShowResult::kNoBounds;
}

void GlicSelectionWidgetController::Close() {
  if (widget_delegate_) {
    widget_delegate_->CloseWidget();
  }
}

void GlicSelectionWidgetController::OnPrimaryPageChanged() {
  is_hidden_on_current_page_ = false;
  UpdatePageBlockedState();
}

void GlicSelectionWidgetController::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::INLINE_CUE_MENU)) {
    UpdatePageBlockedState();
    if (is_site_blocked_on_current_page_ && widget_delegate_) {
      Dismiss(DismissReason::kExternal);
    }
  }
}

void GlicSelectionWidgetController::ShowSelectionOverlay() {
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (!tab_interface) {
    return;
  }
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents());
  if (!controller) {
    return;
  }

  overlay_closed_subscription_ = controller->RegisterOverlayClosedCallback(
      base::BindOnce(
          [](GlicSelectionWidgetController* self,
             SelectionOverlayController::CloseReason reason) {
            if (reason ==
                    SelectionOverlayController::CloseReason::kCloseButton ||
                reason ==
                    SelectionOverlayController::CloseReason::kEscapeKeyPress) {
              self->OnHide();
            }
          },
          base::Unretained(this)));

  // When the side panel is open, let the web client start the capture session.
  if (glic_keyed_service_ &&
      GlicSidePanelCoordinator::IsShowing(tab_interface) &&
      controller->state() == OverlayBaseController::State::kOff) {
    GlicInvokeOptions options(
        Target(*tab_interface),
        glic::mojom::InvocationSource::kCaptureRegionHotkey);
    options.wait_for_panel_open = true;
    glic_keyed_service_->Invoke(std::move(options));
    return;
  }
  // When the side panel is not open, show the overlay directly with the
  // current selection.
  if (std::optional<gfx::Rect> bounds =
          delegate_->GetCurrentSelectionBounds()) {
    controller->ShowWithSelection(
        delegate_->GetSelectedFrame(), *bounds,
        selection::InteractionOptions::New(
            /*hide_handles=*/true, /*disable_multi_select=*/true));
  } else {
    controller->Show(/*options=*/nullptr);
  }
}

bool GlicSelectionWidgetController::ShouldShowSelectionWidget() {
  return !is_hidden_on_current_page_ && !is_site_blocked_on_current_page_ &&
         web_contents() && !web_contents()->IsShowingContextMenu();
}

void GlicSelectionWidgetController::UpdatePageBlockedState() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  is_site_blocked_on_current_page_ =
      IsSiteBlockedForInlineCue(profile, web_contents()->GetLastCommittedURL());
}

void GlicSelectionWidgetController::ShowHiddenToast(ToastId toast_id) {
  if (auto* toast_controller =
          ToastController::MaybeGetForWebContents(web_contents())) {
    toast_controller->MaybeShowToast(ToastParams(toast_id));
  }
}

}  // namespace glic
