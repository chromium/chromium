// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_page_action_controller.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/types/pass_key.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/indigo/api_client.h"
#include "chrome/browser/indigo/indigo_agent_host.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/page_content_annotations/core/tracked_element_feature.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace indigo {

namespace {
const char kForceIndigoSwitch[] = "force-indigo";
const char kForceIndigoOnboardingSwitch[] = "force-indigo-onboarding";

void RecordTransformationResultCannotGenerateImage(
    const CombinedEligibility& eligibility) {
  DCHECK(!eligibility.CanGenerateImage());
  IndigoTransformationResult result;

  if (eligibility.local_eligibility != LocalEligibility::kEligible) {
    switch (eligibility.local_eligibility) {
      case LocalEligibility::kNotSignedIn:
        result = IndigoTransformationResult::kNotSignedIn;
        break;
      case LocalEligibility::kRefreshTokenInPersistentErrorState:
        result =
            IndigoTransformationResult::kRefreshTokenInPersistentErrorState;
        break;
      case LocalEligibility::kMissingCapabilities:
        result = IndigoTransformationResult::kMissingCapabilities;
        break;
      case LocalEligibility::kDisabledByPolicy:
        result = IndigoTransformationResult::kDisabledByPolicy;
        break;
      case LocalEligibility::kMissingScript:
        result = IndigoTransformationResult::kMissingScript;
        break;
      case LocalEligibility::kEligible:
        NOTREACHED();
    }
  } else if (!eligibility.remote_eligibility.has_value()) {
    result = IndigoTransformationResult::kRemoteStatusMissing;
  } else if (!eligibility.remote_eligibility
                  ->is_service_supported_for_account) {
    result = IndigoTransformationResult::kServiceNotSupported;
  } else if (!eligibility.remote_eligibility->has_user_image) {
    result = IndigoTransformationResult::kMissingUserImage;
  } else if (!eligibility.has_onboarded_pref) {
    result = IndigoTransformationResult::kNotOnboarded;
  } else {
    result = IndigoTransformationResult::kUnknown;
  }

  base::UmaHistogramEnumeration("Indigo.Transformation.Result", result);
}
}  // namespace

DEFINE_USER_DATA(IndigoPageActionController);

IndigoPageActionController::IndigoPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : tabs::ContentsObservingTabFeature(tab_interface),
      page_actions::PageActionObserver(kActionIndigo),
      page_action_controller_(page_action_controller),
      optimization_guide_(OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(
              tab_interface.GetContents()->GetBrowserContext()))),
      indigo_service_(
          IndigoServiceFactory::GetForProfile(Profile::FromBrowserContext(
              tab_interface.GetContents()->GetBrowserContext()))),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  RegisterAsPageActionObserver(page_action_controller);

  if (optimization_guide_) {
    optimization_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::OptimizationType::INDIGO});
  }

  if (indigo_service_) {
    indigo_service_subscription_ =
        indigo_service_->RegisterLocalEligibilityChangedCallback(
            base::BindRepeating(
                &IndigoPageActionController::OnLocalEligibilityChanged,
                base::Unretained(this)));
  }

  // TODO(b/511166876): Split view visual swaps (reversing panels) and some
  // other related changes do not fire tab visibility changes, even though it
  // may have changed which ContentsContainerView shows a tab contents.
  // We'll need to either observe these directly or create a higher level
  // event that describes when a tab may have changed how it is rendered in the
  // BrowserView.
  tab_became_hidden_subscription_ = tab_interface.RegisterWillBecomeHidden(
      base::BindRepeating(&IndigoPageActionController::TabWillBecomeHidden,
                          base::Unretained(this)));
  tab_became_visible_subscription_ = tab_interface.RegisterDidBecomeVisible(
      base::BindRepeating(&IndigoPageActionController::TabDidBecomeVisible,
                          base::Unretained(this)));

  content::RenderWidgetHost* host = nullptr;
  if (tab_interface.GetContents() &&
      tab_interface.GetContents()->GetRenderViewHost()) {
    host = tab_interface.GetContents()->GetRenderViewHost()->GetWidget();
  }
  RegisterObserverWithHost(host);

  UpdateEntryPointsState();
}

