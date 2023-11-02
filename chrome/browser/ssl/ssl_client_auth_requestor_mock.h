// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class ClientCertificateDelegate;
}

namespace net {
class SSLCertRequestInfo;
class SSLPrivateKey;
class X509Certificate;
}

class SSLClientAuthRequestorMock
    : public base::RefCountedThreadSafe<SSLClientAuthRequestorMock> {
 public:
  explicit SSLClientAuthRequestorMock(
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info);

  std::unique_ptr<content::ClientCertificateDelegate> CreateDelegate();
  void WaitForCompletion();

  MOCK_METHOD2(CertificateSelected,
               void(net::X509Certificate* cert, net::SSLPrivateKey* key));
  MOCK_METHOD0(CancelCertificateSelection, void());

  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

 protected:
  friend class base::RefCountedThreadSafe<SSLClientAuthRequestorMock>;
  virtual ~SSLClientAuthRequestorMock();

 private:
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_
