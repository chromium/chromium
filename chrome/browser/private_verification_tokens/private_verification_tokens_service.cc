// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/private_verification_tokens/common/privacy_pass_athm_batch_request.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "components/private_verification_tokens/common/private_verification_tokens_store.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");

using private_verification_tokens::PrivateVerificationTokensStore;

const char* PrivacyPassAthmBatchRequestErrorToString(
    private_verification_tokens::PrivacyPassAthmBatchRequestError error) {
  switch (error) {
    case private_verification_tokens::PrivacyPassAthmBatchRequestError::
        kInvalidBatchSize:
      return "kInvalidBatchSize";
    case private_verification_tokens::PrivacyPassAthmBatchRequestError::
        kInvalidBucketCount:
      return "kInvalidBucketCount";
    case private_verification_tokens::PrivacyPassAthmBatchRequestError::
        kClientRequestGenerationFailed:
      return "kClientRequestGenerationFailed";
    case private_verification_tokens::PrivacyPassAthmBatchRequestError::
        kAlreadyFinalized:
      return "kAlreadyFinalized";
    case private_verification_tokens::PrivacyPassAthmBatchRequestError::
        kInvalidResponseBodyLength:
      return "kInvalidResponseBodyLength";
    case private_verification_tokens::PrivacyPassAthmBatchRequestError::
        kClientFinalizeFailed:
      return "kClientFinalizeFailed";
  }
}

const char* TryGetTokensErrorToString(
    private_verification_tokens::TryGetTokensError error) {
  switch (error) {
    case private_verification_tokens::TryGetTokensError::kNetNotOk:
      return "kNetNotOk";
    case private_verification_tokens::TryGetTokensError::kNullResponse:
      return "kNullResponse";
  }
}

}  // namespace

// static
std::unique_ptr<PrivateVerificationTokensService>
PrivateVerificationTokensService::Create(
    const base::FilePath& data_directory,
    HostContentSettingsMap* host_content_settings_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (data_directory.empty()) {
    return nullptr;
  }
  auto service = base::WrapUnique(
      new PrivateVerificationTokensService(host_content_settings_map));
  base::FilePath db_path = data_directory.Append(kDatabaseName);

  auto store = PrivateVerificationTokensStore::Create(
      db_path,
      base::BindOnce(&PrivateVerificationTokensService::OnStoreInitialized,
                     service->weak_ptr_factory_.GetWeakPtr()));

  if (!store) {
    return nullptr;
  }

  service->store_ = std::move(store);
  return service;
}

PrivateVerificationTokensService::PrivateVerificationTokensService(
    HostContentSettingsMap* host_content_settings_map)
    : host_content_settings_map_(host_content_settings_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PrivateVerificationTokensService::~PrivateVerificationTokensService() = default;

void PrivateVerificationTokensService::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  is_shutting_down_ = true;
  auto operations = std::move(pending_operations_);
  for (auto& operation : operations) {
    std::move(operation).Run();
  }
  active_fetchers_.clear();
  store_ = nullptr;
}

void PrivateVerificationTokensService::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.AddObserver(observer);
}

void PrivateVerificationTokensService::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

bool PrivateVerificationTokensService::is_initialized() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return store_ && store_->is_initialized();
}

bool PrivateVerificationTokensService::IsAntiAbuseEnabled(
    const url::Origin& issuer) const {
  if (!host_content_settings_map_) {
    return true;
  }
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      issuer.GetURL(), issuer.GetURL(), ContentSettingsType::ANTI_ABUSE);
  return setting != CONTENT_SETTING_BLOCK;
}