IndigoPageActionController::~IndigoPageActionController() {
  UnregisterObserverFromHost(current_host_);
  // If there is a toolbar, hide it before anything else. This makes sure that
  // the OnClose delegate function isn't called after some members have been
  // destroyed.
  DestroyToolbar();
}

// static
IndigoPageActionController* IndigoPageActionController::From(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

void IndigoPageActionController::InvokeAction() {
  base::RecordAction(base::UserMetricsAction("Indigo.PageAction.Click"));

  if (!indigo_service_) {
    return;
  }

  indigo_service_->GetCombinedEligibility(
      base::BindOnce(&IndigoPageActionController::CheckEligibilityForOnboarding,
                     invoke_weak_ptr_factory_.GetWeakPtr()));
}

void IndigoPageActionController::CheckEligibilityForOnboarding(
    const CombinedEligibility& eligibility) {
  if (eligibility.local_eligibility ==
      LocalEligibility::kRefreshTokenInPersistentErrorState) {
    RecordTransformationResultCannotGenerateImage(eligibility);
    content::WebContents* web_contents = tab().GetContents();
    if (web_contents) {
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      // TODO(b/513564094): Consider a gentler UI (e.g. a toast/bubble) if users
      // are confused by the sudden tab launch.
      signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
          profile, signin_metrics::AccessPoint::kIndigo);
    }
    return;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool force_onboarding =
      command_line->HasSwitch(kForceIndigoOnboardingSwitch);

  // Show onboarding if the user is ready to onboard, or if it's forced.
  if (eligibility.ReadyToOnboard() || force_onboarding) {
    ShowOnboardingDialog(OnboardingDisposition::kDefault);
    return;
  }

  ContinueInvoke(eligibility);
}

void IndigoPageActionController::ContinueInvoke(
    const CombinedEligibility& eligibility) {
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  if (!eligibility.CanGenerateImage()) {
    // TODO(b/505743640): Show a toast or something if we can't generate an
    // image and aren't ready to onboard.
    LOG(WARNING)
        << "Indigo not eligible for generation and not ready to onboard";
    RecordTransformationResultCannotGenerateImage(eligibility);
    return;
  }

  if (base::FeatureList::IsEnabled(features::kIndigoOpenGlic)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (auto* glic_keyed_service = glic::GlicKeyedService::Get(profile)) {
      glic::GlicInvokeOptions options(
          glic::Target(tab()),
          glic::mojom::InvocationSource::kIndigoPageAction);

      std::string skill_id = features::kIndigoGlicSkillId.Get();
      const skills::Skill* skill = nullptr;
      if (!skill_id.empty()) {
        if (auto* skills_service =
                skills::SkillsServiceFactory::GetForProfile(profile)) {
          skill = skills_service->GetSkillById(skill_id);
        }
      }

      std::string prompt;
      if (skill) {
        prompt = skill->prompt;
        options.skill_id = skill_id;
      } else {
        prompt = features::kIndigoGlicPrompt.Get();
        if (prompt.empty()) {
          std::string prompt_key = features::kIndigoGlicPromptKey.Get();
          if (!prompt_key.empty() && indigo_service_) {
            std::optional<std::string> proto_prompt =
                indigo_service_->GetPrompt(prompt_key);
            if (proto_prompt.has_value()) {
              prompt = *proto_prompt;
            }
          }
        }
      }

      if (!prompt.empty()) {
        options.prompts.push_back(std::move(prompt));
        glic_keyed_service->InvokeWithAutoSubmit(
            glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
            std::move(options));
      }
    }
  }

  if (IndigoAgentHost::GetOrCreateForPage(web_contents->GetPrimaryPage())
          ->Invoke()) {
    base::RecordAction(
        base::UserMetricsAction("Indigo.Transformation.Trigger"));
    return;
  }
}

void IndigoPageActionController::ShowOnboardingDialog(
    OnboardingDisposition disposition) {
  if (onboarding_dialog_) {
    return;
  }

  std::string onboarding_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kForceIndigoOnboardingSwitch);
  if (onboarding_url.empty()) {
    onboarding_url = features::kIndigoOnboardingUrl.Get();
  }

  GURL url(onboarding_url);
  if (disposition == OnboardingDisposition::kReplacePhoto) {
    url = net::AppendQueryParameter(url, "toyri", "1");
    base::RecordAction(base::UserMetricsAction("Indigo.ReplaceImage.Trigger"));
  } else {
    base::RecordAction(base::UserMetricsAction("Indigo.Onboarding.Trigger"));
  }

  auto callback =
      base::BindOnce(&IndigoPageActionController::OnOnboardingDialogClosed,
                     invoke_weak_ptr_factory_.GetWeakPtr(), disposition);

  if (onboarding_dialog_factory_for_testing_) {
    onboarding_dialog_ = onboarding_dialog_factory_for_testing_.Run(
        tab(), url, std::move(callback));
  } else {
    onboarding_dialog_ =
        IndigoOnboardingDialog::Show(tab(), url, std::move(callback));
  }
}
void IndigoPageActionController::Reset(ResetType reset_type) {
  DestroyToolbar();
  tracked_bounds_ = std::nullopt;

  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  content::Page& primary_page = web_contents->GetPrimaryPage();
  if (auto* manager = IndigoImageReplacementManager::GetForPage(primary_page)) {
    manager->ResetAllReplacements(base::PassKey<IndigoPageActionController>());
  }

  if (reset_type == ResetType::kResetReplacementsAndContentScript) {
    if (auto* host = IndigoAgentHost::GetForPage(primary_page)) {
      host->Reset();
    }
  }
}
void IndigoPageActionController::ShowToolbar() {
  if (!toolbar_) {
    toolbar_ = std::make_unique<IndigoToolbar>(this);
  }
  views::View* parent_view = GetIndigoOverlayView();
  toolbar_->Show(parent_view);
  if (tracked_bounds_) {
    toolbar_->UpdateTrackedPosition(*tracked_bounds_);
  }
}

