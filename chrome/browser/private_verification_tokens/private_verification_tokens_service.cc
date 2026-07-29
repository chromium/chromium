// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/private_verification_tokens/common/private_verification_tokens_store.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");

using private_verification_tokens::PrivateVerificationTokensStore;

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
  store_ = nullptr;
  receivers_.Clear();
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

void PrivateVerificationTokensService::BindReceiver(
    mojo::PendingReceiver<
        private_verification_tokens::mojom::PrivateVerificationTokensProvider>
        pending_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  receivers_.Add(this, std::move(pending_receiver));
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

void PrivateVerificationTokensService::GetTokens(
    private_verification_tokens::mojom::PrivateVerificationTokensProvider::
        GetTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    std::move(callback).Run({});
    return;
  }

  if (!is_initialized()) {
    pending_operations_.push_back(
        base::BindOnce(&PrivateVerificationTokensService::GetTokens,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>
      tokens;
  CHECK(store_);
  for (const auto& [issuer, token_with_id] : store_->tokens()) {
    if (!IsAntiAbuseEnabled(issuer)) {
      continue;
    }
    auto mojo_token = private_verification_tokens::mojom::
        PrivateVerificationTokensToken::New();
    mojo_token->issuer = issuer;
    mojo_token->serialized_token = token_with_id.token.token();
    tokens.push_back(std::move(mojo_token));
  }
  std::move(callback).Run(std::move(tokens));
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
  store_->StoreTokens(std::move(tokens), std::move(callback));
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

void PrivateVerificationTokensService::SetIssuerConfig(
    scoped_refptr<const private_verification_tokens::
                      PrivateVerificationTokensIssuerConfig> issuer_config) {
  issuer_config_ = std::move(issuer_config);
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
