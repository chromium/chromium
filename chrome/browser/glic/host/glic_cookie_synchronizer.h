// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_COOKIE_SYNCHRONIZER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_COOKIE_SYNCHRONIZER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace signin {
enum class SetAccountsInCookieResult;
}  // namespace signin

namespace glic {

// Helper to sync cookies to the webview storage partition.
class GlicCookieSynchronizer
    : public signin::AccountsCookieMutator::PartitionDelegate,
      public signin::IdentityManager::Observer {
 public:
  // Callback with authentication result.
  // Called when webview authentication is finished.
  using OnWebviewAuth = base::OnceCallback<void(bool)>;

  // The maximum number of retries attempted following a transient error.
  static constexpr int kMaxRetries = 3;

  // If `use_for_fre` the storage partition is configured for use by the glic
  // FRE webview. Otherwise, it is configured for use by the main glic webview.
  GlicCookieSynchronizer(content::BrowserContext* context,
                         signin::IdentityManager* identity_manager,
                         bool use_for_fre);
  GlicCookieSynchronizer(const GlicCookieSynchronizer&) = delete;
  GlicCookieSynchronizer& operator=(const GlicCookieSynchronizer&) = delete;
  ~GlicCookieSynchronizer() override;

  // Virtual for overriding in tests.
  virtual void CopyCookiesToWebviewStoragePartition(
      base::OnceCallback<void(bool)> callback);

  // signin::IdentityManager::Observer
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 protected:
  // Returns storage partition for this authentication request.
  // visible for testing.
  virtual content::StoragePartition* GetStoragePartition();

 private:
  class SyncCookiesForDevelopmentTask;
  base::WeakPtr<GlicCookieSynchronizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SyncCookiesForDevelopmentComplete(bool success);
  void BeginCookieSync();

  // signin::AccountsCookieMutator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;

  // Handles the webview authentication result.
  void OnAuthFinished(signin::SetAccountsInCookieResult cookie_result);
  void CompleteAuth(bool is_success);

  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_{this};

  // Whether to configure the storage partiion for use by the glic FRE webview.
  bool use_for_fre_ = false;

  std::vector<base::OnceCallback<void(bool)>> callbacks_;
  std::unique_ptr<signin::AccountsCookieMutator::SetAccountsInCookieTask>
      cookie_loader_;
  std::unique_ptr<SyncCookiesForDevelopmentTask>
      sync_cookies_for_development_task_;
  base::WeakPtrFactory<GlicCookieSynchronizer> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_COOKIE_SYNCHRONIZER_H_
