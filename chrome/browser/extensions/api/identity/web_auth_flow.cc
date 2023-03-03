// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/identity_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "crypto/random.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::RenderViewHost;
using content::WebContents;
using content::WebContentsObserver;
using guest_view::GuestViewBase;

namespace extensions {

namespace {

// Returns whether `partition` should be persisted on disk.
bool ShouldPersistStorage(WebAuthFlow::Partition partition) {
  switch (partition) {
    case WebAuthFlow::LAUNCH_WEB_AUTH_FLOW:
      return base::FeatureList::IsEnabled(kPersistentStorageForWebAuthFlow);
    case WebAuthFlow::GET_AUTH_TOKEN:
      return false;
  }

  NOTREACHED() << "Unexpected partition value " << partition;
  return false;
}

// Returns a unique identifier of the storage partition corresponding to
// `partition`.
std::string GetStoragePartitionId(WebAuthFlow::Partition partition) {
  switch (partition) {
    case WebAuthFlow::LAUNCH_WEB_AUTH_FLOW:
      return "launchWebAuthFlow";
    case WebAuthFlow::GET_AUTH_TOKEN:
      return "getAuthFlow";
  }

  NOTREACHED() << "Unexpected partition value " << partition;
  return std::string();
}

// Returns a partition name suitable to use in the `webview.partition`
// parameter.
std::string GetPartitionNameForWebView(WebAuthFlow::Partition partition) {
  std::string persist_prefix =
      ShouldPersistStorage(partition) ? "persist:" : "";
  return persist_prefix + GetStoragePartitionId(partition);
}
}  // namespace

namespace identity_private = api::identity_private;

BASE_FEATURE(kPersistentStorageForWebAuthFlow,
             "PersistentStorageForWebAuthFlow",
             base::FEATURE_DISABLED_BY_DEFAULT);

WebAuthFlow::WebAuthFlow(
    Delegate* delegate,
    Profile* profile,
    const GURL& provider_url,
    Mode mode,
    Partition partition,
    AbortOnLoad abort_on_load_for_non_interactive,
    absl::optional<base::TimeDelta> timeout_for_non_interactive)
    : delegate_(delegate),
      profile_(profile),
      provider_url_(provider_url),
      mode_(mode),
      partition_(partition),
      abort_on_load_for_non_interactive_(abort_on_load_for_non_interactive),
      timeout_for_non_interactive_(timeout_for_non_interactive),
      non_interactive_timeout_timer_(std::make_unique<base::OneShotTimer>()) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("identity", "WebAuthFlow", this);
  if (timeout_for_non_interactive_) {
    DCHECK_GE(*timeout_for_non_interactive_, base::TimeDelta());
    DCHECK_LE(*timeout_for_non_interactive_, base::Minutes(1));
  }
}

