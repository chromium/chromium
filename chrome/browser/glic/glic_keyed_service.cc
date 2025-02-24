// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service.h"

#include "base/containers/flat_set.h"
#include "chrome/browser/glic/auth_controller.h"
#include "chrome/browser/glic/border_view.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_page_context_fetcher.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_screenshot_capturer.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

GlicKeyedService::GlicKeyedService(Profile* profile,
                                   signin::IdentityManager* identity_manager,
                                   GlicProfileManager* profile_manager)
    : profile_(profile),
      configuration_(profile),
      window_controller_(
          std::make_unique<GlicWindowController>(profile,
                                                 identity_manager,
                                                 this)),
      focused_tab_manager_(profile, *window_controller_),
      screenshot_capturer_(std::make_unique<GlicScreenshotCapturer>()),
      auth_controller_(std::make_unique<AuthController>(profile,
                                                        identity_manager,
                                                        /*use_for_fre=*/false)),
      profile_manager_(profile_manager) {
  CHECK(GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(profile)));
  metrics_ = std::make_unique<GlicMetrics>(window_controller_.get());
}

GlicKeyedService::~GlicKeyedService() = default;

void GlicKeyedService::Shutdown() {
  window_controller_->Shutdown();
  SetContextAccessIndicator(false);
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  profile_manager_->SetActiveGlic(this);
  window_controller_->Toggle(bwi);
}

void GlicKeyedService::GuestAdded(content::WebContents* guest_contents) {
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);
  auto* page_handler = GetPageHandler(top);
  if (page_handler) {
    page_handler->GuestAdded(guest_contents);
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
  // TODO(crbug.com/393391681): This is a placeholder implementation. Implement
  // createTab() correctly. It should consider which window to use, and observe
  // the `open_in_background` flag. It should return actual data using the
  // callback.
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = open_in_background
                           ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  std::move(callback).Run(glic::mojom::TabData::New());
}

void GlicKeyedService::OpenGlicSettingsPage() {
  ::glic::OpenGlicSettingsPage(profile_);
}

void GlicKeyedService::ClosePanel() {
  window_controller_->Close();
  SetContextAccessIndicator(false);
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

void GlicKeyedService::ShowProfilePicker() {
  base::OnceCallback<void(Profile*)> callback =
      base::BindOnce([](Profile* profile) {
        if (profile) {
          GlicKeyedService* service =
              GlicKeyedServiceFactory::GetGlicKeyedService(profile);
          service->ToggleUI(nullptr);
        }
      });
  ProfilePicker::Show(
      ProfilePicker::Params::ForGlicManager(std::move(callback)));
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
  content::WebContents* web_contents = GetFocusedTab();
  if (!web_contents) {
    // TODO(crbug.com/379773651): Clean up logspam when it's no longer useful.
    LOG(ERROR) << "GetContextFromFocusedTab: No web contents";
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        mojom::GetTabContextErrorReason::kWebContentsChanged));
    return;
  }
  DCHECK(web_contents->GetPrimaryMainFrame());

  auto fetcher = std::make_unique<glic::GlicPageContextFetcher>();
  fetcher->Fetch(
      web_contents, options,
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

void GlicKeyedService::CaptureScreenshot(
    mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  screenshot_capturer_->CaptureScreenshot(
      window_controller_->GetGlicWidget()->GetNativeWindow(),
      std::move(callback));
}

content::WebContents* GlicKeyedService::GetFocusedTab() {
  return focused_tab_manager_.GetWebContentsForFocusedTab();
}

bool GlicKeyedService::IsContextAccessIndicatorShown(
    const content::WebContents* contents) {
  return is_context_access_indicator_enabled_ && GetFocusedTab() == contents;
}

void GlicKeyedService::WebClientCreated() {
  web_client_created_callbacks_.Notify();
}

base::CallbackListSubscription GlicKeyedService::AddWebClientCreatedCallback(
    base::OnceCallback<void()> callback) {
  return web_client_created_callbacks_.Add(std::move(callback));
}

void GlicKeyedService::TryPreload() {
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));
  if (!profile_manager_) {
    return;
  }
  Profile* profile = profile_;
  if (!profile_manager_->ShouldPreloadForProfile(profile)) {
    return;
  }

  window_controller_->Preload();
}

void GlicKeyedService::Reload() {
  window_controller().Reload();
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
