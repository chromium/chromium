// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/mac/auth_session_request.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/apple/url_conversions.h"
#include "url/url_canon.h"

namespace {

// A navigation throttle that calls a closure when a navigation to a specified
// scheme is seen.
class AuthNavigationThrottle : public content::NavigationThrottle {
 public:
  using SchemeURLFoundCallback = base::OnceCallback<void(const GURL&)>;

  AuthNavigationThrottle(content::NavigationHandle* handle,
                         const std::string& scheme,
                         SchemeURLFoundCallback scheme_found)
      : content::NavigationThrottle(handle),
        scheme_(scheme),
        scheme_found_(std::move(scheme_found)) {
    DCHECK(!scheme_found_.is_null());
  }
  ~AuthNavigationThrottle() override = default;

  ThrottleCheckResult WillStartRequest() override { return HandleRequest(); }

  ThrottleCheckResult WillRedirectRequest() override { return HandleRequest(); }

  const char* GetNameForLogging() override { return "AuthNavigationThrottle"; }

 private:
  ThrottleCheckResult HandleRequest() {
    // Cancel any prerendering.
    if (!navigation_handle()->IsInPrimaryMainFrame()) {
      DCHECK(navigation_handle()->IsInPrerenderedMainFrame());
      return CANCEL_AND_IGNORE;
    }

    GURL url = navigation_handle()->GetURL();
    if (!url.SchemeIs(scheme_))
      return PROCEED;

    // Paranoia; if the callback was already fired, ignore all further
    // navigations that somehow get through before the WebContents deletion
    // happens.
    if (scheme_found_.is_null())
      return CANCEL_AND_IGNORE;

    // Post the callback; triggering the deletion of the WebContents that owns
    // the navigation that is in the middle of being throttled would likely not
    // be the best of ideas.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(scheme_found_), url));

    return CANCEL_AND_IGNORE;
  }

  // The scheme to watch for.
  std::string scheme_;

  // The closure to call once the scheme has been seen.
  SchemeURLFoundCallback scheme_found_;
};

}  // namespace

AuthSessionRequest::~AuthSessionRequest() {
  std::string uuid = base::SysNSStringToUTF8(request_.UUID.UUIDString);

  auto iter = GetMap().find(uuid);
  if (iter == GetMap().end())
    return;

  GetMap().erase(iter);
}

