// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_view_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/extensions/api/platform_keys_core/platform_keys_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/host_port_pair.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

namespace {

const char kErrorEmptyCertificateChain[] =
    "Server certificate chain must not be empty.";

base::expected<scoped_refptr<net::X509Certificate>, std::string>
CreateCertChain(std::vector<std::vector<uint8_t>> server_certificate_chain) {
  if (server_certificate_chain.empty()) {
    return base::unexpected(kErrorEmptyCertificateChain);
  }

  std::vector<std::string_view> der_cert_chain;
  for (const std::vector<uint8_t>& cert_der : server_certificate_chain) {
    if (cert_der.empty()) {
      return base::unexpected(platform_keys::kErrorInvalidX509Cert);
    }
    der_cert_chain.push_back(base::as_string_view(cert_der));
  }

  scoped_refptr<net::X509Certificate> cert_chain(
      net::X509Certificate::CreateFromDERCertChain(der_cert_chain));
  if (!cert_chain) {
    return base::unexpected(platform_keys::kErrorInvalidX509Cert);
  }

  return cert_chain;
}

}  // namespace

VerifyTrustApiImpl::VerifyTrustApiImpl(content::BrowserContext* context)
    : browser_context_(context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

VerifyTrustApiImpl::~VerifyTrustApiImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void VerifyTrustApiImpl::Verify(Params params,
                                const std::string& extension_id,
                                VerifyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // WeakPtr usage ensures that `callback` is not called after the
  // API is destroyed.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CreateCertChain,
                     std::move(params.details.server_certificate_chain)),
      base::BindOnce(&VerifyTrustApiImpl::OnCertChainCreated,
                     weak_factory_.GetWeakPtr(),
                     std::move(params.details.hostname), extension_id,
                     std::move(callback)));
}

void VerifyTrustApiImpl::OnCertChainCreated(
    std::string hostname,
    const std::string& extension_id,
    VerifyCallback callback,
    base::expected<scoped_refptr<net::X509Certificate>, std::string>
        maybe_cert_chain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ASSIGN_OR_RETURN(auto cert_chain, std::move(maybe_cert_chain),
                   [&](const std::string& error) {
                     std::move(callback).Run(error, /*verify_result=*/0,
                                             /*cert_status=*/0);
                   });

  browser_context_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->VerifyCert(
          cert_chain, net::HostPortPair(hostname, 443),
          /*ocsp_response=*/{}, /*sct_list=*/{},
          base::BindOnce(&VerifyTrustApiImpl::OnVerifyCert,
                         // WeakPtr usage ensures that `callback` is not
                         // called after the API is destroyed.
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void VerifyTrustApiImpl::OnVerifyCert(VerifyCallback callback,
                                      int verify_result,
                                      const net::CertVerifyResult& result,
                                      bool pkp_bypassed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(/*error=*/std::string(), verify_result,
                          result.cert_status);
}

}  // namespace extensions
