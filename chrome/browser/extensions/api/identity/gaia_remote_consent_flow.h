// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_REMOTE_CONSENT_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_REMOTE_CONSENT_FLOW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

namespace extensions {

class GaiaRemoteConsentFlow : public WebAuthFlow::Delegate {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum Failure {
    NONE = 0,
    WINDOW_CLOSED = 1,
    LOAD_FAILED = 2,
    // Deprecated:
    // SET_ACCOUNTS_IN_COOKIE_FAILED = 3,
    INVALID_CONSENT_RESULT = 4,
    NO_GRANT = 5,
    // Deprecated:
    // USER_NAVIGATED_AWAY = 6,
    CANNOT_CREATE_WINDOW = 7,
    kMaxValue = CANNOT_CREATE_WINDOW
  };

  class Delegate {
   public:
    virtual ~Delegate();
    // Called when the flow ends without getting the user consent.
    virtual void OnGaiaRemoteConsentFlowFailed(Failure failure) = 0;
    // Called when the user gives the approval via the OAuth2 remote consent
    // screen.
    virtual void OnGaiaRemoteConsentFlowApproved(
        const std::string& consent_result,
        const std::string& gaia_id) = 0;
  };

  GaiaRemoteConsentFlow(Delegate* delegate,
                        Profile* profile,
                        const ExtensionTokenKey& token_key,
                        const RemoteConsentResolutionData& resolution_data,
                        bool user_gesture);
  ~GaiaRemoteConsentFlow() override;

  GaiaRemoteConsentFlow(const GaiaRemoteConsentFlow& other) = delete;
  GaiaRemoteConsentFlow& operator=(const GaiaRemoteConsentFlow& other) = delete;

  // Starts the flow by setting accounts in cookie.
  void Start();
  void Stop();

  // Handles `consent_result` value when using either a Browser Tab or an App
  // Window to display the Auth page.
  void ReactToConsentResult(const std::string& consent_result);

  // WebAuthFlow::Delegate:
  void OnAuthFlowFailure(WebAuthFlow::Failure failure) override;
  void OnNavigationFinished(
      content::NavigationHandle* navigation_handle) override;

  void SetWebAuthFlowForTesting(std::unique_ptr<WebAuthFlow> web_auth_flow);
  WebAuthFlow* GetWebAuthFlowForTesting() const;

 private:
  void GaiaRemoteConsentFlowFailed(Failure failure);

  void DetachWebAuthFlow();

  network::mojom::CookieManager* GetCookieManagerForPartition();

  const raw_ptr<Delegate> delegate_;
  raw_ptr<Profile> profile_;
  const RemoteConsentResolutionData resolution_data_;
  const bool user_gesture_;

  std::unique_ptr<WebAuthFlow> web_flow_;
  bool web_flow_started_;

  base::WeakPtrFactory<GaiaRemoteConsentFlow> weak_factory{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_REMOTE_CONSENT_FLOW_H_