WebAuthFlow::~WebAuthFlow() {
  DCHECK(!delegate_);

  if (using_auth_with_browser_tab_ && web_contents()) {
    web_contents()->Close();
  }

  CloseInfoBar();

  // Stop listening to notifications first since some of the code
  // below may generate notifications.
  WebContentsObserver::Observe(nullptr);

  if (!app_window_key_.empty()) {
    AppWindowRegistry::Get(profile_)->RemoveObserver(this);

    if (app_window_ && app_window_->web_contents())
      app_window_->web_contents()->Close();
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0("identity", "WebAuthFlow", this);
}

void WebAuthFlow::SetClockForTesting(
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  non_interactive_timeout_timer_ =
      std::make_unique<base::OneShotTimer>(tick_clock);
  non_interactive_timeout_timer_->SetTaskRunner(task_runner);
}

void WebAuthFlow::Start() {
  DCHECK(profile_);
  DCHECK(!profile_->IsOffTheRecord());

  if (base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab)) {
    using_auth_with_browser_tab_ = true;

    content::WebContents::CreateParams params(profile_);
    web_contents_ = content::WebContents::Create(params);
    WebContentsObserver::Observe(web_contents_.get());

    content::NavigationController::LoadURLParams load_params(provider_url_);
    web_contents_->GetController().LoadURLWithParams(load_params);

    MaybeStartTimeout();
    return;
  }

  AppWindowRegistry::Get(profile_)->AddObserver(this);

  // Attach a random ID string to the window so we can recognize it
  // in OnAppWindowAdded.
  std::string random_bytes;
  crypto::RandBytes(base::WriteInto(&random_bytes, 33), 32);
  base::Base64Encode(random_bytes, &app_window_key_);

  // identityPrivate.onWebFlowRequest(app_window_key, provider_url_, mode_)
  base::Value::List args;
  args.Append(app_window_key_);
  args.Append(provider_url_.spec());
  if (mode_ == WebAuthFlow::INTERACTIVE)
    args.Append("interactive");
  else
    args.Append("silent");
  args.Append(GetPartitionNameForWebView(partition_));

  auto event =
      std::make_unique<Event>(events::IDENTITY_PRIVATE_ON_WEB_FLOW_REQUEST,
                              identity_private::OnWebFlowRequest::kEventName,
                              std::move(args), profile_);
  ExtensionSystem* system = ExtensionSystem::Get(profile_);

  extensions::ComponentLoader* component_loader =
      system->extension_service()->component_loader();
  if (!component_loader->Exists(extension_misc::kIdentityApiUiAppId)) {
    component_loader->Add(
        IDR_IDENTITY_API_SCOPE_APPROVAL_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("identity_scope_approval_dialog")));
  }

  EventRouter::Get(profile_)->DispatchEventWithLazyListener(
      extension_misc::kIdentityApiUiAppId, std::move(event));

  MaybeStartTimeout();
}

