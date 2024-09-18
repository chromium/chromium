// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/verify_trust_api.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/platform_keys_core/platform_keys_utils.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry_factory.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"

namespace extensions {

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<VerifyTrustAPI>>::Leaky
    g_verify_trust_api_factory = LAZY_INSTANCE_INITIALIZER;

const char kErrorEmptyCertificateChain[] =
    "Server certificate chain must not be empty.";

}  // namespace

// This class bundles IO data and functions of the VerifyTrustAPI that are to be
// used on the IO thread only.
// It is created on the UI thread and afterwards lives on the IO thread.
class VerifyTrustAPI::IOPart {
 public:
  ~IOPart();

  // Verifies the certificate as stated by |params| and calls back |callback|
  // with the result (see the declaration of VerifyCallback).
  // Will not call back after this object is destructed or the verifier for this
  // extension is deleted (see OnExtensionUnloaded).
  void Verify(std::optional<Params> params,
              const std::string& extension_id,
              VerifyCallback callback);

  // Must be called when the extension with id |extension_id| is unloaded.
  // Deletes the verifier for |extension_id| and cancels all pending
  // verifications of this verifier.
  void OnExtensionUnloaded(const std::string& extension_id);

 private:
  struct RequestState {
    RequestState() {}

    RequestState(const RequestState&) = delete;
    RequestState& operator=(const RequestState&) = delete;

    std::unique_ptr<net::CertVerifier::Request> request;
  };

  // Calls back |callback| with the result and no error.
  void CallBackWithResult(VerifyCallback callback,
                          std::unique_ptr<net::CertVerifyResult> verify_result,
                          RequestState* request_state,
                          int return_value);

  // One CertVerifier per extension to verify trust. Each verifier is created on
  // first usage and deleted when this IOPart is destructed or the respective
  // extension is unloaded.
  std::map<std::string, std::unique_ptr<net::CertVerifier>>
      extension_to_verifier_;
};

// static
BrowserContextKeyedAPIFactory<VerifyTrustAPI>*
VerifyTrustAPI::GetFactoryInstance() {
  return g_verify_trust_api_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<
    VerifyTrustAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

VerifyTrustAPI::VerifyTrustAPI(content::BrowserContext* context)
    : io_part_(new IOPart) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  registry_observation_.Observe(ExtensionRegistry::Get(context));
}

VerifyTrustAPI::~VerifyTrustAPI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void VerifyTrustAPI::Verify(std::optional<Params> params,
                            const std::string& extension_id,
                            VerifyCallback ui_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Call back through the VerifyTrustAPI object on the UIThread. Because of the
  // WeakPtr usage, this will ensure that |ui_callback| is not called after the
  // API is destroyed.
  VerifyCallback finish_callback(base::BindOnce(
      &CallBackOnUI,
      base::BindOnce(&VerifyTrustAPI::FinishedVerificationOnUI,
                     weak_factory_.GetWeakPtr(), std::move(ui_callback))));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOPart::Verify, base::Unretained(io_part_.get()),
                     std::move(params), extension_id,
                     std::move(finish_callback)));
}

void VerifyTrustAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOPart::OnExtensionUnloaded,
                     base::Unretained(io_part_.get()), extension->id()));
}

void VerifyTrustAPI::FinishedVerificationOnUI(VerifyCallback ui_callback,
                                              const std::string& error,
                                              int return_value,
                                              int cert_status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(ui_callback).Run(error, return_value, cert_status);
}

// static
void VerifyTrustAPI::CallBackOnUI(VerifyCallback ui_callback,
                                  const std::string& error,
                                  int return_value,
                                  int cert_status) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(ui_callback), error, return_value, cert_status));
}

VerifyTrustAPI::IOPart::~IOPart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void VerifyTrustAPI::IOPart::Verify(std::optional<Params> params,
                                    const std::string& extension_id,
                                    VerifyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const api::platform_keys::VerificationDetails& details = params->details;

  if (details.server_certificate_chain.empty()) {
    std::move(callback).Run(kErrorEmptyCertificateChain, 0, 0);
    return;
  }

  std::vector<std::string_view> der_cert_chain;
  for (const std::vector<uint8_t>& cert_der :
       details.server_certificate_chain) {
    if (cert_der.empty()) {
      std::move(callback).Run(platform_keys::kErrorInvalidX509Cert, 0, 0);
      return;
    }
    der_cert_chain.push_back(std::string_view(
        reinterpret_cast<const char*>(cert_der.data()), cert_der.size()));
  }
  scoped_refptr<net::X509Certificate> cert_chain(
      net::X509Certificate::CreateFromDERCertChain(der_cert_chain));
  if (!cert_chain) {
    std::move(callback).Run(platform_keys::kErrorInvalidX509Cert, 0, 0);
    return;
  }

  if (!base::Contains(extension_to_verifier_, extension_id)) {
    extension_to_verifier_[extension_id] =
        net::CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr);
  }
  net::CertVerifier* verifier = extension_to_verifier_[extension_id].get();

  std::unique_ptr<net::CertVerifyResult> verify_result(
      new net::CertVerifyResult);
  std::unique_ptr<net::NetLogWithSource> net_log(new net::NetLogWithSource);
  const int flags = 0;

  std::string ocsp_response;
  std::string sct_list;
  net::CertVerifyResult* const verify_result_ptr = verify_result.get();

  RequestState* request_state = new RequestState();
  using VerificationCallback = base::OnceCallback<void(int)>;
  VerificationCallback bound_callback = base::BindOnce(
      &IOPart::CallBackWithResult, base::Unretained(this), std::move(callback),
      std::move(verify_result), base::Owned(request_state));
  std::pair<VerificationCallback, VerificationCallback> split_callback =
      base::SplitOnceCallback(std::move(bound_callback));

  const int return_value = verifier->Verify(
      net::CertVerifier::RequestParams(std::move(cert_chain), details.hostname,
                                       flags, ocsp_response, sct_list),
      verify_result_ptr, std::move(split_callback.first),
      &request_state->request, *net_log);

  if (return_value != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(return_value);
    return;
  }
}

void VerifyTrustAPI::IOPart::OnExtensionUnloaded(
    const std::string& extension_id) {
  extension_to_verifier_.erase(extension_id);
}

void VerifyTrustAPI::IOPart::CallBackWithResult(
    VerifyCallback callback,
    std::unique_ptr<net::CertVerifyResult> verify_result,
    RequestState* request_state,
    int return_value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(std::string() /* no error message */, return_value,
                          verify_result->cert_status);
}

}  // namespace extensions