// static
void AuthSessionRequest::StartNewAuthSession(
    ASWebAuthenticationSessionRequest* request,
    Profile* profile) {
  NSString* error_string = nil;

  // Canonicalize the scheme so that it will compare correctly to the GURLs that
  // are visited later. Bail if it is invalid.
  NSString* raw_scheme = request.callbackURLScheme;
  std::optional<std::string> canonical_scheme =
      CanonicalizeScheme(base::SysNSStringToUTF8(raw_scheme));
  if (!canonical_scheme) {
    error_string =
        [NSString stringWithFormat:@"Scheme '%@' is not valid as per RFC 3986.",
                                   raw_scheme];
  }

  // Create a Browser with an empty tab.
  Browser* browser = nil;
  if (!error_string) {
    browser = CreateBrowser(request, profile);
    if (!browser) {
      error_string = @"Failed to create a WebContents to present the "
                     @"authorization session.";
    }
  }

  if (error_string) {
    // It's not clear what error to return here. -cancelWithError:'s
    // documentation says that it has to be an NSError with the domain as
    // specified below and a "suitable" ASWebAuthenticationSessionErrorCode, but
    // none of those codes really is good for "something went wrong while trying
    // to start the authentication session". PresentationContextInvalid will
    // have to do.
    NSError* error = [NSError
        errorWithDomain:ASWebAuthenticationSessionErrorDomain
                   code:
                       ASWebAuthenticationSessionErrorCodePresentationContextInvalid
               userInfo:@{NSDebugDescriptionErrorKey : error_string}];
    [request cancelWithError:error];
    return;
  }

  // Then create the auth session that owns that browser and will intercept
  // navigation requests.
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  AuthSessionRequest::CreateForWebContents(contents, browser, request,
                                           canonical_scheme.value());

  // Only then actually load the requested page, to make sure that if the very
  // first navigation is the one that authorizes the login, it's caught.
  // https://crbug.com/1195202
  contents->GetController().LoadURL(net::GURLWithNSURL(request.URL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_LINK, std::string());
}

// static
void AuthSessionRequest::CancelAuthSession(
    ASWebAuthenticationSessionRequest* request) {
  std::string uuid = base::SysNSStringToUTF8(request.UUID.UUIDString);

  auto iter = GetMap().find(uuid);
  if (iter == GetMap().end())
    return;

  iter->second->CancelAuthSession();
}

// static
std::optional<std::string> AuthSessionRequest::CanonicalizeScheme(
    std::string scheme) {
  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  bool result = url::CanonicalizeScheme(
      scheme.data(), url::Component(0, static_cast<int>(scheme.size())),
      &canon_output, &component);
  if (!result)
    return std::nullopt;

  return std::string(canon_output.data() + component.begin, component.len);
}

std::unique_ptr<content::NavigationThrottle> AuthSessionRequest::CreateThrottle(
    content::NavigationHandle* handle) {
  // Only attach a throttle to outermost main frames. Note non-primary main
  // frames will cancel the navigation in the throttle.
  switch (handle->GetNavigatingFrameType()) {
    case content::FrameType::kSubframe:
    case content::FrameType::kFencedFrameRoot:
      return nil;
    case content::FrameType::kPrimaryMainFrame:
    case content::FrameType::kPrerenderMainFrame:
      break;
  }

  auto scheme_found = base::BindOnce(&AuthSessionRequest::SchemeWasNavigatedTo,
                                     weak_factory_.GetWeakPtr());

  return std::make_unique<AuthNavigationThrottle>(handle, scheme_,
                                                  std::move(scheme_found));
}

AuthSessionRequest::AuthSessionRequest(
    content::WebContents* web_contents,
    Browser* browser,
    ASWebAuthenticationSessionRequest* request,
    std::string scheme)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AuthSessionRequest>(*web_contents),
      browser_(browser),
      request_(request),
      scheme_(scheme) {
  std::string uuid = base::SysNSStringToUTF8(request.UUID.UUIDString);
  GetMap()[uuid] = this;
}

// static
Browser* AuthSessionRequest::CreateBrowser(
    ASWebAuthenticationSessionRequest* request,
    Profile* profile) {
  if (!profile)
    return nullptr;

  bool ephemeral_sessions_allowed_by_policy =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
      policy::IncognitoModeAvailability::kDisabled;

  // As per the documentation for `shouldUseEphemeralSession`: "Whether the
  // request is honored depends on the user’s default web browser." If policy
  // does not allow for the use of an ephemeral session, the options would be
  // either to use a non-ephemeral session, or to error out. However, erroring
  // out would leave any app that uses `ASWebAuthenticationSession` unable to do
  // any sign-in at all via this API. Given that the docs do not actually
  // provide a guarantee of an ephemeral session if requested, take advantage of
  // that to not block the user's ability to sign in.
  if (request.shouldUseEphemeralSession &&
      ephemeral_sessions_allowed_by_policy) {
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
  if (!profile)
    return nullptr;

  // Note that this creates a popup-style window to do the signin. This is a
  // specific choice motivated by security concerns, and must *not* be changed
  // without consultation with the security team.
  //
  // The UX concern here is that an ordinary tab is not the right tool. This is
  // a magical WebContents that will dismiss itself when a valid login happens
  // within it, and so an ordinary tab can't be used as it invites a user to
  // navigate by putting a new URL or search into the omnibox. The location
  // information must be read-only.
  //
  // But the critical security concern is that the window *must have* a location
  // indication. This is an OS API for which UI needs to be created to allow the
  // user to log into a website by providing credentials. Chromium must provide
  // the user with an indication of where they are using the credentials.
  //
  // Having a location indicator that is present but read-only is satisfied with
  // a popup window. That must not be changed.
  //
  // Omit it from session restore as well. This is a special window for use by
  // this code; if it were restored it would not have the AuthSessionRequest and
  // would not behave correctly.

  Browser::CreateParams params(Browser::TYPE_POPUP, profile, true);
  params.omit_from_session_restore = true;
  Browser* browser = Browser::Create(params);
  chrome::AddTabAt(browser, GURL("about:blank"), -1, true);
  browser->window()->Show();

  return browser;
}

// static
AuthSessionRequest::UUIDToSessionRequestMap& AuthSessionRequest::GetMap() {
  static base::NoDestructor<UUIDToSessionRequestMap> map;
  return *map;
}

void AuthSessionRequest::DestroyWebContents() {
  // Detach the WebContents that owns this object from the tab strip. Because
  // the Browser is a TYPE_POPUP, there will only be one tab (tab index 0). This
  // will cause the browser window to dispose of itself once it realizes that it
  // has no tabs left. Close the tab this way (as opposed to, say,
  // TabStripModel::CloseWebContentsAt) so that the web page will no longer be
  // able to show any dialogs, particularly a `beforeunload` one.
  browser_->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  // The destruction of the WebContents triggers a call to
  // WebContentsDestroyed() below.
}

void AuthSessionRequest::CancelAuthSession() {
  // macOS has requested that this authentication session be canceled. Close the
  // browser window and call it a day.

  DestroyWebContents();
  // `DestroyWebContents` triggered the death of this object; perform no more
  // work.
}

void AuthSessionRequest::SchemeWasNavigatedTo(const GURL& url) {
  // Notify the OS that the authentication was successful, and provide the URL
  // that was navigated to.
  [request_ completeWithCallbackURL:net::NSURLWithGURL(url)];

  // This is a success, so no cancellation callback is needed.
  perform_cancellation_callback_ = false;

  // The authentication session is now complete, so close the browser window.
  DestroyWebContents();
  // `DestroyWebContents` triggered the death of this object; perform no more
  // work.
}

void AuthSessionRequest::WebContentsDestroyed() {
  // This function can be called through one of three code paths: one of a
  // successful login, and two of cancellation.
  //
  // Success code path:
  //
  // - The user successfully logged in, in which case the closure of the page
  //   was triggered above in `SchemeWasNavigatedTo()`.
  //
  // Cancellation code paths:
  //
  // - The user closed the window without successfully logging in.
  // - The OS asked for cancellation, in which case the closure of the page was
  //   triggered above in `CancelAuthSession()`.
  //
  // In both cancellation cases, the OS must receive a cancellation callback.
  // (This is an undocumented requirement in the case that the OS asked for the
  // cancellation; see https://crbug.com/1400714.)

  if (perform_cancellation_callback_) {
    NSError* error = [NSError
        errorWithDomain:ASWebAuthenticationSessionErrorDomain
                   code:ASWebAuthenticationSessionErrorCodeCanceledLogin
               userInfo:nil];
    [request_ cancelWithError:error];
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AuthSessionRequest);

std::unique_ptr<content::NavigationThrottle> MaybeCreateAuthSessionThrottleFor(
    content::NavigationHandle* handle) {
  AuthSessionRequest* request =
      AuthSessionRequest::FromWebContents(handle->GetWebContents());
  if (!request)
    return nullptr;

  return request->CreateThrottle(handle);
}
