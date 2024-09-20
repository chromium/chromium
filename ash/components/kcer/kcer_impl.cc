// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/kcer/kcer_impl.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_token.h"
#include "ash/components/kcer/token_key_finder.h"
#include "ash/components/kcer/token_results_merger.h"
#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_runner.h"
#include "base/types/expected.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"

namespace kcer::internal {

KcerImpl::KcerImpl()
    : init_queue_(std::make_unique<std::deque<base::OnceClosure>>()) {}
KcerImpl::~KcerImpl() = default;

void KcerImpl::Initialize(scoped_refptr<base::TaskRunner> token_task_runner,
                          base::WeakPtr<KcerToken> user_token,
                          base::WeakPtr<KcerToken> device_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(init_queue_) << "Initialize can only be called once";

  token_task_runner_ = std::move(token_task_runner);
  user_token_ = std::move(user_token);
  device_token_ = std::move(device_token);
  if (user_token_.MaybeValid() || device_token_.MaybeValid()) {
    notifier_.Initialize();
  }

  // Moving out from `init_queue_` makes it invalid and all the other methods
  // will start processing requests instead of queueing them.
  auto pending_tasks = std::move(init_queue_);
  for (base::OnceClosure& task : *pending_tasks) {
    std::move(task).Run();
  }
}

base::WeakPtr<KcerImpl> KcerImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

base::CallbackListSubscription KcerImpl::AddObserver(
    base::RepeatingClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return notifier_.AddObserver(std::move(callback));
}

void KcerImpl::GenerateRsaKey(Token token,
                              RsaModulusLength modulus_length_bits,
                              bool hardware_backed,
                              GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::GenerateRsaKey, weak_factory_.GetWeakPtr(), token,
        modulus_length_bits, hardware_backed, std::move(callback)));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GenerateRsaKey, kcer_token,
                     modulus_length_bits, hardware_backed,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::GenerateEcKey(Token token,
                             EllipticCurve curve,
                             bool hardware_backed,
                             GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::GenerateEcKey, weak_factory_.GetWeakPtr(),
                       token, curve, hardware_backed, std::move(callback)));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GenerateEcKey, kcer_token, curve,
                     hardware_backed,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ImportKey(Token token,
                         Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                         ImportKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::ImportKey, weak_factory_.GetWeakPtr(), token,
                       pkcs8_private_key_info_der, std::move(callback)));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::ImportKey, kcer_token,
                     std::move(pkcs8_private_key_info_der),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ImportCertFromBytes(Token token,
                                   CertDer cert_der,
                                   StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::ImportCertFromBytes, weak_factory_.GetWeakPtr(), token,
        std::move(cert_der), std::move(callback)));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::ImportCertFromBytes, kcer_token,
                     std::move(cert_der),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ImportX509Cert(Token token,
                              scoped_refptr<net::X509Certificate> cert,
                              StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::ImportX509Cert, weak_factory_.GetWeakPtr(),
                       token, std::move(cert), std::move(callback)));
  }

  if (!cert) {
    return std::move(callback).Run(
        base::unexpected(Error::kInvalidCertificate));
  }

  const CRYPTO_BUFFER* buffer = cert->cert_buffer();
  CertDer cert_der(std::vector<uint8_t>(
      CRYPTO_BUFFER_data(buffer),
      CRYPTO_BUFFER_data(buffer) + CRYPTO_BUFFER_len(buffer)));

  return ImportCertFromBytes(token, std::move(cert_der), std::move(callback));
}

void KcerImpl::ImportPkcs12Cert(Token token,
                                Pkcs12Blob pkcs12_blob,
                                std::string password,
                                bool hardware_backed,
                                bool mark_as_migrated,
                                StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::ImportPkcs12Cert, weak_factory_.GetWeakPtr(),
                       token, std::move(pkcs12_blob), std::move(password),
                       hardware_backed, mark_as_migrated, std::move(callback)));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::ImportPkcs12Cert, kcer_token,
                     std::move(pkcs12_blob), std::move(password),
                     hardware_backed, mark_as_migrated,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                ExportPkcs12Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(244408716): Implement.
}