void WebAuthFlow::DetachDelegateAndDelete() {
  delegate_ = nullptr;
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

content::StoragePartition* WebAuthFlow::GetGuestPartition() {
  // When using the Auth through the Browser Tab, the guest partition shouldn't
  // be used, consider using `Profile::GetDefaultStoragePartition()` instead.
  if (base::FeatureList::IsEnabled(features::kWebAuthFlowInBrowserTab)) {
    return nullptr;
  }

  return profile_->GetStoragePartition(
      GetWebViewPartitionConfig(partition_, profile_));
}

const std::string& WebAuthFlow::GetAppWindowKey() const {
  return app_window_key_;
}

// static
content::StoragePartitionConfig WebAuthFlow::GetWebViewPartitionConfig(
    Partition partition,
    content::BrowserContext* browser_context) {
  // This has to mirror the logic in WebViewGuest::CreateWebContents for
  // creating the correct StoragePartitionConfig.
  auto result = content::StoragePartitionConfig::Create(
      browser_context, extension_misc::kIdentityApiUiAppId,
      GetStoragePartitionId(partition),
      /*in_memory=*/!ShouldPersistStorage(partition));
  result.set_fallback_to_partition_domain_for_blob_urls(
      browser_context->IsOffTheRecord()
          ? content::StoragePartitionConfig::FallbackMode::
                kFallbackPartitionInMemory
          : content::StoragePartitionConfig::FallbackMode::
                kFallbackPartitionOnDisk);
  return result;
}

void WebAuthFlow::OnAppWindowAdded(AppWindow* app_window) {
  if (app_window->window_key() == app_window_key_ &&
      app_window->extension_id() == extension_misc::kIdentityApiUiAppId) {
    app_window_ = app_window;
    WebContentsObserver::Observe(app_window->web_contents());
  }
}

void WebAuthFlow::OnAppWindowRemoved(AppWindow* app_window) {
  if (app_window->window_key() == app_window_key_ &&
      app_window->extension_id() == extension_misc::kIdentityApiUiAppId) {
    app_window_ = nullptr;
    WebContentsObserver::Observe(nullptr);

    if (delegate_)
      delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
  }
}

bool WebAuthFlow::IsObservingProviderWebContents() const {
  return web_contents() &&
         (embedded_window_created_ || using_auth_with_browser_tab_);
}

void WebAuthFlow::DisplayInfoBar() {
  DCHECK(web_contents());
  DCHECK(using_auth_with_browser_tab_);

  info_bar_delegate_ = WebAuthFlowInfoBarDelegate::Create(
      web_contents(), info_bar_parameters_.extension_display_name);
}

void WebAuthFlow::CloseInfoBar() {
  if (info_bar_delegate_) {
    info_bar_delegate_->CloseInfoBar();
  }
}

bool WebAuthFlow::IsDisplayingAuthPageInTab() const {
  // If web_contents_ is nullptr, then the auth page tab is opened.
  return using_auth_with_browser_tab_ && !web_contents_;
}

void WebAuthFlow::BeforeUrlLoaded(const GURL& url) {
  if (delegate_ && IsObservingProviderWebContents()) {
    delegate_->OnAuthFlowURLChange(url);
  }
}

void WebAuthFlow::AfterUrlLoaded() {
  initial_url_loaded_ = true;
  if (delegate_ && IsObservingProviderWebContents() &&
      mode_ == WebAuthFlow::SILENT) {
    if (abort_on_load_for_non_interactive_ == AbortOnLoad::kYes) {
      non_interactive_timeout_timer_->Stop();
      delegate_->OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED);
    } else {
      // Wait for timeout.
    }
    return;
  }

  // If `web_contents_` is nullptr, this means that the interactive tab has
  // already been opened once.
  if (delegate_ && using_auth_with_browser_tab_ && web_contents_ &&
      mode_ == WebAuthFlow::INTERACTIVE) {
    switch (features::kWebAuthFlowInBrowserTabMode.Get()) {
      case features::WebAuthFlowInBrowserTabMode::kNewTab: {
        // Displays the auth page in a new tab attached to an existing/new
        // browser.
        chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile_);
        NavigateParams params(browser_displayer.browser(),
                              std::move(web_contents_));
        Navigate(&params);
        break;
      }
      case features::WebAuthFlowInBrowserTabMode::kPopupWindow: {
        // Displays the auth page in a browser popup window.
        NavigateParams params(profile_, GURL(),
                              ui::PageTransition::PAGE_TRANSITION_FIRST);
        params.contents_to_insert = std::move(web_contents_);
        params.disposition = WindowOpenDisposition::NEW_POPUP;
        Navigate(&params);
        break;
      }
    }

    if (info_bar_parameters_.should_show) {
      DisplayInfoBar();
    }
  }
}

void WebAuthFlow::MaybeStartTimeout() {
  if (mode_ != WebAuthFlow::SILENT) {
    // Only applies to non-interactive flows.
    return;
  }
  if (abort_on_load_for_non_interactive_ == AbortOnLoad::kYes &&
      !timeout_for_non_interactive_) {
    // Preserve previous behaviour: no timeout if aborting on load and timeout
    // value is not specified.
    return;
  }
  // `base::Unretained(this)` is safe because `this` owns
  // `non_interactive_timeout_timer_`.
  non_interactive_timeout_timer_->Start(
      FROM_HERE,
      timeout_for_non_interactive_.value_or(kNonInteractiveMaxTimeout),
      base::BindOnce(&WebAuthFlow::OnTimeout, base::Unretained(this)));
}

void WebAuthFlow::OnTimeout() {
  if (delegate_) {
    delegate_->OnAuthFlowFailure(initial_url_loaded_
                                     ? WebAuthFlow::INTERACTION_REQUIRED
                                     : WebAuthFlow::TIMED_OUT);
  }
}

void WebAuthFlow::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  DCHECK(app_window_);

  if (!delegate_ || embedded_window_created_)
    return;

  // Switch from watching the app window to the guest inside it.
  embedded_window_created_ = true;
  WebContentsObserver::Observe(inner_web_contents);
}

void WebAuthFlow::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // When in `using_auth_with_browser_tab_` mode,
  // `WebAuthFlow::WebContentsDestroyed()` takes care of this flow.
  if (delegate_ && !using_auth_with_browser_tab_)
    delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
}

