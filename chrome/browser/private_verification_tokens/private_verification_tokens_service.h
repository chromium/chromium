// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_
#define CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/private_verification_tokens/common/private_verification_tokens_fetcher.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_store.h"

namespace private_verification_tokens {
class PrivacyPassAthmBatchRequest;
}

namespace url {
class Origin;
}

namespace network {
class SharedURLLoaderFactory;
}

class GURL;
class HostContentSettingsMap;

class PrivateVerificationTokensService : public KeyedService {
 public:
  static std::unique_ptr<PrivateVerificationTokensService> Create(
      const base::FilePath& data_directory,
      HostContentSettingsMap* host_content_settings_map = nullptr);
  ~PrivateVerificationTokensService() override;
  void Shutdown() override;
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInitializationComplete() {}
    virtual void OnTokensStored() {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool is_initialized() const;

  // Retrieve all token issuer origins asynchronously.
  void GetTokenIssuers(
      base::OnceCallback<void(std::vector<url::Origin>)> callback);

  // Delete tokens within a time range [delete_begin, delete_end) and/or
  // matching specific origins.
  // If `issuers` is std::nullopt, no filtering is performed on origins.
  // If `issuers` is an empty vector, the method terminates early and returns.
  void DeleteTokens(base::Time delete_begin,
                    base::Time delete_end,
                    base::OnceClosure callback,
                    std::optional<std::vector<url::Origin>> issuers);

  // Delete tokens within a time range [delete_begin, delete_end) and matching a
  // specified filter. If a null RepeatingCallback is supplied, delete all
  // entries in the supplied time range.
  void DeleteTokensByFilter(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const blink::StorageKey&)>
          storage_key_filter,
      base::OnceClosure callback);

  // Store tokens asynchronously.
  void StoreTokens(
      std::vector<private_verification_tokens::PrivateVerificationTokensToken>
          tokens,
      base::OnceClosure callback);

  // Triggers fetching tokens for request_url if request_url origin is a
  // registered issuer and token count is low.
  void MaybeFetchTokens(
      const GURL& request_url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Gets a token for the specified redeemer_origin. Returns a pair of
  // (token_id, base64_encoded_token) if available. Does not delete or remove
  // the token from storage.
  std::optional<std::pair<int64_t, std::string>> GetTokenForRedemption(
      const url::Origin& redeemer_origin);

  // Deletes the token with `token_id` from the cache and database.
  void DeleteToken(int64_t token_id, base::OnceClosure callback);

  // Returns true if the redeemer origin is registered in the config.
  bool IsRegisteredRedeemer(const url::Origin& redeemer_origin) const;

  base::WeakPtr<PrivateVerificationTokensService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetIssuerConfig(
      scoped_refptr<const private_verification_tokens::
                        PrivateVerificationTokensIssuerConfig> issuer_config);
  scoped_refptr<
      const private_verification_tokens::PrivateVerificationTokensIssuerConfig>
  issuer_config() const {
    return issuer_config_;
  }

 private:
  explicit PrivateVerificationTokensService(
      HostContentSettingsMap* host_content_settings_map);

  void OnStoreInitialized();

  bool IsAntiAbuseEnabled(const url::Origin& issuer) const;

  void OnFetchTokensCompleted(
      url::Origin issuer,
      private_verification_tokens::PrivacyPassAthmBatchRequest batch_request,
      uint32_t key_id,
      base::Time expiration,
      uint32_t version,
      base::expected<std::string,
                     private_verification_tokens::TryGetTokensResult> result);

  std::unique_ptr<private_verification_tokens::PrivateVerificationTokensStore>
      store_;

  scoped_refptr<
      const private_verification_tokens::PrivateVerificationTokensIssuerConfig>
      issuer_config_;

  // Maps redeemer origins to their corresponding issuer origin. Computed
  // whenever `issuer_config_` is updated.
  base::flat_map<url::Origin, url::Origin> redeemer_to_issuer_;

  raw_ptr<HostContentSettingsMap> host_content_settings_map_ = nullptr;
  bool is_shutting_down_ = false;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  std::vector<base::OnceClosure> pending_operations_;

  std::map<url::Origin,
           std::unique_ptr<
               private_verification_tokens::PrivateVerificationTokensFetcher>>
      active_fetchers_;

  base::WeakPtrFactory<PrivateVerificationTokensService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_
