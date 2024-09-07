// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "net/ssl/client_cert_identity.h"

namespace content {
class ClientCertificateDelegate;
class WebContents;
}

namespace net {
class SSLCertRequestInfo;
}

// Opens a constrained SSL client certificate selection dialog under |parent|,
// offering certificates in |client_certs| for the host specified by
// |cert_request_info|. When the user has made a selection, the dialog will
// report back to |delegate|. If the dialog is closed with no selection,
// |delegate| will simply be destroyed.
//
// Returns a UI-thread callback that will cancel the dialog. The callback may be
// null depending on the implementation and is not required to be run.
base::OnceClosure ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate);

using ShowSSLClientCertificateSelectorTestingHook =
    base::RepeatingCallback<base::OnceClosure(
        content::WebContents* contents,
        net::SSLCertRequestInfo* cert_request_info,
        net::ClientCertIdentityList client_certs,
        std::unique_ptr<content::ClientCertificateDelegate> delegate)>;
// Sets a test-only hook to substitute the default implementation of
// `ShowSSLClientCertificateSelector()`. Pass null as `hook` in order to unset
// the hook and switch back to the default implementation.
void SetShowSSLClientCertificateSelectorHookForTest(
    ShowSSLClientCertificateSelectorTestingHook hook);

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
