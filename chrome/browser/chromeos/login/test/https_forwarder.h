// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HTTPS_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HTTPS_FORWARDER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "url/gurl.h"

namespace net {
class ScopedTestRoot;
}

namespace chromeos {

class ForwardingServer;

// An https test server that forwards all requests to another server. This
// allows a server that supports http only to be accessed over https.
//
// The server will bind to |127.0.0.1| but will present a certificate issued to
// |ssl_host|.
class HTTPSForwarder {
 public:
  HTTPSForwarder();
  ~HTTPSForwarder();

  // Returns a URL that uses |ssl_host_| as the host.
  GURL GetURLForSSLHost(const std::string& path) const;

  bool Initialize(const std::string& ssl_host,
                  const GURL& forward_target) WARN_UNUSED_RESULT;

  bool Stop() WARN_UNUSED_RESULT;

 private:
  std::string ssl_host_;

  std::unique_ptr<net::ScopedTestRoot> test_root_;
  std::unique_ptr<ForwardingServer> forwarding_server_;

  DISALLOW_COPY_AND_ASSIGN(HTTPSForwarder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HTTPS_FORWARDER_H_