void WebAuthFlow::WebContentsDestroyed() {
  WebContentsObserver::Observe(nullptr);
  if (delegate_) {
    delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
  }
}

void WebAuthFlow::TitleWasSet(content::NavigationEntry* entry) {
  if (delegate_)
    delegate_->OnAuthFlowTitleChange(base::UTF16ToUTF8(entry->GetTitle()));
}

void WebAuthFlow::DidStopLoading() {
  AfterUrlLoaded();
}

void WebAuthFlow::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the navigation is initiated by the user, the tab will exit the auth
  // flow screen, this should result in a declined authentication and deleting
  // the flow.
  if (IsDisplayingAuthPageInTab() &&
      !navigation_handle->IsRendererInitiated()) {
    // Stop observing the web contents since it is not part of the flow anymore.
    WebContentsObserver::Observe(nullptr);
    delegate_->OnAuthFlowFailure(Failure::USER_NAVIGATED_AWAY);
    return;
  }

  if (navigation_handle->IsInPrimaryMainFrame()) {
    BeforeUrlLoaded(navigation_handle->GetURL());
  }
}

void WebAuthFlow::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    BeforeUrlLoaded(navigation_handle->GetURL());
  }
}

void WebAuthFlow::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Websites may create and remove <iframe> during the auth flow. In
  // particular, to integrate CAPTCHA tests. Chrome shouldn't abort the auth
  // flow if a navigation failed in a sub-frame. https://crbug.com/1049565.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  if (delegate_) {
    delegate_->OnNavigationFinished(navigation_handle);
  }

  bool failed = false;
  if (navigation_handle->GetNetErrorCode() != net::OK) {
    if (navigation_handle->GetURL().spec() == url::kAboutBlankURL) {
      // As part of the OAUth 2.0 protocol with GAIA, at the end of the web
      // authorization flow, GAIA redirects to a custom scheme URL of type
      // |com.googleusercontent.apps.123:/<extension_id>|, where
      // |com.googleusercontent.apps.123| is the reverse DNS notation of the
      // client ID of the extension that started the web sign-in flow. (The
      // intent of this weird URL scheme was to make sure it couldn't be loaded
      // anywhere at all as this makes it much harder to pull off a cross-site
      // attack that could leak the returned oauth token to a malicious script
      // or site.)
      //
      // This URL is not an accessible URL from within a Guest WebView, so
      // during its load of this URL, Chrome changes it to |about:blank| and
      // then the Identity Scope Approval Dialog extension fails to load it.
      // Failing to load |about:blank| must not be treated as a failure of
      // the web auth flow.
      DCHECK_EQ(net::ERR_UNKNOWN_URL_SCHEME,
                navigation_handle->GetNetErrorCode());
    } else if (navigation_handle->GetResponseHeaders() &&
               navigation_handle->GetResponseHeaders()->response_code() ==
                   net::HTTP_NO_CONTENT) {
      // Navigation to no content URLs is aborted but shouldn't be treated as a
      // failure.
      // In particular, Gaia navigates to a no content page to pass Mirror
      // response headers.
    } else {
      failed = true;
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "identity", "DidFinishNavigationFailure", this, "error_code",
          navigation_handle->GetNetErrorCode());
    }
  } else if (navigation_handle->GetResponseHeaders() &&
             navigation_handle->GetResponseHeaders()->response_code() >= 400) {
    failed = true;
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
        "identity", "DidFinishNavigationFailure", this, "response_code",
        navigation_handle->GetResponseHeaders()->response_code());
  }

  if (failed && delegate_) {
    delegate_->OnAuthFlowFailure(LOAD_FAILED);
  }
}

void WebAuthFlow::SetShouldShowInfoBar(
    const std::string& extension_display_name) {
  info_bar_parameters_.should_show = true;
  info_bar_parameters_.extension_display_name = extension_display_name;
}

base::WeakPtr<WebAuthFlowInfoBarDelegate>
WebAuthFlow::GetInfoBarDelegateForTesting() {
  return info_bar_delegate_;
}

}  // namespace extensions