void KcerImpl::RemoveKeyAndCerts(PrivateKeyHandle key,
                                 StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::RemoveKeyAndCerts, weak_factory_.GetWeakPtr(),
                       std::move(key), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return RemoveKeyAndCertsWithToken(std::move(callback), std::move(key));
  }

  auto on_find_key_done =
      base::BindOnce(&KcerImpl::RemoveKeyAndCertsWithToken,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::RemoveKeyAndCertsWithToken(
    StatusCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::RemoveKeyAndCerts, kcer_token, std::move(key),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::RemoveCert(scoped_refptr<const Cert> cert,
                          StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::RemoveCert, weak_factory_.GetWeakPtr(),
                       std::move(cert), std::move(callback)));
  }

  if (!cert) {
    return std::move(callback).Run(
        base::unexpected(Error::kInvalidCertificate));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(cert->GetToken());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::RemoveCert, kcer_token, std::move(cert),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ListKeys(base::flat_set<Token> tokens,
                        ListKeysCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::ListKeys, weak_factory_.GetWeakPtr(),
                       std::move(tokens), std::move(callback)));
  }

  if (tokens.empty()) {
    return std::move(callback).Run(/*certs=*/{}, /*errors=*/{});
  }

  scoped_refptr<TokenResultsMerger<PublicKey>> merger =
      internal::TokenResultsMerger<PublicKey>::Create(
          /*results_to_receive=*/tokens.size(), std::move(callback));
  for (Token token : tokens) {
    auto callback_for_token = merger->GetCallback(token);
    const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
    if (!kcer_token.MaybeValid()) {
      std::move(callback_for_token)
          .Run(base::unexpected(Error::kTokenIsNotAvailable));
    } else {
      token_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&KcerToken::ListKeys, kcer_token,
                                    base::BindPostTaskToCurrentDefault(
                                        std::move(callback_for_token))));
    }
  }
}

void KcerImpl::ListCerts(base::flat_set<Token> tokens,
                         ListCertsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::ListCerts, weak_factory_.GetWeakPtr(),
                       std::move(tokens), std::move(callback)));
  }

  if (tokens.empty()) {
    return std::move(callback).Run(/*certs=*/{}, /*errors=*/{});
  }

  scoped_refptr<TokenResultsMerger<scoped_refptr<const Cert>>> merger =
      TokenResultsMerger<scoped_refptr<const Cert>>::Create(
          /*results_to_receive=*/tokens.size(), std::move(callback));
  for (Token token : tokens) {
    auto callback_for_token = merger->GetCallback(token);
    const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
    if (!kcer_token.MaybeValid()) {
      std::move(callback_for_token)
          .Run(base::unexpected(Error::kTokenIsNotAvailable));
    } else {
      token_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&KcerToken::ListCerts, kcer_token,
                                    base::BindPostTaskToCurrentDefault(
                                        std::move(callback_for_token))));
    }
  }
}

void KcerImpl::DoesPrivateKeyExist(PrivateKeyHandle key,
                                   DoesKeyExistCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::DoesPrivateKeyExist, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    const base::WeakPtr<KcerToken>& kcer_token =
        GetToken(key.GetTokenInternal().value());
    if (!kcer_token.MaybeValid()) {
      return std::move(callback).Run(
          base::unexpected(Error::kTokenIsNotAvailable));
    }
    token_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &KcerToken::DoesPrivateKeyExist, kcer_token, std::move(key),
            base::BindPostTaskToCurrentDefault(std::move(callback))));
    return;
  }

  auto on_find_key_done =
      base::BindOnce(&KcerImpl::DoesPrivateKeyExistWithToken,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  return FindKeyToken(/*allow_guessing=*/false,
                      /*key=*/std::move(key), std::move(on_find_key_done));
}

void KcerImpl::DoesPrivateKeyExistWithToken(
    DoesKeyExistCallback callback,
    base::expected<std::optional<Token>, Error> find_key_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!find_key_result.has_value()) {
    return std::move(callback).Run(base::unexpected(find_key_result.error()));
  }
  const std::optional<Token>& token = find_key_result.value();
  return std::move(callback).Run(token.has_value());
}

