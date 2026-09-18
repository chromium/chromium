// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"
#include "chrome/browser/contextual_cueing/cueing_log.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/suggestions/glic_cue_tab_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/pdf/common/constants.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#endif

namespace glic {
namespace {

base::TimeDelta GetTimeSinceLastInvocation(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return base::TimeDelta::Max();
  }
  base::Time last_invoke_time =
      profile->GetPrefs()->GetTime(prefs::kGlicLastInvokedTime);
  if (last_invoke_time.is_null()) {
    return base::TimeDelta::Max();
  }
  return std::max(base::TimeDelta(), base::Time::Now() - last_invoke_time);
}

}  // namespace

// static
void GlicCueTarget::Register(tabs::TabInterface& tab) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED() << "Glic contextual cue not yet implemented for Android.";
#else
  auto* glic_keyed_service = GlicKeyedService::Get(tab.GetProfile());
  if (!glic_keyed_service) {
    return;
  }

  auto* contextual_cueing_controller =
      tab.GetTabFeatures()->contextual_cueing_controller();
  CHECK(contextual_cueing_controller);
  contextual_cueing_controller->RegisterCueTarget(
      contextual_cueing::CueTargetType::kGlic,
      std::make_unique<GlicCueTarget>(
          *glic_keyed_service,
          OptimizationGuideKeyedServiceFactory::GetForProfile(tab.GetProfile()),
          tab));
#endif
}

GlicCueTarget::GlicCueTarget(
    GlicKeyedService& glic_keyed_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    tabs::TabInterface& tab)
    : glic_keyed_service_(glic_keyed_service),
      optimization_guide_keyed_service_(optimization_guide_keyed_service),
      tab_(tab) {}

GlicCueTarget::~GlicCueTarget() = default;

contextual_cueing::CueTargetType GlicCueTarget::GetType() const {
  return contextual_cueing::CueTargetType::kGlic;
}

bool GlicCueTarget::RequiresModelExecution() const {
  return true;
}

void GlicCueTarget::CheckEligibility(
    base::WeakPtr<content::WebContents> web_contents,
    contextual_cueing::CueIntrusiveness intrusiveness,
    EligibilityCallback callback) {
  if (!web_contents) {
    CUEING_LOG("GlicCueTarget::CheckEligibility failed: WebContents gone.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, ContentGenerator()));
    return;
  }

  GlicCueTabState* cue_tab_state = GlicCueTabState::From(&tab_.get());
  if (!cue_tab_state) {
    CUEING_LOG("GlicCueTarget::CheckEligibility failed: No GlicCueTabState");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, ContentGenerator()));
    return;
  }
  cue_tab_state->CheckEligibility(intrusiveness, std::move(callback), this);
}

bool GlicCueTarget::IsPageEligible(
    const page_content_annotations::PageContentAnnotationsResult& result,
    content::WebContents* active_web_contents) const {
  if (!active_web_contents) {
    CUEING_LOG("GlicCueTarget::IsPageEligible failed: No active WebContents.");
    return false;
  }

  if (result.GetType() !=
      page_content_annotations::AnnotationType::kCategoryClassifier) {
    CUEING_LOG(
        "GlicCueTarget::IsPageEligible failed: invalid "
        "PageContentAnnotationsResult");
    return false;
  }

  bool passes_edu = false;
  bool passes_shopping = false;
  for (const page_content_annotations::Category& category :
       result.GetCategoryResults()) {
    if (category.category_type ==
            page_content_annotations::CategoryType::kEducation &&
        category.score > contextual_cueing::kEduClassifierThreshold.Get()) {
      passes_edu = true;
    }
    if (category.category_type ==
            page_content_annotations::CategoryType::kShopping &&
        category.score >
            contextual_cueing::kShoppingClassifierThreshold.Get()) {
      passes_shopping = true;
    }
  }

  CUEING_LOG(base::StringPrintf(
      "GlicCueTarget::IsPageEligible passes_edu=%d passes_shopping=%d",
      passes_edu, passes_shopping));

  if (contextual_cueing::kDiscardShoppingPdfs.Get() &&
      active_web_contents->GetContentsMimeType() == pdf::kPDFMimeType) {
    CUEING_LOG("GlicCueTarget::IsPageEligible discard shopping pdf");
    return passes_edu && !passes_shopping;
  }
  return passes_edu || passes_shopping;
}

