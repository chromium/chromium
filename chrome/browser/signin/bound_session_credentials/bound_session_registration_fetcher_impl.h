// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace unexportable_keys {
class UnexportableKeyService;
}

class BoundSessionRegistrationFetcherImpl
    : public BoundSessionRegistrationFetcher {
 public:
  explicit BoundSessionRegistrationFetcherImpl(
      BoundSessionRegistrationFetcherParam registration_params,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      unexportable_keys::UnexportableKeyService& key_service);

  BoundSessionRegistrationFetcherImpl(
      BoundSessionRegistrationFetcherImpl&& other) = delete;
  BoundSessionRegistrationFetcherImpl& operator=(
      BoundSessionRegistrationFetcherImpl&& other) noexcept = delete;

  BoundSessionRegistrationFetcherImpl(
      const BoundSessionRegistrationFetcherImpl&) = delete;
  BoundSessionRegistrationFetcherImpl& operator=(
      const BoundSessionRegistrationFetcherImpl&) = delete;
  ~BoundSessionRegistrationFetcherImpl() override;

  // BoundSessionRegistrationFetcher:
  void Start(RegistrationCompleteCallback callback) override;

 private:
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnRegistrationTokenCreated(
      absl::optional<RegistrationTokenHelper::Result> result);

  void StartFetchingRegistration(const std::string& registration_token);

  BoundSessionRegistrationFetcherParam registration_params_;
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  std::string wrapped_key_str_;

  // Non-null after a fetch has started.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<RegistrationTokenHelper> registration_token_helper_;

  RegistrationCompleteCallback callback_;
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_IMPL_H_