void PrivateVerificationTokensService::StoreTokens(
    std::vector<private_verification_tokens::PrivateVerificationTokensToken>
        tokens,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    std::move(callback).Run();
    return;
  }

  if (!is_initialized()) {
    pending_operations_.push_back(
        base::BindOnce(&PrivateVerificationTokensService::StoreTokens,
                       weak_ptr_factory_.GetWeakPtr(), std::move(tokens),
                       std::move(callback)));
    return;
  }

  std::erase_if(
      tokens,
      [this](const private_verification_tokens::PrivateVerificationTokensToken&
                 token) { return !IsAntiAbuseEnabled(token.issuer()); });

  if (tokens.empty()) {
    std::move(callback).Run();
    return;
  }

  CHECK(store_);
  store_->StoreTokens(
      std::move(tokens),
      base::BindOnce(
          [](base::WeakPtr<PrivateVerificationTokensService> service,
             base::OnceClosure callback) {
            if (service) {
              for (auto& observer : service->observers_) {
                observer.OnTokensStored();
              }
            }
            if (callback) {
              std::move(callback).Run();
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrivateVerificationTokensService::GetTokenIssuers(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    std::move(callback).Run({});
    return;
  }

  if (!is_initialized()) {
    pending_operations_.push_back(
        base::BindOnce(&PrivateVerificationTokensService::GetTokenIssuers,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::vector<url::Origin> issuers;
  CHECK(store_);
  const std::map<url::Origin, private_verification_tokens::TokenWithId>&
      tokens = store_->tokens();
  issuers.reserve(tokens.size());
  for (const auto& [issuer, unused] : tokens) {
    issuers.push_back(issuer);
  }
  std::move(callback).Run(std::move(issuers));
}

void PrivateVerificationTokensService::DeleteTokens(
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure callback,
    std::optional<std::vector<url::Origin>> issuers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if ((issuers.has_value() && issuers->empty()) || is_shutting_down_) {
    std::move(callback).Run();
    return;
  }

  if (!is_initialized()) {
    pending_operations_.push_back(
        base::BindOnce(&PrivateVerificationTokensService::DeleteTokens,
                       weak_ptr_factory_.GetWeakPtr(), delete_begin, delete_end,
                       std::move(callback), std::move(issuers)));
    return;
  }

  CHECK(store_);
  store_->DeleteTokens(delete_begin, delete_end, std::move(issuers),
                       std::move(callback));
}

void PrivateVerificationTokensService::DeleteTokensByFilter(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const blink::StorageKey&)> storage_key_filter,
    base::OnceClosure callback) {
  // Since this method is composed of the other methods in this service, which
  // handle uninitialized/pending state, we don't need to check and queue these
  // calls up here.
  if (storage_key_filter.is_null()) {
    DeleteTokens(delete_begin, delete_end, std::move(callback), std::nullopt);
    return;
  }
  base::OnceCallback<std::vector<url::Origin>(std::vector<url::Origin>)>
      apply_filter_cb = base::BindOnce(
          [](base::RepeatingCallback<bool(const blink::StorageKey&)>
                 storage_key_filter,
             std::vector<url::Origin> issuers) {
            std::erase_if(issuers, [storage_key_filter](url::Origin issuer) {
              return !storage_key_filter.Run(
                  blink::StorageKey::CreateFirstParty(issuer));
            });
            return issuers;
          },
          storage_key_filter);

  GetTokenIssuers(
      std::move(apply_filter_cb)
          .Then(base::BindOnce(&PrivateVerificationTokensService::DeleteTokens,
                               weak_ptr_factory_.GetWeakPtr(), delete_begin,
                               delete_end, std::move(callback))));
}

void PrivateVerificationTokensService::MaybeFetchTokens(
    const GURL& request_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_ || !url_loader_factory) {
    return;
  }

  if (!is_initialized()) {
    pending_operations_.push_back(
        base::BindOnce(&PrivateVerificationTokensService::MaybeFetchTokens,
                       weak_ptr_factory_.GetWeakPtr(), request_url,
                       std::move(url_loader_factory)));
    return;
  }

  if (!issuer_config_) {
    return;
  }

  url::Origin issuer = url::Origin::Create(request_url);
  if (!IsAntiAbuseEnabled(issuer)) {
    return;
  }

  if (active_fetchers_.contains(issuer)) {
    return;
  }

  CHECK(store_);

  auto it = issuer_config_->config().find(issuer);
  if (it == issuer_config_->config().end()) {
    return;
  }
  const auto& config = it->second;

  if (config.public_key.expiration() <= base::Time::Now()) {
    return;
  }

  if (store_->TokenCountForIssuer(issuer) >
      static_cast<size_t>(config.batch_size / 2)) {
    return;
  }

  auto params = private_verification_tokens::GetParametersForVersion(
      config.public_key.version());
  if (!params.has_value()) {
    VLOG(1) << "Invalid version value in PVT config. Version: "
            << config.public_key.version();
    return;
  }

  base::expected<private_verification_tokens::PrivacyPassAthmBatchRequest,
                 private_verification_tokens::PrivacyPassAthmBatchRequestError>
      batch_request =
          private_verification_tokens::PrivacyPassAthmBatchRequest::Create(
              config, params->num_buckets);
  if (!batch_request.has_value()) {
    VLOG(1) << "PVT token request derivation failed with error: "
            << PrivacyPassAthmBatchRequestErrorToString(batch_request.error());
    return;
  }

  auto fetcher =
      private_verification_tokens::PrivateVerificationTokensFetcher::Create(
          config.issuer_request_url, url_loader_factory->Clone());
  if (!fetcher) {
    VLOG(1) << "Failed to initialize PVT fetcher for URL: "
            << config.issuer_request_url;
    return;
  }

  auto* fetcher_ptr = fetcher.get();
  active_fetchers_[issuer] = std::move(fetcher);

  std::string request_body(batch_request->request_body().begin(),
                           batch_request->request_body().end());

  uint32_t key_id = config.public_key.truncated_key_id();
  base::Time expiration = config.public_key.expiration();
  uint32_t version = config.public_key.version();

  fetcher_ptr->TryGetTokens(
      std::move(request_body),
      base::BindOnce(&PrivateVerificationTokensService::OnFetchTokensCompleted,
                     weak_ptr_factory_.GetWeakPtr(), issuer,
                     std::move(*batch_request), key_id, expiration, version));
}

void PrivateVerificationTokensService::OnFetchTokensCompleted(
    url::Origin issuer,
    private_verification_tokens::PrivacyPassAthmBatchRequest batch_request,
    uint32_t key_id,
    base::Time expiration,
    uint32_t version,
    base::expected<std::string, private_verification_tokens::TryGetTokensResult>
        result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  active_fetchers_.erase(issuer);
  if (!result.has_value()) {
    VLOG(1) << "PVT fetcher failed with error: "
            << TryGetTokensErrorToString(result.error().error)
            << ", network error code: " << result.error().network_error_code;
    return;
  }
  base::expected<std::vector<std::vector<uint8_t>>,
                 private_verification_tokens::PrivacyPassAthmBatchRequestError>
      finalized_tokens =
          batch_request.Finalize(base::as_byte_span(result.value()));
  if (!finalized_tokens.has_value()) {
    VLOG(1) << "PVT response parsing failed with error: "
            << PrivacyPassAthmBatchRequestErrorToString(
                   finalized_tokens.error());
    return;
  }
  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
      tokens;
  tokens.reserve(finalized_tokens->size());
  for (auto& token_bytes : *finalized_tokens) {
    tokens.emplace_back(issuer, std::move(token_bytes), key_id, expiration,
                        version);
  }
  StoreTokens(std::move(tokens), base::DoNothing());
}

std::optional<std::pair<int64_t, std::string>>
PrivateVerificationTokensService::GetTokenForRedemption(
    const url::Origin& redeemer_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_ || !is_initialized() || !issuer_config_) {
    return std::nullopt;
  }

  if (!IsAntiAbuseEnabled(redeemer_origin)) {
    return std::nullopt;
  }

  auto it_issuer = redeemer_to_issuer_.find(redeemer_origin);
  if (it_issuer == redeemer_to_issuer_.end()) {
    return std::nullopt;
  }

  const url::Origin& matching_issuer = it_issuer->second;
  if (!IsAntiAbuseEnabled(matching_issuer)) {
    return std::nullopt;
  }

  auto config_it = issuer_config_->config().find(matching_issuer);
  if (config_it == issuer_config_->config().end() ||
      config_it->second.public_key.expiration() <= base::Time::Now()) {
    return std::nullopt;
  }

  CHECK(store_);
  const auto& tokens = store_->tokens();
  auto it = tokens.find(matching_issuer);
  if (it == tokens.end()) {
    return std::nullopt;
  }

  std::string base64_token = base::Base64Encode(it->second.token.token());
  return std::make_pair(it->second.id, std::move(base64_token));
}

void PrivateVerificationTokensService::DeleteToken(int64_t token_id,
                                                   base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_ || !store_) {
    if (callback) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback));
    }
    return;
  }
  if (!is_initialized()) {
    pending_operations_.push_back(base::BindOnce(
        &PrivateVerificationTokensService::DeleteToken,
        weak_ptr_factory_.GetWeakPtr(), token_id, std::move(callback)));
    return;
  }
  store_->DeleteToken(token_id, std::move(callback));
}

