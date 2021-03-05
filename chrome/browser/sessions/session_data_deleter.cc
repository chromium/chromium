// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace {

bool OriginMatcher(const url::Origin& origin,
                   storage::SpecialStoragePolicy* policy) {
  return policy->IsStorageSessionOnly(origin.GetURL()) &&
         !policy->IsStorageProtected(origin.GetURL());
}

class SessionDataDeleter
    : public base::RefCountedThreadSafe<SessionDataDeleter> {
 public:
  SessionDataDeleter(storage::SpecialStoragePolicy* storage_policy,
                     bool delete_only_by_session_only_policy);

  void Run(content::StoragePartition* storage_partition,
           HostContentSettingsMap* host_content_settings_map);

 private:
  friend class base::RefCountedThreadSafe<SessionDataDeleter>;
  ~SessionDataDeleter();

  // Takes the result of a CookieManager::GetAllCookies() method, and
  // initiates deletion of all cookies that are session only by the
  // storage policy of the constructor.
  void DeleteSessionOnlyOriginCookies(
      const std::vector<net::CanonicalCookie>& cookies);

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;
  const bool delete_only_by_session_only_policy_;

  DISALLOW_COPY_AND_ASSIGN(SessionDataDeleter);
};

SessionDataDeleter::SessionDataDeleter(
    storage::SpecialStoragePolicy* storage_policy,
    bool delete_only_by_session_only_policy)
    : storage_policy_(storage_policy),
      delete_only_by_session_only_policy_(delete_only_by_session_only_policy) {
}

void SessionDataDeleter::Run(
    content::StoragePartition* storage_partition,
    HostContentSettingsMap* host_content_settings_map) {
  if (storage_policy_.get() && storage_policy_->HasSessionOnlyOrigins()) {
    // Cookies are not origin scoped, so they are handled separately.
    const uint32_t removal_mask =
        content::StoragePartition::REMOVE_DATA_MASK_ALL &
        ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
    storage_partition->ClearData(
        removal_mask, content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
        base::BindRepeating(&OriginMatcher),
        /*cookie_deletion_filter=*/nullptr,
        /*perform_storage_cleanup=*/false, base::Time(), base::Time::Max(),
        base::DoNothing());
  }

  storage_partition->GetNetworkContext()->GetCookieManager(
      cookie_manager_.BindNewPipeAndPassReceiver());

  if (!delete_only_by_session_only_policy_) {
    network::mojom::CookieDeletionFilterPtr filter(
        network::mojom::CookieDeletionFilter::New());
    filter->session_control =
        network::mojom::CookieDeletionSessionControl::SESSION_COOKIES;
    cookie_manager_->DeleteCookies(
        std::move(filter),
        // Fire and forget
        network::mojom::CookieManager::DeleteCookiesCallback());

    // If the permissions policy feature is enabled, delete the client hint
    // preferences
    if (base::FeatureList::IsEnabled(features::kFeaturePolicyForClientHints)) {
      host_content_settings_map->ClearSettingsForOneType(
          ContentSettingsType::CLIENT_HINTS);
    }
  }

  if (!storage_policy_.get() || !storage_policy_->HasSessionOnlyOrigins())
    return;

  cookie_manager_->GetAllCookies(base::BindOnce(
      &SessionDataDeleter::DeleteSessionOnlyOriginCookies, this));
  // Note that from this point on |*this| is kept alive by scoped_refptr<>
  // references automatically taken by |Bind()|, so when the last callback
  // created by Bind() is released (after execution of that function), the
  // object will be deleted.  This may result in any callbacks passed to
  // |*cookie_manager_.get()| methods not being executed because of the
  // destruction of the mojo::Remote<CookieManager>, but the model of the
  // SessionDataDeleter is "fire-and-forget" and all such callbacks are
  // empty, so that is ok.  Mojo guarantees that all messages pushed onto a
  // pipe will be executed by the server side of the pipe even if the client
  // side of the pipe is closed, so all deletion requested will still occur.
}

void SessionDataDeleter::DeleteSessionOnlyOriginCookies(
    const std::vector<net::CanonicalCookie>& cookies) {
  auto delete_cookie_predicate =
      storage_policy_->CreateDeleteCookieOnExitPredicate();
  DCHECK(delete_cookie_predicate);

  for (const auto& cookie : cookies) {
    if (!delete_cookie_predicate.Run(cookie.Domain(), cookie.IsSecure())) {
      continue;
    }
    // Fire and forget.
    cookie_manager_->DeleteCanonicalCookie(cookie, base::DoNothing());
  }
}

SessionDataDeleter::~SessionDataDeleter() {}

}  // namespace

void DeleteSessionOnlyData(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser_shutdown::IsTryingToQuit())
    return;

  // TODO: Remove Athena special casing once the AthenaSessionRestore is in
  // place.
#if defined(OS_ANDROID)
  SessionStartupPref::Type startup_pref_type =
      SessionStartupPref::GetDefaultStartupType();
#else
  SessionStartupPref::Type startup_pref_type =
      StartupBrowserCreator::GetSessionStartupPref(
          *base::CommandLine::ForCurrentProcess(), profile).type;
#endif

  scoped_refptr<SessionDataDeleter> deleter(
      new SessionDataDeleter(profile->GetSpecialStoragePolicy(),
                             startup_pref_type == SessionStartupPref::LAST));
  deleter->Run(Profile::GetDefaultStoragePartition(profile),
               HostContentSettingsMapFactory::GetForProfile(profile));
}