bool GlicCueTarget::IsEligible() const {
  auto* window = tab_->GetBrowserWindowInterface();
  if (!window) {
    CUEING_LOG("GlicCueTarget::IsEligible failed: No window.");
    return false;
  }
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(tab_->GetProfile());
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kHistory)) {
    CUEING_LOG(
        "GlicCueTarget::IsEligible failed: No sync service or no history "
        "sync.");
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kGlicContextualCueV2ActiveUserBackoff)) {
    if (GetTimeSinceLastInvocation(tab_->GetProfile()) <
        base::Days(features::kMinDaysSinceLastInvocation.Get())) {
      CUEING_LOG(
          "GlicCueTarget::IsEligible failed: Time since last invocation is too "
          "short.");
      return false;
    }
  }
  return GlicEnabling::IsEnabledForProfile(tab_->GetProfile()) &&
         tab_->GetProfile()->GetPrefs()->GetBoolean(
             prefs::kGlicPinnedToTabstrip) &&
         !glic_keyed_service_->IsPanelShowingForBrowser(*window);
}

void GlicCueTarget::OnAnchoredMessageClicked(
    contextual_cueing::CueActionData data) {
  InvokeGlic(std::move(data), base::FeatureList::IsEnabled(
                                  features::kGlicContextualCueingV2AutoSubmit));
}

bool GlicCueTarget::SupportsEditPrompt() const {
  return true;
}

void GlicCueTarget::OnEditPrompt(contextual_cueing::CueActionData data) {
  InvokeGlic(std::move(data), /*should_autosubmit=*/false);
}

void GlicCueTarget::InvokeGlic(contextual_cueing::CueActionData data,
                               bool should_autosubmit) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED() << "Glic contextual cue not yet implemented for Android.";
#else
  if (!std::holds_alternative<contextual_cueing::GlicCueActionData>(data)) {
    return;
  }
  auto& glic_data = std::get<contextual_cueing::GlicCueActionData>(data);
  Target target(*tab_, NewConversation());
  GlicInvokeOptions options(
      std::move(target),
      glic::mojom::InvocationSource::kAutoOpenedByContextualCue);
  options.prompts.emplace_back(std::move(glic_data.prompt));

  CUEING_LOG(
      base::StringPrintf("Sharing %d tabs", glic_data.tabs_to_share.size()));
  options.tab_sharing = TabSharingOptions(std::move(glic_data.tabs_to_share),
                                          GlicPinTrigger::kContextualCue);

  if (should_autosubmit) {
    if (!GlicEnabling::HasConsentedForProfile(glic_keyed_service_->profile()) &&
        base::FeatureList::IsEnabled(
            features::kGlicMessageFirstFreForContextualCue)) {
      options.fre_override = mojom::FreOverride::kTrustFirstInline;
    }
    glic_keyed_service_->InvokeWithAutoSubmit(
        InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), std::move(options));
  } else {
    // If autosubmit is disabled, invoke with a prefilled prompt but don't
    // submit.
    glic_keyed_service_->Invoke(std::move(options));
  }
#endif
}

ui::ImageModel GlicCueTarget::GetAnchoredMessageIcon() const {
  gfx::ImageSkia* icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GLIC_BUTTON_ALT_ICON);
  return icon ? ui::ImageModel::FromImageSkia(*icon) : ui::ImageModel();
}

ui::ImageModel GlicCueTarget::GetOmniboxChipIcon() const {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED() << "Glic contextual cue not yet implemented for Android.";
  return ui::ImageModel();
#else
  return ui::ImageModel::FromVectorIcon(
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON),
      ui::kColorSysOnSurface, 16);
#endif
}

contextual_cueing::CueActionData GlicCueTarget::CueActionDataFromResponse(
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<tabs::TabHandle> tabs_to_show) const {
  contextual_cueing::GlicCueActionData data;
  if (!cue.has_gemini_in_chrome_surface()) {
    CUEING_LOG("Missing Gemini surface data.");
    return data;
  }
  if (cue.gemini_in_chrome_surface().prompt().empty()) {
    CUEING_LOG("Missing prompt in Gemini surface data.");
    return data;
  }
  data.prompt = cue.gemini_in_chrome_surface().prompt();
  data.tabs_to_share = std::move(tabs_to_show);
  return data;
}

optimization_guide::proto::ContextualCueingSurface GlicCueTarget::GetSurface()
    const {
  return optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_GEMINI_IN_CHROME;
}

}  // namespace glic
