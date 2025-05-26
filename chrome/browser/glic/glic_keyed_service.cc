// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic_actor_controller.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

namespace {

base::TimeDelta GetWarmingDelay() {
  base::TimeDelta delay_start =
      base::Milliseconds(features::kGlicWarmingDelayMs.Get());
  base::TimeDelta delay_limit =
      delay_start + base::Milliseconds(features::kGlicWarmingJitterMs.Get());
  if (delay_limit > delay_start) {
    return RandTimeDelta(delay_start, delay_limit);
  }
  return delay_start;
}

}  // namespace

GlicKeyedService::GlicKeyedService(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    ProfileManager* profile_manager,
    GlicProfileManager* glic_profile_manager,
    contextual_cueing::ContextualCueingService* contextual_cueing_service)
    : profile_(profile),
      enabling_(std::make_unique<GlicEnabling>(
          profile,
          &profile_manager->GetProfileAttributesStorage())),
      metrics_(std::make_unique<GlicMetrics>(profile, enabling_.get())),
      host_(std::make_unique<Host>(profile)),
      window_controller_(
          std::make_unique<GlicWindowControllerImpl>(profile,
                                                     identity_manager,
                                                     this,
                                                     enabling_.get())),
      focused_tab_manager_(profile, *window_controller_),
      screenshot_capturer_(std::make_unique<GlicScreenshotCapturer>()),
      auth_controller_(std::make_unique<AuthController>(profile,
                                                        identity_manager,
                                                        /*use_for_fre=*/false)),
      contextual_cueing_service_(contextual_cueing_service) {
  CHECK(GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(profile)));
  host_->Initialize(window_controller_.get());
  metrics_->SetControllers(window_controller_.get(), &focused_tab_manager_);

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&GlicKeyedService::OnMemoryPressure,
                                     weak_ptr_factory_.GetWeakPtr()));

  // If `--glic-always-open-fre` is present, unset this pref to ensure the FRE
  // is shown for testing convenience.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAlwaysOpenFre)) {
    profile_->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kNotStarted));
    // or if automation is enabled, skip FRE
  } else if (command_line->HasSwitch(::switches::kGlicAutomation)) {
    profile_->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kCompleted));
  }

  if (base::FeatureList::IsEnabled(features::kGlicActor)) {
    actor_controller_ = std::make_unique<GlicActorController>(profile_);
  }

  // This is only used by automation for tests.
  glic_profile_manager->MaybeAutoOpenGlicPanel();
}

GlicKeyedService::~GlicKeyedService() {
  host().Destroy();
  metrics_->SetControllers(nullptr, nullptr);
}

// static
GlicKeyedService* GlicKeyedService::Get(content::BrowserContext* context) {
  return GlicKeyedServiceFactory::GetGlicKeyedService(context);
}

void GlicKeyedService::Shutdown() {
  CloseUI();
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->OnServiceShutdown(this);
  }
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi,
                                bool prevent_close,
                                mojom::InvocationSource source) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->SetActiveGlic(this);
  }
  window_controller_->Toggle(bwi, prevent_close, source);
}

void GlicKeyedService::OpenFreDialogInNewTab(BrowserWindowInterface* bwi) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->SetActiveGlic(this);
  }
  window_controller_->fre_controller()->OpenFreDialogInNewTab(bwi);
}

void GlicKeyedService::CloseUI() {
  window_controller_->Shutdown();
  host().Shutdown();
  SetContextAccessIndicator(false);
}

void GlicKeyedService::PrepareForOpen() {
  window_controller_->fre_controller()->MaybePreconnect();

  auto* active_web_contents = GetFocusedTabData().focus();
  if (contextual_cueing_service_ && active_web_contents) {
    contextual_cueing_service_
        ->PrepareToFetchContextualGlicZeroStateSuggestions(active_web_contents);
  }
}

void GlicKeyedService::OnZeroStateSuggestionsFetched(
    mojom::ZeroStateSuggestionsPtr suggestions,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback,
    std::optional<std::vector<std::string>> returned_suggestions) {
  std::vector<mojom::SuggestionContentPtr> output_suggestions;
  if (returned_suggestions) {
    for (const std::string& suggestion_string : returned_suggestions.value()) {
      output_suggestions.push_back(
          mojom::SuggestionContent::New(suggestion_string));
    }
    suggestions->suggestions = std::move(output_suggestions);
  }

  std::move(callback).Run(std::move(suggestions));
}

