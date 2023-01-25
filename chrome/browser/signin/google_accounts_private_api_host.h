// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_GOOGLE_ACCOUNTS_PRIVATE_API_HOST_H_
#define CHROME_BROWSER_SIGNIN_GOOGLE_ACCOUNTS_PRIVATE_API_HOST_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/common/google_accounts_private_api_extension.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

// Host side of the Mojo bridge that reacts to additional functionalities added
// to Google accounts pages.
class GoogleAccountsPrivateApiHost
    : public chrome::mojom::GoogleAccountsPrivateApiExtension,
      public content::DocumentUserData<GoogleAccountsPrivateApiHost> {
 public:
  ~GoogleAccountsPrivateApiHost() override;
  GoogleAccountsPrivateApiHost(const GoogleAccountsPrivateApiHost&) = delete;
  GoogleAccountsPrivateApiHost& operator=(const GoogleAccountsPrivateApiHost&) =
      delete;

  static void CreateReceiver(base::RepeatingCallback<void(const std::string&)>
                                 on_consent_result_callback,
                             content::NavigationHandle* navigation_handle);
  static void BindHost(
      mojo::PendingAssociatedReceiver<
          chrome::mojom::GoogleAccountsPrivateApiExtension> receiver,
      content::RenderFrameHost* render_frame_host);

  void BindReceiver(mojo::PendingAssociatedReceiver<
                    chrome::mojom::GoogleAccountsPrivateApiExtension> receiver);

  // chrome::mojom::GoogleAccountsPrivateApiExtension:
  void SetConsentResult(const std::string& consent_result) override;

 private:
  explicit GoogleAccountsPrivateApiHost(
      content::RenderFrameHost* rfh,
      base::RepeatingCallback<void(const std::string&)>
          on_consent_result_callback);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::AssociatedReceiver<chrome::mojom::GoogleAccountsPrivateApiExtension>
      receiver_;

  base::RepeatingCallback<void(const std::string&)> on_consent_result_callback_;
};

#endif  // CHROME_BROWSER_SIGNIN_GOOGLE_ACCOUNTS_PRIVATE_API_HOST_H_
