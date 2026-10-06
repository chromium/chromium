// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_cue_target.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/cueing_log.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_search/contextual_search_cue_tab_state.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/pdf/common/constants.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

namespace contextual_search {

// static
void ContextualSearchCueTarget::Register(tabs::TabInterface& tab) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED()
      << "Contextual search contextual cue not yet implemented for Android.";
#else
  auto* contextual_cueing_controller =
      tab.GetTabFeatures()->contextual_cueing_controller();
  CHECK(contextual_cueing_controller);
  contextual_cueing_controller->RegisterCueTarget(
      contextual_cueing::CueTargetType::kContextualSearch,
      std::make_unique<ContextualSearchCueTarget>(
          OptimizationGuideKeyedServiceFactory::GetForProfile(tab.GetProfile()),
          tab));
#endif
}

ContextualSearchCueTarget::ContextualSearchCueTarget(
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    tabs::TabInterface& tab)
    : optimization_guide_keyed_service_(optimization_guide_keyed_service),
      tab_(tab) {}

ContextualSearchCueTarget::~ContextualSearchCueTarget() = default;

contextual_cueing::CueTargetType ContextualSearchCueTarget::GetType() const {
  return contextual_cueing::CueTargetType::kContextualSearch;
}

bool ContextualSearchCueTarget::RequiresModelExecution() const {
  return true;
}

void ContextualSearchCueTarget::CheckEligibility(
    base::WeakPtr<content::WebContents> web_contents,
    contextual_cueing::CueIntrusiveness intrusiveness,
    EligibilityCallback callback) {
  if (!web_contents) {
    CUEING_LOG(
        "ContextualSearchCueTarget::CheckEligibility failed: WebContents "
        "gone.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, ContentGenerator()));
    return;
  }

  ContextualSearchCueTabState* cue_tab_state =
      ContextualSearchCueTabState::From(&tab_.get());
  if (!cue_tab_state) {
    CUEING_LOG(
        "ContextualSearchCueTarget::CheckEligibility failed: No "
        "ContextualSearchCueTabState");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, ContentGenerator()));
    return;
  }
  cue_tab_state->CheckEligibility(intrusiveness, std::move(callback),
                                  GetWeakPtr());
}

bool ContextualSearchCueTarget::IsPageEligible(
    const page_content_annotations::PageContentAnnotationsResult& result,
    content::WebContents* active_web_contents) const {
  if (!active_web_contents) {
    CUEING_LOG(
        "ContextualSearchCueTarget::IsPageEligible failed: No active "
        "WebContents.");
    return false;
  }

  if (result.GetType() !=
      page_content_annotations::AnnotationType::kCategoryClassifier) {
    CUEING_LOG(
        "ContextualSearchCueTarget::IsPageEligible failed: invalid "
        "PageContentAnnotationsResult");
    return false;
  }

  bool passes_edu = false;
  bool passes_shopping = false;
  for (const page_content_annotations::Category& category :
       result.GetCategoryResults()) {
    if (base::FeatureList::IsEnabled(
            contextual_tasks::kContextualSearchContextualCuesHandleEdu) &&
        category.category_type ==
            page_content_annotations::CategoryType::kEducation &&
        category.score > contextual_cueing::kEduClassifierThreshold.Get()) {
      passes_edu = true;
    }
    if (base::FeatureList::IsEnabled(
            contextual_tasks::kContextualSearchContextualCuesHandleShopping) &&
        category.category_type ==
            page_content_annotations::CategoryType::kShopping &&
        category.score >
            contextual_cueing::kShoppingClassifierThreshold.Get()) {
      passes_shopping = true;
    }
  }

  CUEING_LOG(base::StringPrintf(
      "ContextualSearchCueTarget::IsPageEligible passes_edu=%d "
      "passes_shopping=%d",
      passes_edu, passes_shopping));

  if (contextual_cueing::kDiscardShoppingPdfs.Get() &&
      active_web_contents->GetContentsMimeType() == pdf::kPDFMimeType) {
    CUEING_LOG(
        "ContextualSearchCueTarget::IsPageEligible discard shopping pdf");
    return passes_edu && !passes_shopping;
  }
  return passes_edu || passes_shopping;
}

