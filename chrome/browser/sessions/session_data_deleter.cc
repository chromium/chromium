// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_data_deleter.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
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
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace {

bool OriginMatcher(const blink::StorageKey& storage_key,
                   storage::SpecialStoragePolicy* policy) {
  return policy->IsStorageSessionOnly(storage_key.origin().GetURL()) &&
         !policy->IsStorageProtected(storage_key.origin().GetURL());
}

class SessionDataDeleterInternal
    : public base::RefCountedThreadSafe<SessionDataDeleterInternal> {
 public:
  SessionDataDeleterInternal(Profile* profile,
                             bool delete_only_by_session_only_policy,
                             base::OnceClosure callback);

  SessionDataDeleterInternal(const SessionDataDeleterInternal&) = delete;
  SessionDataDeleterInternal& operator=(const SessionDataDeleterInternal&) =
      delete;

  void Run(content::StoragePartition* storage_partition,
           HostContentSettingsMap* host_content_settings_map);

 private:
  friend class base::RefCountedThreadSafe<SessionDataDeleterInternal>;
  ~SessionDataDeleterInternal();

  // These functions are used to hold a reference to this object until the
  // cookie and storage deletions are done. This way the keep alives ensure that
  // the profile does not shut down during the deletion.
  void OnCookieDeletionDone(uint32_t count) {}
  void OnStorageDeletionDone() {}

  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  base::OnceClosure callback_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;
  const bool delete_only_by_session_only_policy_;
};

SessionDataDeleterInternal::SessionDataDeleterInternal(
    Profile* profile,
    bool delete_only_by_session_only_policy,
    base::OnceClosure callback)
    : keep_alive_(std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::SESSION_DATA_DELETER,
          KeepAliveRestartOption::ENABLED)),
      profile_keep_alive_(std::make_unique<ScopedProfileKeepAlive>(
          profile,
          ProfileKeepAliveOrigin::kSessionDataDeleter)),
      callback_(std::move(callback)),
      storage_policy_(profile->GetSpecialStoragePolicy()),
      delete_only_by_session_only_policy_(delete_only_by_session_only_policy) {}

void SessionDataDeleterInternal::Run(
    content::StoragePartition* storage_partition,
    HostContentSettingsMap* host_content_settings_map) {
  if (storage_policy_.get() && storage_policy_->HasSessionOnlyOrigins()) {
    // Cookies are not origin scoped, so they are handled separately.
    const uint32_t removal_mask =
        content::StoragePartition::REMOVE_DATA_MASK_ALL &
        ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
    // Clear storage and keep this object alive until deletion is done.
    storage_partition->ClearData(
        removal_mask, content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
        /*filter_builder=*/nullptr, base::BindRepeating(&OriginMatcher),
        /*cookie_deletion_filter=*/nullptr,
        /*perform_storage_cleanup=*/false, base::Time(), base::Time::Max(),
        base::BindOnce(&SessionDataDeleterInternal::OnStorageDeletionDone,
                       this));
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
        // Fire and forget. Session cookies will be cleaned up on start as well.
        // (SQLitePersistentCookieStore::Backend::DeleteSessionCookiesOnStartup)
        base::DoNothing());
    host_content_settings_map->ClearSettingsForOneType(
        ContentSettingsType::CLIENT_HINTS);
    host_content_settings_map->ClearSettingsForOneType(
        ContentSettingsType::REDUCED_ACCEPT_LANGUAGE);
  }

  if (!storage_policy_.get() || !storage_policy_->HasSessionOnlyOrigins())
    return;

  cookie_manager_->DeleteSessionOnlyCookies(
      base::BindOnce(&SessionDataDeleterInternal::OnCookieDeletionDone, this));
  // Note that from this point on |*this| is kept alive by scoped_refptr<>
  // references automatically taken by |Bind()|, so when the last callback
  // created by Bind() is released (after execution of that function), the
  // object will be deleted.
}

SessionDataDeleterInternal::~SessionDataDeleterInternal() {
  if (callback_)
    std::move(callback_).Run();
}

}  // namespace

SessionDataDeleter::SessionDataDeleter(Profile* profile) : profile_(profile) {}

SessionDataDeleter::~SessionDataDeleter() = default;

void SessionDataDeleter::DeleteSessionOnlyData(bool skip_session_cookies,
                                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // As this function is creating KeepAlives, it should not be
  // called during shutdown.
  DCHECK(!browser_shutdown::IsTryingToQuit());

  SessionStartupPref startup_pref =
      StartupBrowserCreator::GetSessionStartupPref(
          *base::CommandLine::ForCurrentProcess(), profile_);

  bool delete_only_by_session_only_policy =
      skip_session_cookies || startup_pref.ShouldRestoreLastSession();

  auto deleter = base::MakeRefCounted<SessionDataDeleterInternal>(
      profile_, delete_only_by_session_only_policy, std::move(callback));
  deleter->Run(profile_->GetDefaultStoragePartition(),
               HostContentSettingsMapFactory::GetForProfile(profile_));
}
