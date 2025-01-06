// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_COOKIE_SYNCHRONIZER_H_
#define CHROME_BROWSER_GLIC_GLIC_COOKIE_SYNCHRONIZER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"

namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace signin {
class IdentityManager;
enum class SetAccountsInCookieResult;
}  // namespace signin

namespace glic {

// Helper to sync cookies to the webview storage partition.
class GlicCookieSynchronizer
    : public signin::AccountsCookieMutator::PartitionDelegate {
 public:
  // Callback with authentication result.
  // Called when webview authentication is finished.
  using OnWebviewAuth = base::OnceCallback<void(bool)>;

  // The maximum number of retries attempted following a transient error.
  static constexpr int kMaxRetries = 3;

  GlicCookieSynchronizer(content::BrowserContext* context,
                         signin::IdentityManager* identity_manager);
  GlicCookieSynchronizer(const GlicCookieSynchronizer&) = delete;
  GlicCookieSynchronizer& operator=(const GlicCookieSynchronizer&) = delete;
  virtual ~GlicCookieSynchronizer();

  void CopyCookiesToWebviewStoragePartition(
      base::OnceCallback<void(bool)> callback);

 protected:
  // Returns storage partition for this authentication request.
  // visible for testing.
  virtual content::StoragePartition* GetStoragePartition();

 private:
  base::WeakPtr<GlicCookieSynchronizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  // signin::AccountsCookieMutator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;

  // Handles the webview authentication result.
  void OnAuthFinished(signin::SetAccountsInCookieResult cookie_result);
  void CompleteAuth(bool is_success);

  // Storage partition configuration for this authentication request.
  content::StoragePartitionConfig storage_partition_config_;

  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  std::vector<base::OnceCallback<void(bool)>> callbacks_;
  std::unique_ptr<signin::AccountsCookieMutator::SetAccountsInCookieTask>
      cookie_loader_;

  base::WeakPtrFactory<GlicCookieSynchronizer> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_COOKIE_SYNCHRONIZER_H_