void GlicKeyedService::FetchZeroStateSuggestions(
    bool is_first_run,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback) {
  auto* active_web_contents = GetFocusedTabData().focus();

  if (contextual_cueing_service_ && active_web_contents && IsWindowShowing()) {
    auto suggestions = mojom::ZeroStateSuggestions::New();
    suggestions->tab_id = GetTabId(active_web_contents);
    suggestions->tab_url = active_web_contents->GetLastCommittedURL();
    contextual_cueing_service_->GetContextualGlicZeroStateSuggestions(
        active_web_contents, is_first_run,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&GlicKeyedService::OnZeroStateSuggestionsFetched,
                           GetWeakPtr(), std::move(suggestions),
                           std::move(callback)),
            std::nullopt));

  } else {
    std::move(callback).Run(nullptr);
  }
}

GlicWindowController& GlicKeyedService::window_controller() {
  CHECK(window_controller_);
  return *window_controller_.get();
}

void GlicKeyedService::GuestAdded(content::WebContents* guest_contents) {
  host().GuestAdded(guest_contents);
}

bool GlicKeyedService::IsWindowShowing() const {
  return window_controller_->IsShowing();
}

bool GlicKeyedService::IsWindowDetached() const {
  return window_controller_->IsDetached();
}

base::CallbackListSubscription GlicKeyedService::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabChangedCallback(callback);
}

base::CallbackListSubscription
GlicKeyedService::AddFocusedTabInstanceChangedCallback(
    FocusedTabInstanceChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabInstanceChangedCallback(callback);
}

base::CallbackListSubscription
GlicKeyedService::AddFocusedTabOrCandidateInstanceChangedCallback(
    FocusedTabOrCandidateInstanceChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabOrCandidateInstanceChangedCallback(
      callback);
}

base::CallbackListSubscription
GlicKeyedService::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabDataChangedCallback(callback);
}

base::CallbackListSubscription
GlicKeyedService::AddContextAccessIndicatorStatusChangedCallback(
    ContextAccessIndicatorChangedCallback callback) {
  return context_access_indicator_callback_list_.Add(std::move(callback));
}

void GlicKeyedService::CreateTab(
    const ::GURL& url,
    bool open_in_background,
    const std::optional<int32_t>& window_id,
    glic::mojom::WebClientHandler::CreateTabCallback callback) {
  // If we need to open other URL types, it should be done in a more specific
  // function.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(nullptr);
    return;
  }
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = open_in_background
                           ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&params);
  if (!navigation_handle.get()) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Right after requesting the navigation, the WebContents will have almost no
  // information to populate TabData, hence the overriding of the URL. Should we
  // ever want to send more data back to the web client, we should wait until
  // the navigation commits.
  mojom::TabDataPtr tab_data =
      CreateTabData(navigation_handle.get()->GetWebContents());
  if (tab_data) {
    tab_data->url = url;
  }
  std::move(callback).Run(std::move(tab_data));
}

void GlicKeyedService::ClosePanel() {
  window_controller_->Close();
  SetContextAccessIndicator(false);
  screenshot_capturer_->CloseScreenPicker();
}

void GlicKeyedService::AttachPanel() {
  window_controller_->Attach();
}

void GlicKeyedService::DetachPanel() {
  window_controller_->Detach();
}

void GlicKeyedService::ResizePanel(const gfx::Size& size,
                                   base::TimeDelta duration,
                                   base::OnceClosure callback) {
  window_controller_->Resize(size, duration, std::move(callback));
}

void GlicKeyedService::SetPanelDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  window_controller_->SetDraggableAreas(draggable_areas);
}

void GlicKeyedService::SetContextAccessIndicator(bool show) {
  if (is_context_access_indicator_enabled_ == show) {
    return;
  }
  is_context_access_indicator_enabled_ = show;
  context_access_indicator_callback_list_.Notify(show);
}

void GlicKeyedService::GetContextFromFocusedTab(
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kGlicTabContextEnabled) ||
      !window_controller_->IsShowing()) {
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        std::string("permission denied")));
    return;
  }

  metrics_->DidRequestContextFromFocusedTab();

  FetchPageContext(GetFocusedTabData(), options, std::move(callback));
}

void GlicKeyedService::ActInFocusedTab(
    const std::vector<uint8_t>& action_proto,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));

  optimization_guide::proto::BrowserAction action;
  if (!action.ParseFromArray(action_proto.data(), action_proto.size())) {
    mojom::ActInFocusedTabResultPtr result =
        mojom::ActInFocusedTabResult::NewErrorReason(
            mojom::ActInFocusedTabErrorReason::kInvalidActionProto);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return;
  }

  CHECK(actor_controller_);
  actor_controller_->Act(GetFocusedTabData(), action, options,
                         std::move(callback));
}