void IndigoPageActionController::ShowInvocationErrorToast() {
  ToastController* toast_controller =
      ToastController::MaybeGetForTabInterface(&tab());
  if (toast_controller) {
    toast_controller->MaybeShowToast(ToastParams(ToastId::kIndigoInvokeError));
  }
}

void IndigoPageActionController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  ContentsObservingTabFeature::DidFinishNavigation(navigation_handle);

  // Only care about navigations where the URL seems to have changed, excluding
  // the URL fragment. Notably we _do_ care about navigation within a
  // single-page application.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->GetPreviousPrimaryMainFrameURL().EqualsIgnoringRef(
          navigation_handle->GetURL())) {
    return;
  }

  // We only listen for same document navigations here because for
  // cross-document navigations, the previous page gets destroyed (the
  // replacements will already be reset as part of page destruction).
  // Note: A page with active IndigoImageReplacements will never enter BFCache
  // since we don't currently support keeping extension frames in BFCache.
  if (navigation_handle->IsSameDocument()) {
    Reset(ResetType::kResetReplacementsAndContentScript);
  } else {
    DestroyToolbar();
  }

  if (onboarding_dialog_) {
    onboarding_dialog_->Close();
  }

  invoke_weak_ptr_factory_.InvalidateWeakPtrs();

  optimization_guide_decision_ =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  UpdateEntryPointsState();

  if (optimization_guide_) {
    const GURL& url = navigation_handle->GetURL();
    optimization_guide_->CanApplyOptimization(
        url, optimization_guide::proto::OptimizationType::INDIGO,
        base::BindOnce(&IndigoPageActionController::OnOptimizationGuideDecision,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void IndigoPageActionController::TabWillBecomeHidden(tabs::TabInterface* tab) {
  DCHECK_EQ(tab, &this->tab());
  if (toolbar_) {
    toolbar_->TabWillBecomeHidden();
  }
}

void IndigoPageActionController::TabDidBecomeVisible(tabs::TabInterface* tab) {
  DCHECK_EQ(tab, &this->tab());
  if (!toolbar_) {
    return;
  }

  auto* parent_view = GetIndigoOverlayView();
  if (!parent_view) {
    return;
  }

  toolbar_->TabDidBecomeVisible(parent_view);
}

void IndigoPageActionController::OnClose(IndigoToolbar* toolbar) {
  Reset(ResetType::kResetReplacementsAndContentScript);
}

void IndigoPageActionController::OnRegenerate(IndigoToolbar* toolbar) {
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  auto* manager =
      IndigoImageReplacementManager::GetForPage(web_contents->GetPrimaryPage());
  if (manager && manager->RegenerateImage()) {
    DestroyToolbar();
  }
}

void IndigoPageActionController::OnReplaceOriginalPhoto(
    IndigoToolbar* toolbar) {
  ShowOnboardingDialog(OnboardingDisposition::kReplacePhoto);
}

void IndigoPageActionController::OnDeleteOriginalPhoto(IndigoToolbar* toolbar) {
  if (!indigo_service_) {
    return;
  }

  Reset(ResetType::kResetReplacementsAndContentScript);

  indigo_service_->GetApiClient().Delete(
      base::BindOnce(&IndigoPageActionController::OnDeleteOriginalPhotoComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IndigoPageActionController::OnDeleteOriginalPhotoComplete(
    base::expected<void, DeleteError> result) {
  if (result.has_value()) {
    // TODO(b/509508517): Show a toast to inform the user the image
    // was deleted.
  } else {
    LOG(ERROR) << "Delete original photo failed: " << result.error().message;
  }
}

void IndigoPageActionController::UpdateEntryPointsState() {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  if (!indigo_service_) {
    return;
  }

  const bool forced =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kForceIndigoSwitch);
  const bool eligible =
      optimization_guide_decision_ ==
          optimization_guide::OptimizationGuideDecision::kTrue &&
      indigo_service_->IsLocallyEligible();

  const bool should_show = forced || eligible;
  if (should_show == is_shown_) {
    return;
  }

  if (should_show) {
    page_action_controller_->Show(kActionIndigo);
    if (indigo_service_->CanShowAnchoredMessage()) {
      page_action_controller_->SetAnchoredMessageText(
          kActionIndigo, l10n_util::GetStringUTF16(
                             IDS_INDIGO_ENTRYPOINT_ANCHORED_MESSAGE_TEXT));
      gfx::ImageSkia* icon =
          ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_GLIC_BUTTON_ALT_ICON);
      page_action_controller_->SetAnchoredMessageIcon(
          kActionIndigo,
          icon ? ui::ImageModel::FromImageSkia(*icon) : ui::ImageModel());
      page_action_controller_->ShowAnchoredMessage(
          kActionIndigo,
          {.priority =
               page_actions::PageActionPriorityCategory::kContextualCue});
    } else {
      page_action_controller_->ShowSuggestionChip(kActionIndigo);
    }
    base::RecordAction(base::UserMetricsAction("Indigo.PageAction.Show"));
  } else {
    page_action_controller_->Hide(kActionIndigo);
  }
  is_shown_ = should_show;
}

void IndigoPageActionController::OnOnboardingDialogClosed(
    OnboardingDisposition disposition,
    const OnboardingResult& result) {
  const bool acknowledged = result.acknowledge_chrome_disclaimer;
  onboarding_dialog_.reset();

  if (acknowledged) {
    content::WebContents* web_contents = tab().GetContents();
    if (!web_contents) {
      return;
    }

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    profile->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, true);

    if (disposition == OnboardingDisposition::kReplacePhoto) {
      base::RecordAction(
          base::UserMetricsAction("Indigo.ReplaceImage.Complete"));
    } else {
      base::RecordAction(base::UserMetricsAction("Indigo.Onboarding.Complete"));
    }

    if (!indigo_service_) {
      return;
    }

    if (disposition == OnboardingDisposition::kReplacePhoto) {
      OnRegenerate(toolbar_.get());
    } else {
      indigo_service_->GetCombinedEligibility(
          base::BindOnce(&IndigoPageActionController::ContinueInvoke,
                         invoke_weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void IndigoPageActionController::OnLocalEligibilityChanged(
    LocalEligibility state) {
  UpdateEntryPointsState();
}

void IndigoPageActionController::OnPageActionAnchoredMessageShown(
    const page_actions::PageActionState& page_action) {
  if (indigo_service_) {
    indigo_service_->AnchoredMessageShown();
  }
  base::RecordAction(
      base::UserMetricsAction("Indigo.PageAction.ShowAnchoredMessage"));
}

void IndigoPageActionController::OnOptimizationGuideDecision(
    const GURL& url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If the answer comes after another navigation, ignore it.
  if (!url.EqualsIgnoringRef(tab().GetContents()->GetLastCommittedURL())) {
    return;
  }
  optimization_guide_decision_ = decision;
  UpdateEntryPointsState();
}

views::View* IndigoPageActionController::GetIndigoOverlayView() const {
  if (!tab().IsVisible()) {
    return nullptr;
  }

  auto* browser_view =
      BrowserView::GetBrowserViewForBrowser(tab().GetBrowserWindowInterface());
  CHECK(browser_view);

  auto* contents_container =
      browser_view->GetContentsContainerViewFor(tab().GetContents());
  CHECK(contents_container);

  return contents_container->indigo_overlay_view();
}

void IndigoPageActionController::DestroyToolbar() {
  if (toolbar_) {
    toolbar_->Hide();
    toolbar_.reset();
  }
}

void IndigoPageActionController::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  content::RenderWidgetHost* new_widget =
      new_host ? new_host->GetWidget() : nullptr;
  RegisterObserverWithHost(new_widget);
}

void IndigoPageActionController::RegisterObserverWithHost(
    content::RenderWidgetHost* host) {
  if (current_host_ == host) {
    return;
  }
  UnregisterObserverFromHost(current_host_);
  current_host_ = host;
  if (current_host_) {
    current_host_->AddTrackedElementObserver(this);
  }
}

void IndigoPageActionController::UnregisterObserverFromHost(
    content::RenderWidgetHost* host) {
  if (host) {
    host->RemoveTrackedElementObserver(this);
    if (current_host_ == host) {
      current_host_ = nullptr;
    }
  }
}

void IndigoPageActionController::OnTrackedElementRectsChanged(
    const viz::TrackedElementRects& rects,
    float device_scale_factor) {
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  auto* manager =
      IndigoImageReplacementManager::GetForPage(web_contents->GetPrimaryPage());
  if (!manager) {
    return;
  }

  std::optional<base::Token> primary_token =
      manager->GetPrimaryTrackedElementId();
  if (!primary_token) {
    ClearTrackedBoundsAndHideToolbar();
    return;
  }

  const auto feature_id = static_cast<viz::TrackedElementFeature>(
      page_content_annotations::TrackedElementFeature::kIndigoImageReplacement);

  auto it = rects.find(feature_id);
  if (it == rects.end()) {
    ClearTrackedBoundsAndHideToolbar();
    return;
  }

  bool found = false;
  for (const auto& rect : it->second) {
    if (rect.id == *primary_token) {
      float dip_scale = 1.0f / device_scale_factor;
      tracked_bounds_ = gfx::ScaleToRoundedRect(rect.visible_bounds, dip_scale);
      if (toolbar_) {
        toolbar_->UpdateTrackedPosition(*tracked_bounds_);
      }
      found = true;
      break;
    }
  }
  if (!found) {
    ClearTrackedBoundsAndHideToolbar();
  }
}

void IndigoPageActionController::ClearTrackedBoundsAndHideToolbar() {
  tracked_bounds_ = std::nullopt;
  if (toolbar_) {
    toolbar_->UpdateTrackedPosition(gfx::Rect());
  }
}

}  // namespace indigo
