// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/ui_util.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#else
static_assert(BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS));
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
  TRACE_EVENT_BEGIN(
      "identity", "WebAuthFlow",
      perfetto::NamedTrack::FromPointer("extensions::WebAuthFlow", this));
  if (timeout_for_non_interactive_) {
    DCHECK_GE(*timeout_for_non_interactive_, base::TimeDelta());
    DCHECK_LE(*timeout_for_non_interactive_, base::Minutes(1));
  }

  // profile_ can be null in unit tests.
  if (profile_ != nullptr) {
    profile_observation_.Observe(profile_);
  }
}

WebAuthFlow::~WebAuthFlow() {
  DCHECK(!delegate_);
  BrowserWindowInterface* popup_browser =
      web_contents()
          ? browser_window_util::GetBrowserForTabContents(*web_contents())
          : nullptr;
  if (popup_browser) {
    popup_browser->GetWindow()->Close();
  } else if (web_contents()) {
    // Explicitly close `web_contents()` if it's not displayed in any browser
    // window.
    web_contents()->Close();
  }

  CloseInfoBar();

  // Stop listening to notifications first since some of the code
  // below may generate notifications.
  WebContentsObserver::Observe(nullptr);

  TRACE_EVENT_END("identity", perfetto::NamedTrack::FromPointer(
                                  "extensions::WebAuthFlow", this));
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
  DCHECK(!profile_->ShutdownStarted());

  content::WebContents::CreateParams params(profile_);
  web_contents_ = content::WebContents::Create(params);
  WebContentsObserver::Observe(web_contents_.get());

  content::NavigationController::LoadURLParams load_params(provider_url_);
  web_contents_->GetController().LoadURLWithParams(load_params);

  MaybeStartTimeout();
}

void WebAuthFlow::DetachDelegateAndDelete() {
  delegate_ = nullptr;

  // WebAuthFlow must be destroyed asynchronously to avoid reentrancy issues.
  //
  // WebAuthFlow observes WebContents and notifies its delegate from within
  // WebContentsObserver callbacks. The delegate may call
  // DetachDelegateAndDelete() in response.
  //
  // If WebAuthFlow is destroyed synchronously during such a callback, it would
  // synchronously destroy its owned WebContents. However, WebContents cannot be
  // destroyed while it's in the middle of notifying observers — doing so
  // triggers a CHECK().
  //
  // Therefore, destruction of WebAuthFlow must be deferred to avoid violating
  // this constraint. If the Profile is destroyed before the async destruction
  // runs, WebAuthFlow will be notified via OnProfileWillBeDestroyed, and the
  // WebContents will be explicitly destroyed at that point, ensuring they do
  // not outlive the Profile.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

// static
void WebAuthFlow::RegisterInfoBar(
    infobars::BrowserInfoBarManager& infobar_manager) {
  auto spec =
      infobars::InfoBarSpec::Builder(
          infobars::InfoBarDelegate::EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE)
          .SetMessageTextTemplate(l10n_util::GetStringUTF16(
              IDS_EXTENSION_LAUNCH_WEB_AUTH_FLOW_TAB_INFO_BAR_TEXT))
          .SetScope(infobars::InfoBarScope::kTab)
          .SetExpireOnNavigation(false)
          .Build();
  infobar_manager.Register(std::move(spec));
}

void WebAuthFlow::DisplayInfoBar() {
  DCHECK(web_contents());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::
              EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE)) {
    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    CHECK(browser_infobar_manager);
    auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents());
    if (tab) {
      infobars::InfoBarShowParams params;
      params.substitutions = {MessageSubstitution(
          ui_util::GetFixupExtensionNameForUIDisplay(
              info_bar_parameters_.extension_display_name),
          /*is_link=*/false, /*accessible_name=*/std::nullopt)};
      browser_infobar_manager->Show(
          tab,
          infobars::InfoBarDelegate::EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE,
          std::move(params));
    }
    return;
  }
#endif

  info_bar_delegate_ = WebAuthFlowInfoBarDelegate::Create(
      web_contents(), info_bar_parameters_.extension_display_name);
}

void WebAuthFlow::CloseInfoBar() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::
              EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE)) {
    if (web_contents()) {
      auto* browser_infobar_manager =
          infobars::BrowserInfoBarManager::From(g_browser_process);
      CHECK(browser_infobar_manager);
      browser_infobar_manager->Hide(
          web_contents(),
          infobars::InfoBarDelegate::EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE);
    }
    return;
  }