void GlicKeyedService::StopActorTask(actor::TaskId task_id) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(actor_controller_);
  actor_controller_->StopTask(task_id);
}

void GlicKeyedService::PauseActorTask(actor::TaskId task_id) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(actor_controller_);
  actor_controller_->PauseTask(task_id);
}

void GlicKeyedService::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(actor_controller_);
  actor_controller_->ResumeTask(task_id, context_options, std::move(callback));
}

bool GlicKeyedService::IsActorCoordinatorActingOnTab(
    const content::WebContents* tab) const {
  return actor_controller_ &&
         actor_controller_->IsActorCoordinatorActingOnTab(tab);
}

actor::ActorCoordinator& GlicKeyedService::GetActorCoordinatorForTesting() {
  CHECK(actor_controller_);
  return actor_controller_->GetActorCoordinatorForTesting();  // IN-TEST
}

void GlicKeyedService::CaptureScreenshot(
    mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  screenshot_capturer_->CaptureScreenshot(
      window_controller_->GetGlicWidget()->GetNativeWindow(),
      std::move(callback));
}

FocusedTabData GlicKeyedService::GetFocusedTabData() {
  return focused_tab_manager_.GetFocusedTabData();
}

bool GlicKeyedService::IsContextAccessIndicatorShown(
    const content::WebContents* contents) {
  return is_context_access_indicator_enabled_ &&
         GetFocusedTabData().focus() == contents;
}

void GlicKeyedService::TryPreload() {
  if (base::FeatureList::IsEnabled(features::kGlicDisableWarming) &&
      !base::FeatureList::IsEnabled(features::kGlicWarming)) {
    return;
  }
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  CHECK(glic_profile_manager);
  base::TimeDelta delay = GetWarmingDelay();

  // TODO(b/411100559): Ideally we'd use post delayed task in all cases,
  // but this requires a refactor of tests that are currently brittle. For now,
  // just synchronously call ShouldPreloadForProfile if there is no delay.
  if (delay.is_zero()) {
    glic_profile_manager->ShouldPreloadForProfile(
        profile_,
        base::BindOnce(&GlicKeyedService::FinishPreload, GetWeakPtr()));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &GlicProfileManager::ShouldPreloadForProfile,
            glic_profile_manager->GetWeakPtr(), profile_,
            base::BindOnce(&GlicKeyedService::FinishPreload, GetWeakPtr())),
        delay);
  }
}

void GlicKeyedService::TryPreloadFre() {
  if (base::FeatureList::IsEnabled(features::kGlicDisableWarming) &&
      !base::FeatureList::IsEnabled(features::kGlicFreWarming)) {
    return;
  }
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  CHECK(glic_profile_manager);

  glic_profile_manager->ShouldPreloadFreForProfile(
      profile_,
      base::BindOnce(&GlicKeyedService::FinishPreloadFre, GetWeakPtr()));
}

void GlicKeyedService::Reload() {
  window_controller().Reload();
}

void GlicKeyedService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MemoryPressureLevel::
                   MEMORY_PRESSURE_LEVEL_NONE ||
      (this == GlicProfileManager::GetInstance()->GetLastActiveGlic())) {
    return;
  }

  CloseUI();
}

base::WeakPtr<GlicKeyedService> GlicKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool GlicKeyedService::IsActiveWebContents(content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  return contents == host().webui_contents() ||
         contents == window_controller().GetFreWebContents();
}

void GlicKeyedService::FinishPreload(Profile* profile, bool should_preload) {
  if (base::FeatureList::IsEnabled(features::kGlicWarming) && profile &&
      GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
    base::UmaHistogramBoolean("Glic.ShouldPreload", should_preload);
  }

  if (!should_preload) {
    return;
  }

  window_controller_->Preload();
}

void GlicKeyedService::FinishPreloadFre(Profile* profile, bool should_preload) {
  if (!should_preload) {
    return;
  }

  window_controller_->PreloadFre();
}

bool GlicKeyedService::IsProcessHostForGlic(
    content::RenderProcessHost* process_host) {
  auto* fre_contents = window_controller_->GetFreWebContents();
  if (fre_contents) {
    if (fre_contents->GetPrimaryMainFrame()->GetProcess() == process_host) {
      return true;
    }
  }
  return host().IsGlicWebUiHost(process_host);
}

bool GlicKeyedService::IsGlicWebUi(content::WebContents* web_contents) {
  return host().IsGlicWebUi(web_contents);
}

}  // namespace glic
