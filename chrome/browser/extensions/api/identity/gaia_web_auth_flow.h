// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_WEB_AUTH_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_WEB_AUTH_FLOW_H_

#include "base/macros.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class UbertokenFetcher;
}

namespace extensions {

// Implements a web-based OAuth2 scope approval dialog. This flow has
// four parts:
// 1. Fetch an ubertoken for a signed-in user.
// 2. Use the ubertoken to get session cookies using MergeSession.
// 3. Start the OAuth flow and wait for final redirect.
// 4. Parse results from the fragment component of the final redirect URI.
//
// The OAuth flow is a special version of the OAuth2 out-of-band flow
// where the final response page's title contains the
// redirect_uri. The redirect URI has an unusual format to prevent its
// use in other contexts. The scheme of the URI is a reversed version
// of the OAuth client ID, and the path starts with the Chrome
// extension ID. For example, an app with the OAuth client ID
// "32610281651.apps.googleusercontent.com" and a Chrome app ID
// "kbinjhdkhikmpjoejcfofghmjjpidcnj", would get redirected to:
//
// com.googleusercontent.apps.32610281651:/kbinjhdkhikmpjoejcfofghmjjpidcnj
//
// Arriving at this URI completes the flow. The last response from
// gaia does a JavaScript redirect to the special URI, but also
// includes the same URI in its title. The navigation to this URI gets
// filtered out because of its unusual protocol scheme, so
// GaiaWebAuthFlow pulls it out of the window title instead.

class GaiaWebAuthFlow : public WebAuthFlow::Delegate {
 public:
  enum Failure {
    WINDOW_CLOSED,  // Window closed by user.
    INVALID_REDIRECT,  // Redirect parse error.
    SERVICE_AUTH_ERROR,  // Non-OAuth related authentication error
    OAUTH_ERROR,  // Flow reached final redirect, which contained an error.
    LOAD_FAILED  // An auth flow page failed to load.
  };

  class Delegate {
   public:
    // Called when the flow fails prior to the final OAuth redirect,
    // TODO(courage): LOAD_FAILURE descriptions?
    virtual void OnGaiaFlowFailure(Failure failure,
                                   GoogleServiceAuthError service_error,
                                   const std::string& oauth_error) = 0;
    // Called when the OAuth2 flow completes.
    virtual void OnGaiaFlowCompleted(const std::string& access_token,
                                     const std::string& expiration) = 0;
  };

  GaiaWebAuthFlow(Delegate* delegate,
                  Profile* profile,
                  const ExtensionTokenKey* token_key,
                  const std::string& oauth2_client_id,
                  const std::string& locale);
  ~GaiaWebAuthFlow() override;

  // Starts the flow by fetching an ubertoken. Can override for testing.
  virtual void Start();

  // Ubertoken fetch completion callback.
  void OnUbertokenFetchComplete(GoogleServiceAuthError error,
                                const std::string& token);

  // WebAuthFlow::Delegate implementation.
  void OnAuthFlowFailure(WebAuthFlow::Failure failure) override;
  void OnAuthFlowURLChange(const GURL& redirect_url) override;
  void OnAuthFlowTitleChange(const std::string& title) override;

 private:
  // Creates a WebAuthFlow, which will navigate to |url|. Can override
  // for testing. Used to kick off the MergeSession (step #2).
  virtual std::unique_ptr<WebAuthFlow> CreateWebAuthFlow(GURL url);

  Delegate* delegate_;
  Profile* profile_;
  CoreAccountId account_id_;
  std::string redirect_scheme_;
  std::string redirect_path_prefix_;
  GURL auth_url_;
  std::unique_ptr<signin::UbertokenFetcher> ubertoken_fetcher_;
  std::unique_ptr<WebAuthFlow> web_flow_;

  DISALLOW_COPY_AND_ASSIGN(GaiaWebAuthFlow);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_WEB_AUTH_FLOW_H_
