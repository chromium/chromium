// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::WebContents;
using content::WebContentsObserver;

namespace extensions {

WebAuthFlow::WebAuthFlow(
    Delegate* delegate,
    Profile* profile,
    const GURL& provider_url,
    Mode mode,
    bool user_gesture,
    AbortOnLoad abort_on_load_for_non_interactive,
    std::optional<base::TimeDelta> timeout_for_non_interactive,
    std::optional<gfx::Rect> popup_bounds)
    : delegate_(delegate),
      profile_(profile),
      provider_url_(provider_url),
      mode_(mode),
      user_gesture_(user_gesture),
      abort_on_load_for_non_interactive_(abort_on_load_for_non_interactive),
      timeout_for_non_interactive_(timeout_for_non_interactive),
      non_interactive_timeout_timer_(std::make_unique<base::OneShotTimer>()),
      popup_bounds_(popup_bounds) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("identity", "WebAuthFlow", this);
  if (timeout_for_non_interactive_) {
    DCHECK_GE(*timeout_for_non_interactive_, base::TimeDelta());
    DCHECK_LE(*timeout_for_non_interactive_, base::Minutes(1));
  }
}

WebAuthFlow::~WebAuthFlow() {
  DCHECK(!delegate_);

  if (web_contents()) {
    web_contents()->Close();
  }

  CloseInfoBar();

  // Stop listening to notifications first since some of the code
  // below may generate notifications.
  WebContentsObserver::Observe(nullptr);

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

  content::WebContents::CreateParams params(profile_);
  web_contents_ = content::WebContents::Create(params);
  WebContentsObserver::Observe(web_contents_.get());

  content::NavigationController::LoadURLParams load_params(provider_url_);
  web_contents_->GetController().LoadURLWithParams(load_params);

  MaybeStartTimeout();
}

void WebAuthFlow::DetachDelegateAndDelete() {
  delegate_ = nullptr;
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void WebAuthFlow::Stop() {
  if (web_contents()) {
    web_contents()->Close();
  }
  WebContentsObserver::Observe(nullptr);
  web_contents_.reset();
  delegate_ = nullptr;
  profile_ = nullptr;
}

void WebAuthFlow::DisplayInfoBar() {
  DCHECK(web_contents());

  info_bar_delegate_ = WebAuthFlowInfoBarDelegate::Create(
      web_contents(), info_bar_parameters_.extension_display_name);
}

void WebAuthFlow::CloseInfoBar() {
  if (info_bar_delegate_) {
    info_bar_delegate_->CloseInfoBar();
  }
}

bool WebAuthFlow::DisplayAuthPageInPopupWindow() {
  if (Browser::GetCreationStatusForProfile(profile_) !=
      Browser::CreationStatus::kOk) {
    return false;
  }

  Browser::CreateParams browser_params(Browser::TYPE_POPUP, profile_,
                                       user_gesture_);
  browser_params.omit_from_session_restore = true;
  browser_params.should_trigger_session_restore = false;
  if (popup_bounds_.has_value()) {
    browser_params.initial_bounds = popup_bounds_.value();
  }

  Browser* browser = Browser::Create(browser_params);
  browser->tab_strip_model()->AddWebContents(
      std::move(web_contents_), /*index=*/0,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      AddTabTypes::ADD_ACTIVE);

  browser->window()->Show();
  return true;
}

void WebAuthFlow::BeforeUrlLoaded(const GURL& url) {
  if (delegate_) {
    delegate_->OnAuthFlowURLChange(url);
  }
}

void WebAuthFlow::AfterUrlLoaded() {
  initial_url_loaded_ = true;
  if (delegate_ && mode_ == WebAuthFlow::SILENT) {
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
  if (delegate_ && web_contents_ && mode_ == WebAuthFlow::INTERACTIVE) {
    bool is_auth_page_displayed = DisplayAuthPageInPopupWindow();
    if (!is_auth_page_displayed) {
      delegate_->OnAuthFlowFailure(WebAuthFlow::Failure::CANNOT_CREATE_WINDOW);
      return;
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
