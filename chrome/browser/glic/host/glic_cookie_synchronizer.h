// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_COOKIE_SYNCHRONIZER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_COOKIE_SYNCHRONIZER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"

namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace signin {
enum class SetAccountsInCookieResult;
}  // namespace signin

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicCookieSyncResult)
enum class GlicCookieSyncResult {
  kSuccess = 0,
  kDestroyedWithPendingCallbacks = 1,
  kNoStoragePartition = 2,
  kDevSyncFailure = 3,
  kUserNotSignedIn = 4,
  kAuthTransientError = 5,
  kAuthPersistentError = 6,
  kTimeout = 7,
  kMaxValue = kTimeout,
};
// LINT.ThenChange(tools/metrics/histograms/metadata/glic/enums.xml:GlicCookieSyncResult)

// Helper to sync cookies to the webview storage partition.
class GlicCookieSynchronizer
    : public signin::AccountsCookieMutator::PartitionDelegate,
      public signin::IdentityManager::Observer {
 public:
  // Callback with authentication result.
  // Called when webview authentication is finished.
  using OnWebviewAuth = base::OnceCallback<void(bool)>;

  // The default timeout. Note that we have a timeout because sometimes the
  // underlying multilogin operation hangs.
  static constexpr base::TimeDelta kCookieSyncDefaultTimeout = base::Seconds(7);

  // Create with the default glic storage partition as the target storage
  // partition.
  GlicCookieSynchronizer(content::BrowserContext* context,
                         signin::IdentityManager* identity_manager);

  // Create with the provided target storage partition.
  GlicCookieSynchronizer(
      content::BrowserContext* context,
      signin::IdentityManager* identity_manager,
      content::StoragePartitionConfig target_storage_partition_config);
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
  FRIEND_TEST_ALL_PREFIXES(GlicCookieSynchronizerTest,
                           UnifiedFreUsesGlicPartitionWithBugfixFeature);
  FRIEND_TEST_ALL_PREFIXES(GlicCookieSynchronizerTest,
                           StandaloneFreUsesFrePartition);

  // Returns storage partition for this authentication request.
  // visible for testing.
  virtual content::StoragePartition* GetStoragePartition();

 private:
  class SyncCookiesForDevelopmentTask;
  class ClearCookiesTask;
  class Metrics {
   public:
    void BeginSync();
    void EndSync(GlicCookieSyncResult result);

   private:
    base::TimeTicks sync_start_time_;
  };

  base::WeakPtr<GlicCookieSynchronizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SyncCookiesForDevelopmentComplete(bool success);
  void ClearCookiesComplete();
  void BeginCookieSync();

  // signin::AccountsCookieMutator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;
  signin::PartitionSuffix GetPartitionSuffix() const override;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  network::mojom::DeviceBoundSessionManager*
  GetDeviceBoundSessionManagerForPartition() override;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Handles the webview authentication result.
  void OnAuthFinished(signin::SetAccountsInCookieResult cookie_result);
  void CompleteAuth(GlicCookieSyncResult result);
  void OnTimeout();

  bool has_cleared_cookies_ = false;
  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  content::StoragePartitionConfig target_storage_partition_config_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_{this};

  std::vector<base::OnceCallback<void(bool)>> callbacks_;
  base::OneShotTimer timeout_;
  std::unique_ptr<signin::AccountsCookieMutator::SetAccountsInCookieTask>
      cookie_loader_;
  std::unique_ptr<ClearCookiesTask> clear_cookies_task_;
  std::unique_ptr<SyncCookiesForDevelopmentTask>
      sync_cookies_for_development_task_;
  Metrics metrics_;
  base::WeakPtrFactory<GlicCookieSynchronizer> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_COOKIE_SYNCHRONIZER_H_
