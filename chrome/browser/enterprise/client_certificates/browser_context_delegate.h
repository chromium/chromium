// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/enterprise/client_certificates/core/context_delegate.h"

class PrefService;

namespace client_certificates {

class BrowserContextDelegate : public ContextDelegate {
 public:
  explicit BrowserContextDelegate(PrefService* local_state);
  ~BrowserContextDelegate() override;

  // ContextDelegate:
  void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) override;
  std::string GetExpectedCommonName() const override;
  std::string GetIdentityName() override;
  std::string GetTemporaryIdentityName() override;
  std::string GetPolicyPref() override;
  std::string GetConfigPolicyPref() const override;
  std::string GetLoggingContext() override;

 private:
  const raw_ptr<PrefService> local_state_;
};

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_H_