void KcerImpl::Sign(PrivateKeyHandle key,
                    SigningScheme signing_scheme,
                    DataToSign data,
                    SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::Sign, weak_factory_.GetWeakPtr(), std::move(key),
        signing_scheme, std::move(data), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return SignWithToken(signing_scheme, std::move(data), std::move(callback),
                         std::move(key));
  }

  auto on_find_key_done =
      base::BindOnce(&KcerImpl::SignWithToken, weak_factory_.GetWeakPtr(),
                     signing_scheme, std::move(data), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::SignWithToken(
    SigningScheme signing_scheme,
    DataToSign data,
    SignCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::Sign, kcer_token, std::move(key),
                     signing_scheme, std::move(data),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::SignRsaPkcs1Raw(PrivateKeyHandle key,
                               DigestWithPrefix digest_with_prefix,
                               SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::SignRsaPkcs1Raw, weak_factory_.GetWeakPtr(), std::move(key),
        std::move(digest_with_prefix), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return SignRsaPkcs1RawWithToken(std::move(digest_with_prefix),
                                    std::move(callback), std::move(key));
  }

  auto on_find_key_done = base::BindOnce(
      &KcerImpl::SignRsaPkcs1RawWithToken, weak_factory_.GetWeakPtr(),
      std::move(digest_with_prefix), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::SignRsaPkcs1RawWithToken(
    DigestWithPrefix digest_with_prefix,
    SignCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::SignRsaPkcs1Raw, kcer_token, std::move(key),
                     std::move(digest_with_prefix),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::GetAvailableTokens(GetAvailableTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(&KcerImpl::GetAvailableTokens,
                                                 weak_factory_.GetWeakPtr(),
                                                 std::move(callback)));
  }
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetCurrentTokens()));
}

base::flat_set<Token> KcerImpl::GetCurrentTokens() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::flat_set<Token> result;
  if (user_token_.MaybeValid()) {
    result.insert(Token::kUser);
  }
  if (device_token_.MaybeValid()) {
    result.insert(Token::kDevice);
  }
  return result;
}

