// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COOKIE_SYNCHRONIZER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COOKIE_SYNCHRONIZER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/signin/public/base/signin_buildflags.h"
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

namespace contextual_tasks {

// Helper to sync cookies to the webview storage partition.
class ContextualTasksCookieSynchronizer
    : public signin::AccountsCookieMutator::PartitionDelegate,
      public signin::IdentityManager::Observer {
 public:
  static constexpr base::TimeDelta kCookieSyncDefaultTimeout = base::Seconds(7);

  ContextualTasksCookieSynchronizer(content::BrowserContext* context,
                                    signin::IdentityManager* identity_manager);
  ContextualTasksCookieSynchronizer(const ContextualTasksCookieSynchronizer&) =
      delete;
  ContextualTasksCookieSynchronizer& operator=(
      const ContextualTasksCookieSynchronizer&) = delete;
  ~ContextualTasksCookieSynchronizer() override;

  // Virtual for overriding in tests.
  virtual void CopyCookiesToWebviewStoragePartition();

  // signin::IdentityManager::Observer
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 protected:
  // Returns storage partition for this authentication request.
  // visible for testing.
  virtual content::StoragePartition* GetStoragePartition();

  virtual void CompleteAuth(bool is_success);

 private:
  void BeginCookieSync();

  // signin::AccountsCookieMutator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  network::mojom::DeviceBoundSessionManager*
  GetDeviceBoundSessionManagerForPartition() override;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Handles the webview authentication result.
  void OnAuthFinished(signin::SetAccountsInCookieResult cookie_result);
  void OnTimeout();

  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_{this};

  base::OneShotTimer timeout_;
  std::unique_ptr<signin::AccountsCookieMutator::SetAccountsInCookieTask>
      cookie_loader_;
  base::WeakPtrFactory<ContextualTasksCookieSynchronizer> weak_ptr_factory_{
      this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COOKIE_SYNCHRONIZER_H_
