// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"

#include <memory>
#include <variant>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/permissions/permissions_client.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/base/features.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry>
IsolatedWebAppBrowsingDataToDelegateEntries(
    base::flat_map<url::Origin, uint64_t> isolated_web_app_browsing_data) {
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

std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry>
PrivateVerificationTokenBrowsingDataToDelegateEntries(
    std::vector<url::Origin> issuers) {
  std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry> entries;
  for (const auto& issuer : issuers) {
    entries.emplace_back(issuer,
                         static_cast<BrowsingDataModel::StorageType>(
                             ChromeBrowsingDataModelDelegate::StorageType::
                                 kPrivateVerificationTokens),
                         /*storage_size=*/0);
  }
  return entries;
}

std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry>
FlattenDelegateEntries(
    std::vector<std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry>>
        entries) {
  std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry> flattened_entries;
  for (const auto& vec : entries) {
    flattened_entries.insert(flattened_entries.end(), vec.begin(), vec.end());
  }
  return flattened_entries;
}

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
  base::ConcurrentCallbacks<std::vector<DelegateEntry>> concurrent;

  GetAllFederatedIdentityDataKeys(concurrent.CreateCallback(), {});

#if !BUILDFLAG(IS_ANDROID)
  if (storage_partition_->GetConfig().is_default()) {
    web_app::GetIsolatedWebAppBrowsingData(
        profile_, base::BindOnce(&IsolatedWebAppBrowsingDataToDelegateEntries)
                      .Then(concurrent.CreateCallback()));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  GetAllMediaDeviceSaltDataKeys(concurrent.CreateCallback(), {});

  if (base::FeatureList::IsEnabled(
          net::features::kEnablePrivateVerificationTokens)) {
    if (auto* pvt_service =
            PrivateVerificationTokensServiceFactory::GetForProfile(profile_)) {
      pvt_service->GetTokenIssuers(
          base::BindOnce(&PrivateVerificationTokenBrowsingDataToDelegateEntries)
              .Then(concurrent.CreateCallback()));
    }
  }

  // TODO(crbug.com/40205603): Implement data retrieval for remaining data
  // types.

  std::move(concurrent)
      .Done(base::BindOnce(&FlattenDelegateEntries).Then(std::move(callback)));
}

void ChromeBrowsingDataModelDelegate::RemoveDataKey(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageTypeSet storage_types,
    base::OnceClosure callback) {
  base::ConcurrentClosures concurrent;

  if (storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
          StorageType::kMediaDeviceSalt))) {
    if (const blink::StorageKey* storage_key =
            std::get_if<blink::StorageKey>(&data_key)) {
      RemoveMediaDeviceSalt(*storage_key, concurrent.CreateClosure());
    }
  }

  if (storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
          StorageType::kFederatedIdentity))) {
    if (const webid::FederatedIdentityDataModel::DataKey*
            federated_identity_data_key =
                std::get_if<webid::FederatedIdentityDataModel::DataKey>(
                    &data_key)) {
      RemoveFederatedIdentityData(*federated_identity_data_key,
                                  concurrent.CreateClosure());
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  if (storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
          StorageType::kIsolatedWebApp))) {
    CHECK(std::holds_alternative<url::Origin>(data_key));
    const url::Origin& origin = *std::get_if<url::Origin>(&data_key);

    web_app::RemoveIsolatedWebAppBrowsingData(profile_, origin,
                                              concurrent.CreateClosure());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (base::FeatureList::IsEnabled(
          net::features::kEnablePrivateVerificationTokens) &&
      storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
          StorageType::kPrivateVerificationTokens))) {
    if (const url::Origin* origin = std::get_if<url::Origin>(&data_key)) {
      if (auto* pvt_service =
              PrivateVerificationTokensServiceFactory::GetForProfile(
                  profile_)) {
        pvt_service->DeleteTokens(base::Time(), base::Time::Max(),
                                  concurrent.CreateClosure(),
                                  std::vector<url::Origin>{*origin});
      }
    }
  }

  std::move(concurrent).Done(std::move(callback));
}

