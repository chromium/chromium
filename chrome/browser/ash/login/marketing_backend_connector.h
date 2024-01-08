// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_MARKETING_BACKEND_CONNECTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_MARKETING_BACKEND_CONNECTOR_H_

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ash {

class MarketingBackendConnector
    : public base::RefCountedThreadSafe<MarketingBackendConnector> {
 public:
  MarketingBackendConnector(const MarketingBackendConnector&) = delete;
  MarketingBackendConnector& operator=(const MarketingBackendConnector&) =
      delete;

  // A fire and forget method to be called on the marketing opt-in screen.
  // It will create an instance of  MarketingBackendConnectorthat calls the
  // backend to update the user preferences.
  static void UpdateEmailPreferences(Profile* profile,
                                     const std::string& country_code);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Must coincide with the enum
  // MarketingOptInBackendConnectorEvent on enums.xml
  enum class BackendConnectorEvent {
    // Successfully set the user preference on the server
    kSuccess = 0,
    // Possible errors to keep track of.
    kErrorServerInternal = 1,
    kErrorRequestTimeout = 2,
    kErrorAuth = 3,
    kErrorOther = 4,
    kMaxValue = kErrorOther,
  };

 private:
  friend class ScopedRequestCallbackSetter;
  friend class base::RefCountedThreadSafe<MarketingBackendConnector>;

  explicit MarketingBackendConnector(Profile* user_profile);
  virtual ~MarketingBackendConnector();

  // Sends a request to the server to subscribe the user to all campaigns.
  void PerformRequest(const std::string& country_code);

  // Starts the token fetch process.
  void StartTokenFetch();

  // Handles the token fetch response.
  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     signin::AccessTokenInfo access_token_info);

  // Sets the authentication token in the request header and starts the request
  void SetTokenAndStartRequest();

  // Handles responses from the SimpleURLLoader
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnSimpleLoaderCompleteInternal(int response_code,
                                      const std::string& data);

  // Generates the content of the request to be sent based on the country and
  // the language.
  std::string GetRequestContent();

  // Internal
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::string access_token_;
  raw_ptr<Profile> profile_ = nullptr;

  static base::RepeatingCallback<void(std::string)>*
      request_finished_for_tests_;

  // Country code to be used in the request.
  std::string country_code_;
};

// Scoped callback setter for the MarketingBackendConnector
class ScopedRequestCallbackSetter {
 public:
  explicit ScopedRequestCallbackSetter(
      std::unique_ptr<base::RepeatingCallback<void(std::string)>> callback);
  ~ScopedRequestCallbackSetter();

 private:
  std::unique_ptr<base::RepeatingCallback<void(std::string)>> callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_MARKETING_BACKEND_CONNECTOR_H_
