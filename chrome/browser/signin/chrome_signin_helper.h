// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_

#include <memory>
#include <string>

#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace content_settings {
class CookieSettings;
}

namespace net {
class HttpResponseHeaders;
}

namespace url {
class Origin;
}

class GURL;

// Utility functions for handling Chrome/Gaia headers during signin process.
// Chrome identity should always stay in sync with Gaia identity. Therefore
// Chrome needs to send Gaia special header for requests from a connected
// profile, so that Gaia can modify its response accordingly and let Chrome
// handle signin accordingly.
namespace signin {

enum class Tribool;

// Key for ManageAccountsHeaderReceivedUserData. Exposed for testing.
extern const void* const kManageAccountsHeaderReceivedUserDataKey;

// The source to use when constructing the Mirror header.
extern const char kChromeMirrorHeaderSource[];

class ChromeRequestAdapter : public RequestAdapter {
 public:
  ChromeRequestAdapter(const GURL& url,
                       const net::HttpRequestHeaders& original_headers,
                       net::HttpRequestHeaders* modified_headers,
                       std::vector<std::string>* headers_to_remove);

  ChromeRequestAdapter(const ChromeRequestAdapter&) = delete;
  ChromeRequestAdapter& operator=(const ChromeRequestAdapter&) = delete;

  ~ChromeRequestAdapter() override;

  virtual content::WebContents::Getter GetWebContentsGetter() const = 0;

  virtual network::mojom::RequestDestination GetRequestDestination() const = 0;

  virtual bool IsOutermostMainFrame() const = 0;

  virtual bool IsFetchLikeAPI() const = 0;

  virtual GURL GetReferrer() const = 0;

  // Associate a callback with this request which will be executed when the
  // request is complete (including any redirects). If a callback was already
  // registered this function does nothing.
  virtual void SetDestructionCallback(base::OnceClosure closure) = 0;
};

class ResponseAdapter {
 public:
  ResponseAdapter();

  ResponseAdapter(const ResponseAdapter&) = delete;
  ResponseAdapter& operator=(const ResponseAdapter&) = delete;

  virtual ~ResponseAdapter();

  virtual content::WebContents::Getter GetWebContentsGetter() const = 0;
  virtual bool IsOutermostMainFrame() const = 0;
  virtual GURL GetUrl() const = 0;
  virtual std::optional<url::Origin> GetRequestInitiator() const = 0;
  virtual const url::Origin* GetRequestTopFrameOrigin() const = 0;
  virtual const net::HttpResponseHeaders* GetHeaders() const = 0;
  virtual void RemoveHeader(const std::string& name) = 0;

  virtual base::SupportsUserData::Data* GetUserData(const void* key) const = 0;
  virtual void SetUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) = 0;
};

// When Dice is enabled, the AccountReconcilor is blocked for a short delay
// after sending requests to Gaia. Exposed for testing.
void SetDiceAccountReconcilorBlockDelayForTesting(int delay_ms);

// Adds an account consistency header to Gaia requests from a connected profile,
// with the exception of requests from gaia webview.
// Removes the header if it is already in the headers but should not be there.
void FixAccountConsistencyRequestHeader(
    ChromeRequestAdapter* request,
    const GURL& redirect_url,
    bool is_off_the_record,
    int incognito_availibility,
    AccountConsistencyMethod account_consistency,
    const std::string& gaia_id,
    signin::Tribool is_child_account,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    bool is_secondary_account_addition_allowed,
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    bool is_sync_enabled,
    const std::string& signin_scoped_device_id,
#endif
    content_settings::CookieSettings* cookie_settings);

// Processes account consistency response headers (X-Chrome-Manage-Accounts and
// Dice). |redirect_url| is empty if the request is not a redirect.
void ProcessAccountConsistencyResponseHeaders(ResponseAdapter* response,
                                              const GURL& redirect_url,
                                              bool is_off_the_record);

// Parses and returns an account ID (Gaia ID) from HTTP response header
// Google-Accounts-RemoveLocalAccount. Returns an empty string if parsing
// failed. Exposed for testing purposes.
std::string ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
    const net::HttpResponseHeaders* response_headers);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_
