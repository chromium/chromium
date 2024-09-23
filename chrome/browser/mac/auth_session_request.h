// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_AUTH_SESSION_REQUEST_H_
#define CHROME_BROWSER_MAC_AUTH_SESSION_REQUEST_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if defined(__OBJC__)

@class ASWebAuthenticationSessionRequest;

class Browser;
class Profile;

// A class to manage the WebContents running an
// ASWebAuthenticationSessionRequest.
class AuthSessionRequest
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AuthSessionRequest> {
 public:
  ~AuthSessionRequest() override;

  static void StartNewAuthSession(ASWebAuthenticationSessionRequest* request,
                                  Profile* profile);
  static void CancelAuthSession(ASWebAuthenticationSessionRequest* request);

  // Canonicalizes a scheme string. Returns nullopt if it is invalid.
  static std::optional<std::string> CanonicalizeScheme(std::string scheme);

  // Create a throttle for the ongoing authentication session.
  std::unique_ptr<content::NavigationThrottle> CreateThrottle(
      content::NavigationHandle* handle);

 private:
  friend class content::WebContentsUserData<AuthSessionRequest>;

  // Creates a AuthSessionRequest. `web_contents` is the WebContents to run,
  // `browser` is the browser window containing it, and `request` is the
  // `ASWebAuthenticationSessionRequest` being serviced.
  AuthSessionRequest(content::WebContents* web_contents,
                     Browser* browser,
                     ASWebAuthenticationSessionRequest* request,
                     std::string scheme);

  // Create a Browser and a WebContents to run the request.
  static Browser* CreateBrowser(ASWebAuthenticationSessionRequest* request,
                                Profile* profile);

  // Returns a map that holds all the authentication sessions that are in
  // progress. The keys are the stringified uuids of the authentication
  // requests, and the values are the AuthSessionRequests used to run the
  // session. The sessions are tab helpers and owned by the WebContentses, so
  // these pointers are weak.
  using UUIDToSessionRequestMap = std::map<std::string, AuthSessionRequest*>;
  static UUIDToSessionRequestMap& GetMap();

  // Silently destroys the WebContents. Note that this is triggering a self-
  // destruct on this object.
  void DestroyWebContents();

  // Cancel the ongoing request due to an incoming request from the OS.
  void CancelAuthSession();

  // A navigation to the watched scheme was found. The URL that was navigated to
  // is in `url`.
  void SchemeWasNavigatedTo(const GURL& url);

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  // Whether the cancellation callback should be made upon closing.
  bool perform_cancellation_callback_ = true;

  // The browser containing the WebContents being used to service the request.
  raw_ptr<Browser> browser_ = nullptr;

  // The request being serviced.
  ASWebAuthenticationSessionRequest* __strong request_;

  // The scheme being watched for, canonicalized.
  std::string scheme_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<AuthSessionRequest> weak_factory_{this};
};

#endif  // __OBJC__

// If there is an authentication session in progress for the given navigation
// handle, install a throttle.
std::unique_ptr<content::NavigationThrottle> MaybeCreateAuthSessionThrottleFor(
    content::NavigationHandle* handle);

#endif  // CHROME_BROWSER_MAC_AUTH_SESSION_REQUEST_H_
