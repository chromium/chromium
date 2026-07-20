// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_
#define CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/private_verification_tokens/common/private_verification_tokens_store.h"
#include "components/private_verification_tokens/mojom/private_verification_tokens_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace url {
class Origin;
}

class PrivateVerificationTokensService
    : public KeyedService,
      public private_verification_tokens::mojom::
          PrivateVerificationTokensProvider {
 public:
  static std::unique_ptr<PrivateVerificationTokensService> Create(
      const base::FilePath& data_directory);
  ~PrivateVerificationTokensService() override;
  void Shutdown() override;
  void BindReceiver(
      mojo::PendingReceiver<
          private_verification_tokens::mojom::PrivateVerificationTokensProvider>
          pending_receiver);

  // mojom implementation
  void GetTokens(
      private_verification_tokens::mojom::PrivateVerificationTokensProvider::
          GetTokensCallback callback) override;
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInitializationComplete() = 0;
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

  base::WeakPtr<PrivateVerificationTokensService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  PrivateVerificationTokensService();

  void OnStoreInitialized();

  mojo::ReceiverSet<
      private_verification_tokens::mojom::PrivateVerificationTokensProvider>
      receivers_;
  std::unique_ptr<private_verification_tokens::PrivateVerificationTokensStore>
      store_;
  bool is_shutting_down_ = false;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  std::vector<base::OnceClosure> pending_operations_;

  base::WeakPtrFactory<PrivateVerificationTokensService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_