void KcerImpl::GetTokenInfo(Token token, GetTokenInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(&KcerImpl::GetTokenInfo,
                                                 weak_factory_.GetWeakPtr(),
                                                 token, std::move(callback)));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GetTokenInfo, kcer_token,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::GetKeyInfo(PrivateKeyHandle key, GetKeyInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::GetKeyInfo, weak_factory_.GetWeakPtr(),
                       std::move(key), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return GetKeyInfoWithToken(std::move(callback), std::move(key));
  }

  auto on_find_key_done =
      base::BindOnce(&KcerImpl::GetKeyInfoWithToken, weak_factory_.GetWeakPtr(),
                     std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::GetKeyInfoWithToken(
    GetKeyInfoCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GetKeyInfo, kcer_token, std::move(key),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::GetKeyPermissions(PrivateKeyHandle key,
                                 GetKeyPermissionsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(
        base::BindOnce(&KcerImpl::GetKeyPermissions, weak_factory_.GetWeakPtr(),
                       std::move(key), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return GetKeyPermissionsWithToken(std::move(callback), std::move(key));
  }

  auto on_find_key_done =
      base::BindOnce(&KcerImpl::GetKeyPermissionsWithToken,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::GetKeyPermissionsWithToken(
    GetKeyPermissionsCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GetKeyPermissions, kcer_token, std::move(key),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::GetCertProvisioningProfileId(
    PrivateKeyHandle key,
    GetCertProvisioningProfileIdCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::GetCertProvisioningProfileId, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return GetCertProvisioningProfileIdWithToken(std::move(callback),
                                                 std::move(key));
  }

  auto on_find_key_done =
      base::BindOnce(&KcerImpl::GetCertProvisioningProfileIdWithToken,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::GetCertProvisioningProfileIdWithToken(
    GetCertProvisioningProfileIdCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GetCertProvisioningProfileId, kcer_token,
                     std::move(key),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::SetKeyNickname(PrivateKeyHandle key,
                              std::string nickname,
                              StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::SetKeyNickname, weak_factory_.GetWeakPtr(), std::move(key),
        std::move(nickname), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return SetKeyNicknameWithToken(std::move(nickname), std::move(callback),
                                   std::move(key));
  }

  auto on_token_populated = base::BindOnce(
      &KcerImpl::SetKeyNicknameWithToken, weak_factory_.GetWeakPtr(),
      std::move(nickname), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_token_populated));
}

void KcerImpl::SetKeyNicknameWithToken(
    std::string nickname,
    StatusCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::SetKeyNickname, kcer_token, std::move(key),
                     std::move(nickname),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::SetKeyPermissions(PrivateKeyHandle key,
                                 chaps::KeyPermissions key_permissions,
                                 StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::SetKeyPermissions, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(key_permissions), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return SetKeyPermissionsWithToken(std::move(key_permissions),
                                      std::move(callback), std::move(key));
  }

  auto on_find_key_done = base::BindOnce(
      &KcerImpl::SetKeyPermissionsWithToken, weak_factory_.GetWeakPtr(),
      std::move(key_permissions), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::SetKeyPermissionsWithToken(
    chaps::KeyPermissions key_permissions,
    StatusCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::SetKeyPermissions, kcer_token, std::move(key),
                     std::move(key_permissions),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::SetCertProvisioningProfileId(PrivateKeyHandle key,
                                            std::string profile_id,
                                            StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (init_queue_) {
    return init_queue_->push_back(base::BindOnce(
        &KcerImpl::SetCertProvisioningProfileId, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(profile_id), std::move(callback)));
  }

  if (key.GetTokenInternal().has_value()) {
    return SetCertProvisioningProfileIdWithToken(
        std::move(profile_id), std::move(callback), std::move(key));
  }

  auto on_find_key_done = base::BindOnce(
      &KcerImpl::SetCertProvisioningProfileIdWithToken,
      weak_factory_.GetWeakPtr(), std::move(profile_id), std::move(callback));
  return PopulateTokenForKey(std::move(key), std::move(on_find_key_done));
}

void KcerImpl::SetCertProvisioningProfileIdWithToken(
    std::string profile_id,
    StatusCallback callback,
    base::expected<PrivateKeyHandle, Error> key_or_error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!key_or_error.has_value()) {
    return std::move(callback).Run(base::unexpected(key_or_error.error()));
  }
  PrivateKeyHandle key = std::move(key_or_error).value();

  const base::WeakPtr<KcerToken>& kcer_token =
      GetToken(key.GetTokenInternal().value());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::SetCertProvisioningProfileId, kcer_token,
                     std::move(key), std::move(profile_id),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

base::WeakPtr<internal::KcerToken>& KcerImpl::GetToken(Token token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (token) {
    case Token::kUser:
      return user_token_;
    case Token::kDevice:
      return device_token_;
  }
}

void KcerImpl::FindKeyToken(
    bool allow_guessing,
    PrivateKeyHandle key,
    base::OnceCallback<void(base::expected<std::optional<Token>, Error>)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::flat_set<Token> tokens = GetCurrentTokens();

  if (tokens.empty()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }

  if (allow_guessing && (tokens.size() == 1)) {
    return std::move(callback).Run(*tokens.begin());
  }

  auto key_finder = TokenKeyFinder::Create(
      /*results_to_receive=*/tokens.size(), std::move(callback));
  for (Token token : tokens) {
    auto token_callback =
        base::BindPostTaskToCurrentDefault(key_finder->GetCallback(token));
    token_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&KcerToken::DoesPrivateKeyExist, GetToken(token), key,
                       std::move(token_callback)));
  }
}

void KcerImpl::PopulateTokenForKey(
    PrivateKeyHandle key,
    base::OnceCallback<void(base::expected<PrivateKeyHandle, Error>)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto on_find_key_done =
      base::BindOnce(&KcerImpl::PopulateTokenForKeyWithToken,
                     weak_factory_.GetWeakPtr(), key, std::move(callback));
  FindKeyToken(/*allow_guessing=*/true,
               /*key=*/std::move(key), std::move(on_find_key_done));
}

void KcerImpl::PopulateTokenForKeyWithToken(
    PrivateKeyHandle key,
    base::OnceCallback<void(base::expected<PrivateKeyHandle, Error>)> callback,
    base::expected<std::optional<Token>, Error> find_key_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!find_key_result.has_value()) {
    return std::move(callback).Run(base::unexpected(find_key_result.error()));
  }

  if (!find_key_result.value().has_value()) {
    return std::move(callback).Run(base::unexpected(Error::kKeyNotFound));
  }

  Token token = find_key_result.value().value();
  return std::move(callback).Run(PrivateKeyHandle(token, std::move(key)));
}

}  // namespace kcer::internal
