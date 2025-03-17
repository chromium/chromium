// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_WEBVIEW_AUTH_HANDLER_H_
#define ASH_WEBUI_BOCA_UI_WEBVIEW_AUTH_HANDLER_H_

#include <memory>
#include <string>

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/webview_auth_delegate.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"

namespace mojom = ash::boca::mojom;
using AuthenticateWebviewCallback = base::OnceCallback<void(bool)>;

namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace signin {
enum class SetAccountsInCookieResult;
}  // namespace signin

namespace ash::boca {

// Helper to authenticate the webview identified by the parameters passed at the
// construction time. Initiates the OAuth authentication process, handles the
// result and retries when feasible.
class WebviewAuthHandler
    : public signin::AccountsCookieMutator::PartitionDelegate {
 public:
  // Callback with authentication result.
  // Called when webview authentication is finished.
  using OnWebviewAuth = base::OnceCallback<void(bool is_success)>;

  // Constructs the `WebviewAuthHandler` for the webview identified by given
  // browser `context` and `webview_host_name`.
  WebviewAuthHandler(std::unique_ptr<WebviewAuthDelegate> delegate,
                     content::BrowserContext* context,
                     const std::string& webview_host_name);
  WebviewAuthHandler(const WebviewAuthHandler&) = delete;
  WebviewAuthHandler& operator=(const WebviewAuthHandler&) = delete;
  virtual ~WebviewAuthHandler();

  // Starts webview authentication and copies the credentials into the storage
  // partition that this class was initialized with.
  // Passes the boolean authentication result in the `callback`.
  virtual void AuthenticateWebview(AuthenticateWebviewCallback callback);

 private:
  // signin::AccountsCookieMutator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;

  // Handles the webview authentication result.
  void OnAuthFinished(OnWebviewAuth callback,
                      signin::SetAccountsInCookieResult cookie_result);

  // Returns storage partition for this authentication request.
  content::StoragePartition* GetStoragePartition();

  // Storage partition configuration for this authentication request.
  content::StoragePartitionConfig storage_partition_config_;

  std::unique_ptr<WebviewAuthDelegate> delegate_;
  const raw_ptr<content::BrowserContext> context_;

  std::unique_ptr<signin::AccountsCookieMutator::SetAccountsInCookieTask>
      cookie_loader_;
};

}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_WEBVIEW_AUTH_HANDLER_H_
