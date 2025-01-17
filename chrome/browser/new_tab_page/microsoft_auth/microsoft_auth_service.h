// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_H_

#include <string>

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

// Manages microsoft auth access token and its status for the Desktop NTP.
class MicrosoftAuthService : public KeyedService {
 public:
  MicrosoftAuthService();
  ~MicrosoftAuthService() override;

  virtual void SetAccessToken(new_tab_page::mojom::AccessTokenPtr access_token);
  // Get the current access token. If the current access token is expired,
  // clear it, and return an empty string.
  std::string GetAccessToken();

 private:
  new_tab_page::mojom::AccessTokenPtr access_token_ =
      new_tab_page::mojom::AccessToken::New();
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_H_
