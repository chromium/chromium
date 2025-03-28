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
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
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
#include "chrome/browser/glic/widget/glic_window_controller.h"
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
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

GlicKeyedService::GlicKeyedService(Profile* profile,
                                   signin::IdentityManager* identity_manager,
                                   ProfileManager* profile_manager,
                                   GlicProfileManager* glic_profile_manager)
    : profile_(profile),
      enabling_(std::make_unique<GlicEnabling>(
          profile,
          &profile_manager->GetProfileAttributesStorage())),
      metrics_(std::make_unique<GlicMetrics>(profile, enabling_.get())),
      window_controller_(
          std::make_unique<GlicWindowController>(profile,
                                                 identity_manager,
                                                 this,
                                                 enabling_.get())),
      focused_tab_manager_(profile, *window_controller_),
      screenshot_capturer_(std::make_unique<GlicScreenshotCapturer>()),
      auth_controller_(std::make_unique<AuthController>(profile,
                                                        identity_manager,
                                                        /*use_for_fre=*/false)),
      glic_profile_manager_(glic_profile_manager) {
  CHECK(GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(profile)));
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
  }
  // If automation is enabled do the opposite.
  if (command_line->HasSwitch(::switches::kGlicAutomation)) {
    profile_->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kCompleted));
  }

  if (base::FeatureList::IsEnabled(features::kGlicActor)) {
    actor_controller_ = std::make_unique<GlicActorController>();
  }

  // This is only used by automation for tests.
  glic_profile_manager_->MaybeAutoOpenGlicPanel();
}

GlicKeyedService::~GlicKeyedService() {
  metrics_->SetControllers(nullptr, nullptr);
}

// static
GlicKeyedService* GlicKeyedService::Get(content::BrowserContext* context) {
  return GlicKeyedServiceFactory::GetGlicKeyedService(context);
}

void GlicKeyedService::Shutdown() {
  CloseUI();
  glic_profile_manager_->OnServiceShutdown(this);
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi,
                                bool prevent_close,
                                mojom::InvocationSource source) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  glic_profile_manager_->SetActiveGlic(this);
  window_controller_->Toggle(bwi, prevent_close, source);
}

void GlicKeyedService::CloseUI() {
  window_controller_->Shutdown();
  SetContextAccessIndicator(false);
}

void GlicKeyedService::FocusUI() {
  window_controller_->FocusIfOpen();
}

void GlicKeyedService::GuestAdded(content::WebContents* guest_contents) {
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);

  auto* glic_web_contents = window_controller().GetWebContents();
  if (glic_web_contents) {
    blink::web_pref::WebPreferences prefs(top->GetOrCreateWebPreferences());
    prefs.default_font_size =
        glic_web_contents->GetOrCreateWebPreferences().default_font_size;
    top->SetWebPreferences(prefs);
  }
  auto* page_handler = GetPageHandler(top);
  if (page_handler) {
    auto* webview = extensions::WebViewGuest::FromWebContents(guest_contents);
    CHECK(webview);
    page_handler->GuestAdded(webview);
  }
}

void GlicKeyedService::PageHandlerAdded(GlicPageHandler* page_handler) {
  page_handlers_.insert(page_handler);
}

void GlicKeyedService::PageHandlerRemoved(GlicPageHandler* page_handler) {
  page_handlers_.erase(page_handler);
}

bool GlicKeyedService::IsWindowShowing() const {
  return window_controller_->IsShowing();
}

bool GlicKeyedService::IsWindowDetached() const {
  return window_controller_->IsDetached();
}

void GlicKeyedService::NotifyWindowIntentToShow() {
  for (auto& handler : page_handlers_) {
    handler->NotifyWindowIntentToShow();
  }
}

GlicPageHandler* GlicKeyedService::GetPageHandler(
    const content::WebContents* webui_contents) {
  for (GlicPageHandler* page_handler : page_handlers_) {
    if (page_handler->webui_contents() == webui_contents) {
      return page_handler;
    }
  }
  return nullptr;
}

base::CallbackListSubscription GlicKeyedService::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabChangedCallback(callback);
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

  auto fetcher = std::make_unique<glic::GlicPageContextFetcher>();
  fetcher->Fetch(
      GetFocusedTabData(), options,
      base::BindOnce(
          // Bind `fetcher` to the callback to keep it in scope until it
          // returns.
          // TODO(harringtond): Consider adding throttling of how often we fetch
          // context.
          // TODO(harringtond): Consider deleting the fetcher if the page
          // handler is unbound before the fetch completes.
          [](std::unique_ptr<glic::GlicPageContextFetcher> fetcher,
             mojom::WebClientHandler::GetContextFromFocusedTabCallback callback,
             mojom::GetContextResultPtr result) {
            std::move(callback).Run(std::move(result));
          },
          std::move(fetcher), std::move(callback)));
}

void GlicKeyedService::ActInFocusedTab(
    const std::vector<uint8_t>& action_proto,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
    mojom::ActInFocusedTabResultPtr result =
        mojom::ActInFocusedTabResult::NewErrorReason(
            mojom::ActInFocusedTabErrorReason::kUnknown);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return;
  }

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

void GlicKeyedService::WebClientCreated() {
  web_client_created_callbacks_.Notify();
}

base::CallbackListSubscription GlicKeyedService::AddWebClientCreatedCallback(
    base::OnceCallback<void()> callback) {
  return web_client_created_callbacks_.Add(std::move(callback));
}

void GlicKeyedService::TryPreload() {
  CHECK(glic_profile_manager_);

  Profile* profile = profile_;
  if (!glic_profile_manager_->ShouldPreloadForProfile(profile)) {
    return;
  }

  window_controller_->Preload();
}

void GlicKeyedService::TryPreloadFre() {
  CHECK(glic_profile_manager_);

  Profile* profile = profile_;
  if (!glic_profile_manager_->ShouldPreloadFreForProfile(profile)) {
    return;
  }

  window_controller_->PreloadFre();
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
  return contents == window_controller().GetWebContents() ||
         contents == window_controller().GetFreWebContents();
}

}  // namespace glic
