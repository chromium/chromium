// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/permissions/permissions_client.h"
#endif

namespace {

class DynamicBarrierClosure : public base::RefCounted<DynamicBarrierClosure> {
 public:
  explicit DynamicBarrierClosure(base::OnceClosure closure)
      : scoped_closure_(std::move(closure)) {}

  DynamicBarrierClosure(const DynamicBarrierClosure&) = delete;
  DynamicBarrierClosure& operator=(const DynamicBarrierClosure&) = delete;

  base::OnceClosure CreateCallback() {
    return base::DoNothingWithBoundArgs(base::WrapRefCounted(this));
  }

 private:
  friend class base::RefCounted<DynamicBarrierClosure>;

  ~DynamicBarrierClosure() = default;

  base::ScopedClosureRunner scoped_closure_;
};

#if !BUILDFLAG(IS_ANDROID)
std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry>
IsolatedWebAppBrowsingDataToDelegateEntries(
    base::flat_map<url::Origin, int64_t> isolated_web_app_browsing_data) {
  std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry> entries;
  for (auto const& [origin, size] : isolated_web_app_browsing_data) {
    entries.emplace_back(
        origin,
        static_cast<BrowsingDataModel::StorageType>(
            ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp),
        size);
  }
  return entries;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// static
std::unique_ptr<ChromeBrowsingDataModelDelegate>
ChromeBrowsingDataModelDelegate::CreateForProfile(Profile* profile) {
  return base::WrapUnique(new ChromeBrowsingDataModelDelegate(
      profile, profile->GetDefaultStoragePartition()));
}

// static
std::unique_ptr<ChromeBrowsingDataModelDelegate>
ChromeBrowsingDataModelDelegate::CreateForStoragePartition(
    Profile* profile,
    content::StoragePartition* storage_partition) {
  return base::WrapUnique(
      new ChromeBrowsingDataModelDelegate(profile, storage_partition));
}

// static
void ChromeBrowsingDataModelDelegate::BrowsingDataAccessed(
    content::RenderFrameHost* rfh,
    const BrowsingDataModel::DataKey& data_key,
    StorageType storage_type,
    bool blocked) {
  content_settings::PageSpecificContentSettings::BrowsingDataAccessed(
      rfh, data_key, static_cast<BrowsingDataModel::StorageType>(storage_type),
      blocked);
}

ChromeBrowsingDataModelDelegate::ChromeBrowsingDataModelDelegate(
    Profile* profile,
    content::StoragePartition* storage_partition)
    : profile_(profile), storage_partition_(storage_partition) {}

ChromeBrowsingDataModelDelegate::~ChromeBrowsingDataModelDelegate() = default;

void ChromeBrowsingDataModelDelegate::GetAllDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback) {
#if !BUILDFLAG(IS_ANDROID)
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (web_app_provider && storage_partition_->GetConfig().is_default()) {
    web_app_provider->scheduler().GetIsolatedWebAppBrowsingData(
        base::BindOnce(&IsolatedWebAppBrowsingDataToDelegateEntries)
            .Then(base::BindOnce(
                &ChromeBrowsingDataModelDelegate::GetAllMediaDeviceSaltDataKeys,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  GetAllMediaDeviceSaltDataKeys(std::move(callback), {});

  // TODO(crbug.com/1271155): Implement data retrieval for remaining data types.
}

void ChromeBrowsingDataModelDelegate::RemoveDataKey(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageTypeSet storage_types,
    base::OnceClosure callback) {
  auto dynamic_barrier_closure =
      base::MakeRefCounted<DynamicBarrierClosure>(std::move(callback));

  if (storage_types.Has(
          static_cast<BrowsingDataModel::StorageType>(StorageType::kTopics))) {
    // Topics can be deleted but not queried from disk as the creating origins
    // are hashed before being saved.
    const url::Origin* origin = absl::get_if<url::Origin>(&data_key);
    auto* browsing_topics_service =
        browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile_);
    browsing_topics_service->ClearTopicsDataForOrigin(*origin);
  }

  if (storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
          StorageType::kMediaDeviceSalt))) {
    if (const blink::StorageKey* storage_key =
            absl::get_if<blink::StorageKey>(&data_key)) {
      RemoveMediaDeviceSalt(*storage_key,
                            dynamic_barrier_closure->CreateCallback());
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  if (storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
          StorageType::kIsolatedWebApp))) {
    CHECK(absl::holds_alternative<url::Origin>(data_key));
    const url::Origin& origin = *absl::get_if<url::Origin>(&data_key);

    web_app::RemoveIsolatedWebAppBrowsingData(
        profile_, origin, dynamic_barrier_closure->CreateCallback());
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

absl::optional<BrowsingDataModel::DataOwner>
ChromeBrowsingDataModelDelegate::GetDataOwner(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageType storage_type) const {
  switch (static_cast<StorageType>(storage_type)) {
    case StorageType::kIsolatedWebApp:
      CHECK(absl::holds_alternative<url::Origin>(data_key))
          << "Unsupported IWA DataKey type: " << data_key.index();
      return absl::get<url::Origin>(data_key);

    case StorageType::kTopics:
      CHECK(absl::holds_alternative<url::Origin>(data_key))
          << "Unsupported Topics DataKey type: " << data_key.index();
      return absl::get<url::Origin>(data_key).host();

    case StorageType::kMediaDeviceSalt:
      CHECK(absl::holds_alternative<blink::StorageKey>(data_key))
          << "Unsupported MediaDeviceSalt DataKey type: " << data_key.index();
      return absl::get<blink::StorageKey>(data_key).origin().host();

    default:
      return absl::nullopt;
  }
}

absl::optional<bool>
ChromeBrowsingDataModelDelegate::IsBlockedByThirdPartyCookieBlocking(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageType storage_type) const {
  // Values below the first delegate type are handled in the model itself.
  if (static_cast<int>(storage_type) <
      static_cast<int>(StorageType::kFirstType)) {
    return absl::nullopt;
  }
  switch (
      static_cast<ChromeBrowsingDataModelDelegate::StorageType>(storage_type)) {
    case StorageType::kTopics:
    case StorageType::kIsolatedWebApp:
    case StorageType::kMediaDeviceSalt:
      return false;
    default:
      NOTREACHED_NORETURN();
  }
}

bool ChromeBrowsingDataModelDelegate::IsCookieDeletionDisabled(
    const GURL& url) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  CHECK(profile_);
  return supervised_user::IsCookieDeletionDisabled(url, *profile_->GetPrefs());
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  if (profile_->IsChild()) {
    auto* client = permissions::PermissionsClient::Get();
    return client->IsCookieDeletionDisabled(profile_, url);
  }
#else
  return false;
#endif
}

void ChromeBrowsingDataModelDelegate::GetAllMediaDeviceSaltDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback,
    std::vector<DelegateEntry> entries) {
  if (auto* service =
          MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
              profile_)) {
    service->GetAllStorageKeys(base::BindOnce(
        &ChromeBrowsingDataModelDelegate::GotAllMediaDeviceSaltDataKeys,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback),
        std::move(entries)));
  } else {
    std::move(callback).Run(std::move(entries));
  }
}

void ChromeBrowsingDataModelDelegate::GotAllMediaDeviceSaltDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback,
    std::vector<DelegateEntry> entries,
    std::vector<blink::StorageKey> storage_keys) {
  static constexpr uint64_t kMediaDeviceSaltEntrySize = 100;
  for (const auto& key : storage_keys) {
    entries.emplace_back(key,
                         static_cast<BrowsingDataModel::StorageType>(
                             StorageType::kMediaDeviceSalt),
                         kMediaDeviceSaltEntrySize);
  }
  std::move(callback).Run(std::move(entries));
}

void ChromeBrowsingDataModelDelegate::RemoveMediaDeviceSalt(
    const blink::StorageKey& storage_key,
    base::OnceClosure callback) {
  media_device_salt::MediaDeviceSaltService* service =
      MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
          profile_);
  if (service) {
    service->DeleteSalt(storage_key, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}