bool ContextualSearchCueTarget::IsEligible() const {
  auto* window = tab_->GetBrowserWindowInterface();
  if (!window) {
    CUEING_LOG("ContextualSearchCueTarget::IsEligible failed: No window.");
    return false;
  }
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(tab_->GetProfile());
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kHistory)) {
    CUEING_LOG(
        "ContextualSearchCueTarget::IsEligible failed: No sync service or no "
        "history sync.");
    return false;
  }
  if (!contextual_tasks::EntryPointEligibilityManager::IsEligible(
          tab_->GetProfile())) {
    CUEING_LOG(
        "ContextualSearchCueTarget::IsEligible failed: Entry points not "
        "eligible.");
    return false;
  }
  auto* panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(window);
  if (panel_controller && panel_controller->IsPanelOpenForContextualTask()) {
    CUEING_LOG(
        "ContextualSearchCueTarget::IsEligible failed: Panel already open.");
    return false;
  }
  return true;
}

void ContextualSearchCueTarget::OnAnchoredMessageClicked(
    contextual_cueing::CueActionData data) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED()
      << "Contextual search contextual cue not yet implemented for Android.";
#else
  if (!std::holds_alternative<contextual_cueing::ContextualSearchCueActionData>(
          data)) {
    return;
  }
  auto& cs_data =
      std::get<contextual_cueing::ContextualSearchCueActionData>(data);
  if (cs_data.query.empty()) {
    return;
  }

  Profile* profile = tab_->GetProfile();
  BrowserWindowInterface* window = tab_->GetBrowserWindowInterface();
  if (!profile || !window) {
    return;
  }

  auto* service = ContextualSearchServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  auto config_params =
      std::make_unique<ContextualSearchContextController::ConfigParams>();
  session_handle_ = service->CreateSession(
      std::move(config_params), ContextualSearchSource::kContextualTasks,
      lens::LensOverlayInvocationSource::kAppMenu);
  if (!session_handle_) {
    return;
  }
  session_handle_->NotifySessionStarted();
  session_handle_->CheckSearchContentSharingSettings(profile->GetPrefs());

  if (!query_contextualizer_) {
    auto get_session_callback =
        base::BindRepeating(&ContextualSearchCueTarget::GetSessionHandle,
                            base::Unretained(this));
    auto get_viewport_options_callback = base::BindRepeating(
        []() -> std::optional<lens::ImageEncodingOptions> {
          return std::nullopt;
        });
    query_contextualizer_delegate_ =
        std::make_unique<contextual_tasks::DesktopQueryContextualizerDelegate>(
            std::move(get_session_callback),
            std::move(get_viewport_options_callback),
            contextual_tasks::ContextualTasksContextServiceFactory::
                GetForProfile(profile),
            base::BindRepeating(
                [](tabs::TabInterface* tab) -> BrowserWindowInterface* {
                  return tab->GetBrowserWindowInterface();
                },
                base::Unretained(&tab_.get())));
    query_contextualizer_ =
        std::make_unique<contextual_tasks::QueryContextualizer>(
            contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
                profile),
            query_contextualizer_delegate_.get());
  }

  contextual_tasks::QueryContextualizer::ContextualizeParams params;
  params.task_id = std::nullopt;
  params.query_text = cs_data.query;
  for (const auto& handle : cs_data.tabs_to_share) {
    params.tabs_for_contextual_searchbox_first_turn.push_back(
        handle.raw_value());
  }
  params.on_ineligible_callback = base::DoNothing();
  params.on_processed_callback = base::DoNothing();
  auto on_contextualized_callback =
      base::BindOnce(&ContextualSearchCueTarget::OnContextualizationComplete,
                     weak_ptr_factory_.GetWeakPtr(), cs_data.query);
  if (contextual_tasks::GetIsContextualTasksNonBlockingUrlNavigationEnabled()) {
    params.on_uploads_started_callback = std::move(on_contextualized_callback);
    params.complete_callback = base::DoNothing();
  } else {
    params.complete_callback = std::move(on_contextualized_callback);
  }
  params.enable_smart_tab_selection = false;
  query_contextualizer_->Contextualize(std::move(params));