bool PrivateVerificationTokensService::IsRegisteredRedeemer(
    const url::Origin& redeemer_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!issuer_config_) {
    return false;
  }
  auto it = redeemer_to_issuer_.find(redeemer_origin);
  if (it == redeemer_to_issuer_.end()) {
    return false;
  }

  auto config_it = issuer_config_->config().find(it->second);
  if (config_it == issuer_config_->config().end() ||
      config_it->second.public_key.expiration() <= base::Time::Now()) {
    return false;
  }

  return true;
}

void PrivateVerificationTokensService::SetIssuerConfig(
    scoped_refptr<const private_verification_tokens::
                      PrivateVerificationTokensIssuerConfig> issuer_config) {
  issuer_config_ = std::move(issuer_config);
  std::vector<std::pair<url::Origin, url::Origin>> redeemer_to_issuer;
  if (issuer_config_) {
    for (const auto& [issuer, config] : issuer_config_->config()) {
      for (const auto& redeemer : config.redeemers) {
        redeemer_to_issuer.emplace_back(redeemer, issuer);
      }
    }
  }
  redeemer_to_issuer_ =
      base::flat_map<url::Origin, url::Origin>(std::move(redeemer_to_issuer));
}

void PrivateVerificationTokensService::OnStoreInitialized() {
  if (is_shutting_down_) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnInitializationComplete();
  }
  auto operations = std::move(pending_operations_);
  for (auto& operation : operations) {
    std::move(operation).Run();
  }
}
