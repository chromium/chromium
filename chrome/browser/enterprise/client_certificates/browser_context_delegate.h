// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_H_

#include "components/enterprise/client_certificates/core/context_delegate.h"

namespace client_certificates {

class BrowserContextDelegate : public ContextDelegate {
 public:
  BrowserContextDelegate();
  ~BrowserContextDelegate() override;

  // ContextDelegate:
  void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) override;
  std::string GetIdentityName() override;
  std::string GetTemporaryIdentityName() override;
  std::string GetPolicyPref() override;
  std::string GetLoggingContext() override;
};

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_H_
