// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"

#include <stdint.h>

#include <iterator>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"
#include "url/url_canon.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/platform_auth/cloud_ap_provider_win.h"
#endif

namespace enterprise_auth {

namespace {

std::unique_ptr<PlatformAuthProvider> MakeProvider() {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<CloudApProviderWin>();
#else
  return nullptr;
#endif
}

}  // namespace

// static
PlatformAuthProviderManager& PlatformAuthProviderManager::GetInstance() {
  static base::NoDestructor<PlatformAuthProviderManager> instance;
  return *instance;
}

void PlatformAuthProviderManager::SetEnabled(bool enabled,
                                             base::OnceClosure on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Drop any pending fetch; its callback will never be run.
  weak_factory_.InvalidateWeakPtrs();
  on_enable_complete_.Reset();

  // Drop origins if previously enabled.
  if (!enabled && !origins_.empty())
    origins_.clear();

  enabled_ = enabled;

  on_enable_complete_ = std::move(on_complete);
  StartFetchOrigins();

  // TODO(crbug.com/1246839): Users may add/remove WebAccounts, which could
  // change the set of origins. Consider polling on a low-frequency timer and/or
  // using a `WebAccountMonitor` (obtained from `WebAuthenticationCoreManager`)
  // to watch for account removals. I don't see a way to watch for additions.
}

bool PlatformAuthProviderManager::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

bool PlatformAuthProviderManager::IsEnabledFor(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(origins_, url::Origin::Create(url));
}

void PlatformAuthProviderManager::GetData(const GURL& url,
                                          GetDataCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());

  // In general, callers should only request data for requests that are headed
  // toward one of the origins stored in `origins_`. Given the async nature of
  // changes to the set of origins, it's possible that a request could come in
  // after the manager had been disabled or after a change to the set of
  // origins.
  if (!IsEnabledFor(url)) {
    std::move(callback).Run(net::HttpRequestHeaders());
  } else {
    DCHECK(provider_);
    provider_->GetData(url, std::move(callback));
  }
}

PlatformAuthProviderManager::PlatformAuthProviderManager()
    : PlatformAuthProviderManager(MakeProvider()) {}

PlatformAuthProviderManager::PlatformAuthProviderManager(
    std::unique_ptr<PlatformAuthProvider> provider)
    : provider_(std::move(provider)) {}

PlatformAuthProviderManager::~PlatformAuthProviderManager() = default;

void PlatformAuthProviderManager::StartFetchOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (enabled_ && provider_) {
    provider_->FetchOrigins(base::BindOnce(
        &PlatformAuthProviderManager::OnOrigins, weak_factory_.GetWeakPtr()));
  } else if (on_enable_complete_) {
    std::move(on_enable_complete_).Run();
  }
}

void PlatformAuthProviderManager::OnOrigins(
    std::unique_ptr<std::vector<url::Origin>> origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_set<url::Origin> new_origins;

  if (!origins) {
    // The provider is indicating that it can never return origins, so never ask
    // for them again.
    origins_.clear();
    provider_.reset();
  } else {
    new_origins = base::flat_set<url::Origin>(std::move(*origins));
  }

  if (origins_ != new_origins)
    origins_ = std::move(new_origins);

  if (on_enable_complete_)
    std::move(on_enable_complete_).Run();
}

std::unique_ptr<PlatformAuthProvider>
PlatformAuthProviderManager::SetProviderForTesting(
    std::unique_ptr<PlatformAuthProvider> provider) {
  return std::exchange(provider_, std::move(provider));
}

}  // namespace enterprise_auth