std::optional<BrowsingDataModel::DataOwner>
ChromeBrowsingDataModelDelegate::GetDataOwner(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageType storage_type) const {
  switch (static_cast<StorageType>(storage_type)) {
    case StorageType::kIsolatedWebApp:
      CHECK(std::holds_alternative<url::Origin>(data_key))
          << "Unsupported IWA DataKey type: " << data_key.index();
      return std::get<url::Origin>(data_key);

    case StorageType::kTopics:
      CHECK(std::holds_alternative<url::Origin>(data_key))
          << "Unsupported Topics DataKey type: " << data_key.index();
      return std::get<url::Origin>(data_key).host();

    case StorageType::kMediaDeviceSalt:
      CHECK(std::holds_alternative<blink::StorageKey>(data_key))
          << "Unsupported MediaDeviceSalt DataKey type: " << data_key.index();
      return std::get<blink::StorageKey>(data_key).origin().host();

    case StorageType::kFederatedIdentity:
      CHECK(std::holds_alternative<webid::FederatedIdentityDataModel::DataKey>(
          data_key))
          << "Unsupported FederatedIdentity DataKey type: " << data_key.index();
      return std::get<webid::FederatedIdentityDataModel::DataKey>(data_key)
          .relying_party_embedder()
          .host();

    case StorageType::kPrivateVerificationTokens:
      CHECK(std::holds_alternative<url::Origin>(data_key))
          << "Unsupported PVT DataKey type: " << data_key.index();
      return std::get<url::Origin>(data_key).host();

    default:
      return std::nullopt;
  }
}

std::optional<bool> ChromeBrowsingDataModelDelegate::IsStorageTypeCookieLike(
    BrowsingDataModel::StorageType storage_type) const {
  // Values below the first delegate type are handled in the model itself.
  if (static_cast<int>(storage_type) <
      static_cast<int>(StorageType::kFirstType)) {
    return std::nullopt;
  }
  switch (
      static_cast<ChromeBrowsingDataModelDelegate::StorageType>(storage_type)) {
    case StorageType::kTopics:
    case StorageType::kIsolatedWebApp:
    case StorageType::kMediaDeviceSalt:
    case StorageType::kFederatedIdentity:
    case StorageType::kPrivateVerificationTokens:
      return false;
  }
  NOTREACHED();
}

std::optional<bool>
ChromeBrowsingDataModelDelegate::IsBlockedByThirdPartyCookieBlocking(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageType storage_type) const {
  // TODO(crbug.com/40066162): Implement `GetThirdPartyPartitioningSite()` for
  // delegate-specific data keys.
  return IsStorageTypeCookieLike(storage_type);
}

bool ChromeBrowsingDataModelDelegate::IsCookieDeletionDisabled(
    const GURL& url) {
  CHECK(profile_);
  auto* client = permissions::PermissionsClient::Get();
  return client->IsCookieDeletionDisabled(profile_, url);
}

base::WeakPtr<BrowsingDataModel::Delegate>
ChromeBrowsingDataModelDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

void ChromeBrowsingDataModelDelegate::GetAllFederatedIdentityDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback,
    std::vector<DelegateEntry> entries) {
  if (auto* context =
          FederatedIdentityPermissionContextFactory::GetForProfile(profile_)) {
    context->GetAllDataKeys(base::BindOnce(
        &ChromeBrowsingDataModelDelegate::GotAllFederatedIdentityDataKeys,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback),
        std::move(entries)));
  } else {
    std::move(callback).Run(std::move(entries));
  }
}

void ChromeBrowsingDataModelDelegate::GotAllFederatedIdentityDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback,
    std::vector<DelegateEntry> entries,
    std::vector<webid::FederatedIdentityDataModel::DataKey> data_keys) {
  static constexpr uint64_t kFederatedIdentityDataEntrySize = 100;
  for (const auto& key : data_keys) {
    entries.emplace_back(key,
                         static_cast<BrowsingDataModel::StorageType>(
                             StorageType::kFederatedIdentity),
                         kFederatedIdentityDataEntrySize);
  }
  std::move(callback).Run(std::move(entries));
}

void ChromeBrowsingDataModelDelegate::RemoveFederatedIdentityData(
    const webid::FederatedIdentityDataModel::DataKey& data_key,
    base::OnceClosure callback) {
  if (auto* context =
          FederatedIdentityPermissionContextFactory::GetForProfile(profile_)) {
    context->RemoveFederatedIdentityDataByDataKey(data_key,
                                                  std::move(callback));
  } else {
    std::move(callback).Run();
  }
}