#endif

  if (info_bar_delegate_) {
    info_bar_delegate_->CloseInfoBar();
  }
}

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
void WebAuthFlow::OnBrowserWindowInterfaceInitialized(
    BrowserWindowInterface* browser) {
  if (!browser) {
    delegate_->OnAuthFlowFailure(WebAuthFlow::Failure::CANNOT_CREATE_WINDOW);
    return;
  }

  TabModel* tab_model =
      TabModelList::FindTabModelWithWindowSessionId(browser->GetSessionID());
  tab_model->CreateTab(
      TabAndroid::FromWebContents(tab_model->GetActiveWebContents()),
      std::move(web_contents_), TabModel::kInvalidIndex,
      TabModel::TabLaunchType::FROM_RECENT_TABS_FOREGROUND,
      /*should_pin=*/false);

  if (popup_displayed_callback_for_testing_) {
    std::move(popup_displayed_callback_for_testing_).Run();
  }
}

void WebAuthFlow::SetPopupDisplayedCallbackForTesting(
    base::OnceClosure callback) {
  popup_displayed_callback_for_testing_ = std::move(callback);
}
#endif

bool WebAuthFlow::DisplayAuthPageInPopupWindow() {
  if (GetBrowserWindowCreationStatusForProfile(*profile_) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    return false;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  BrowserWindowCreateParams browser_params(BrowserWindowInterface::TYPE_POPUP,
                                           profile_, user_gesture_);
  browser_params.omit_from_session_restore = true;
  browser_params.should_trigger_session_restore = false;
  if (popup_bounds_.has_value()) {
    browser_params.initial_bounds = popup_bounds_.value();
  }

  BrowserWindowInterface* browser =
      CreateBrowserWindow(std::move(browser_params));
  browser->GetTabStripModel()->AddWebContents(
      std::move(web_contents_), /*index=*/0,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      AddTabTypes::ADD_ACTIVE);

  browser->GetWindow()->Show();
#else
  static_assert(BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS));
  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_POPUP,
                                   *profile_, user_gesture_);
  if (popup_bounds_.has_value()) {
    params.initial_bounds = popup_bounds_.value();
  }

  base::OnceCallback<void(BrowserWindowInterface*)> callback =
      base::BindOnce(&WebAuthFlow::OnBrowserWindowInterfaceInitialized,
                     weak_factory_.GetWeakPtr());
  CreateBrowserWindow(std::move(params), std::move(callback));
#endif

  return true;
}

void WebAuthFlow::BeforeUrlLoaded(const GURL& url) {
  if (delegate_) {
    delegate_->OnAuthFlowURLChange(url);
  }
}

void WebAuthFlow::AfterUrlLoaded() {
  CHECK(profile_);
  if (profile_->ShutdownStarted()) {
    // Don't process further if the profile is being deleted. The pending
    // extension functions will be aborted during KeyedService shutdown.
    return;
  }

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
  if (delegate_) {
    delegate_->OnAuthFlowTitleChange(base::UTF16ToUTF8(entry->GetTitle()));
  }
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
  CHECK(profile_);
  if (profile_->ShutdownStarted()) {
    // Don't process further if the profile is being deleted. The pending
    // extension functions will be aborted during KeyedService shutdown.
    return;
  }

  // Websites may create and remove <iframe> during the auth flow. In
  // particular, to integrate CAPTCHA tests. Chrome shouldn't abort the auth
  // flow if a navigation failed in a sub-frame. https://crbug.com/40672617.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

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
      TRACE_EVENT_INSTANT(
          "identity", "DidFinishNavigationFailure",
          perfetto::NamedTrack::FromPointer("extensions::WebAuthFlow", this),
          "error_code", navigation_handle->GetNetErrorCode());
    }
  } else if (navigation_handle->GetResponseHeaders() &&
             navigation_handle->GetResponseHeaders()->response_code() >= 400) {
    failed = true;
    TRACE_EVENT_INSTANT(
        "identity", "DidFinishNavigationFailure",
        perfetto::NamedTrack::FromPointer("extensions::WebAuthFlow", this),
        "response_code",
        navigation_handle->GetResponseHeaders()->response_code());
  }

  if (failed && delegate_) {
    delegate_->OnAuthFlowFailure(LOAD_FAILED);
  }
}

void WebAuthFlow::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile, profile_);
  profile_observation_.Reset();

  // Null out the delegate early so that we do not call into it while
  // WebContents are being destroyed. It would be cleaner to send a "profile
  // destroyed" notification to the delegate, but all the current delegates
  // already observe Profile destruction, so we can just be silent here.
  delegate_ = nullptr;

  BrowserWindowInterface* popup_browser =
      web_contents()
          ? browser_window_util::GetBrowserForTabContents(*web_contents())
          : nullptr;
  if (popup_browser) {
    popup_browser->GetWindow()->Close();
  } else if (web_contents()) {
    web_contents()->Close();
  }

  WebContentsObserver::Observe(nullptr);
  web_contents_.reset();
  profile_ = nullptr;
}

void WebAuthFlow::SetShouldShowInfoBar(
    const std::string& extension_display_name) {
  info_bar_parameters_.should_show = true;
  info_bar_parameters_.extension_display_name = extension_display_name;
}

}  // namespace extensions
