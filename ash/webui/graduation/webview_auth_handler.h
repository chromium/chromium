// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GRADUATION_WEBVIEW_AUTH_HANDLER_H_
#define ASH_WEBUI_GRADUATION_WEBVIEW_AUTH_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/base/backoff_entry.h"

namespace base {
class OneShotTimer;
}  // namespace base
namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace signin {
enum class SetAccountsInCookieResult;
}  // namespace signin

namespace ash::graduation {

// Helper to authenticate the webview identified by the parameters passed at the
// construction time. Initiates the OAuth authentication process, handles the
// result and retries when feasible.
class WebviewAuthHandler
    : public signin::AccountsCookieMutator::PartitionDelegate {
 public:
  // Callback with authentication result.
  // Called when webview authentication is finished.
  using OnWebviewAuth = base::OnceCallback<void(bool is_success)>;

  // The bucket identifiers used to record the authentication result when
  // authentication finishes. Should be kept consistent with
  // ContentTransferAuthenticationResult in
  // tools/metrics/histograms/metadata/ash/enums.xml.
  enum class AuthResult : int {
    // Authentication has succeeded.
    kSuccess = 0,
    // A transient failure occurred during authentication.
    kTransientFailure = 1,
    // A persistent failure occurred during authentication.
    kPersistentFailure = 2,
    kMaxValue = kPersistentFailure,
  };

  static constexpr char kAuthResultHistogramName[] =
      "Ash.ContentTransfer.AuthenticationResult";

  // The maximum number of retries attempted following a transient error.
  static constexpr int kMaxRetries = 3;

  // Constructs the `WebviewAuthHandler` for the webview identified by given
  // browser `context` and `webview_host_name`.
  WebviewAuthHandler(content::BrowserContext* context,
                     const std::string& webview_host_name);
  WebviewAuthHandler(const WebviewAuthHandler&) = delete;
  WebviewAuthHandler& operator=(const WebviewAuthHandler&) = delete;
  virtual ~WebviewAuthHandler();

  // Starts webview authentication and copies the credentials into the storage
  // partition that this class was initialized with.
  // Passes the authentication result in the `callback`.
  virtual void AuthenticateWebview(OnWebviewAuth callback);

 private:
  // signin::AccountsCookieMutator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;

  // Handles the webview authentication result.
  void OnAuthFinished(OnWebviewAuth callback,
                      signin::SetAccountsInCookieResult cookie_result);
  void CompleteAuth(OnWebviewAuth callback, bool is_success);
  void RetryAuth(OnWebviewAuth callback);

  // Returns storage partition for this authentication request.
  content::StoragePartition* GetStoragePartition();

  // Storage partition configuration for this authentication request.
  content::StoragePartitionConfig storage_partition_config_;

  const raw_ptr<content::BrowserContext> context_;

  // Timer that measures time to the next auth fetch retry.
  // Not initialized if retry is not scheduled.
  base::OneShotTimer retry_auth_timer_;
  // Backoff for auth retry attempts.
  net::BackoffEntry retry_auth_backoff_;

  std::unique_ptr<signin::AccountsCookieMutator::SetAccountsInCookieTask>
      cookie_loader_;
};

}  // namespace ash::graduation

#endif  // ASH_WEBUI_GRADUATION_WEBVIEW_AUTH_HANDLER_H_