#endif
}

ContextualSearchSessionHandle* ContextualSearchCueTarget::GetSessionHandle() {
  return session_handle_.get();
}

void ContextualSearchCueTarget::OnContextualizationComplete(
    std::string query,
    base::WeakPtr<ContextualSearchSessionHandle> session_handle) {
  auto* active_session_handle = session_handle.get();
  if (!active_session_handle) {
    active_session_handle = session_handle_.get();
  }
  if (!active_session_handle) {
    return;
  }

  auto request_info = std::make_unique<
      ContextualSearchContextController::CreateSearchUrlRequestInfo>();
  request_info->query_text = std::move(query);
  request_info->query_start_time = base::Time::Now();
  request_info->search_url_type =
      ContextualSearchContextController::SearchUrlType::kAim;
  request_info->invocation_source = lens::LensOverlayInvocationSource::kAppMenu;

  active_session_handle->CreateSearchUrl(
      std::move(request_info),
      base::BindOnce(&ContextualSearchCueTarget::OpenContextualTasksSidePanel,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContextualSearchCueTarget::OpenContextualTasksSidePanel(GURL url) {
  Profile* profile = tab_->GetProfile();
  BrowserWindowInterface* window = tab_->GetBrowserWindowInterface();
  if (!profile || !window) {
    return;
  }

  auto* ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile);
  if (!ui_service) {
    return;
  }

  ui_service->StartTaskUiInSidePanel(window, &tab_.get(), url,
                                     std::move(session_handle_));
}

bool ContextualSearchCueTarget::SupportsEditPrompt() const {
  // TODO(crbug.com/568812937): Support edit prompt.
  return false;
}

void ContextualSearchCueTarget::OnEditPrompt(
    contextual_cueing::CueActionData data) {
  NOTREACHED();
}

ui::ImageModel ContextualSearchCueTarget::GetAnchoredMessageIcon() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (gfx::ImageSkia* icon =
          ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_GOOGLE_G_GRADIENT_16_ALT)) {
    return ui::ImageModel::FromImageSkia(*icon);
  }
#endif
  return GetOmniboxChipIcon();
}

ui::ImageModel ContextualSearchCueTarget::GetOmniboxChipIcon() const {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED()
      << "Contextual search contextual cue not yet implemented for Android.";
  return ui::ImageModel();
#else

  const gfx::VectorIcon& icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      vector_icons::kGoogleGLogoIcon;
#else
      vector_icons::kSearchIcon;
#endif

  return ui::ImageModel::FromVectorIcon(icon, ui::kColorSysOnSurface, 16);
#endif
}

contextual_cueing::CueActionData
ContextualSearchCueTarget::CueActionDataFromResponse(
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<tabs::TabHandle> tabs_to_show) const {
  contextual_cueing::ContextualSearchCueActionData data;
  if (!cue.has_contextual_search_surface()) {
    CUEING_LOG("Missing ContextualSearch surface data.");
    return data;
  }
  if (cue.contextual_search_surface().query().empty()) {
    CUEING_LOG("Missing query in ContextualSearch surface data.");
    return data;
  }
  data.query = cue.contextual_search_surface().query();
  data.tabs_to_share = std::move(tabs_to_show);
  return data;
}

optimization_guide::proto::ContextualCueingSurface
ContextualSearchCueTarget::GetSurface() const {
  return optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_CONTEXTUAL_SEARCH;
}

}  // namespace contextual_search
