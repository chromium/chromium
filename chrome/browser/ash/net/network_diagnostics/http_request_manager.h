// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTP_REQUEST_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTP_REQUEST_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class Profile;

namespace ash {
namespace network_diagnostics {

// Makes an HTTP request and determines the results. Used as a utility in
// network diagnostics routines.
class HttpRequestManager {
 public:
  using HttpRequestCallback = base::OnceCallback<void(bool)>;

  explicit HttpRequestManager(Profile* profile);
  HttpRequestManager(const HttpRequestManager&) = delete;
  HttpRequestManager& operator=(const HttpRequestManager&) = delete;
  virtual ~HttpRequestManager();

  // Begins the asynchronous http request.
  // |url| - The URL to connect to. It is expected that this will be a
  // gstatic.com host.
  // |timeout| - How long to wait for a response before giving up.
  // |callback| - Invoked once an HTTP response is determined.
  virtual void MakeRequest(const GURL& url,
                           const base::TimeDelta& timeout,
                           HttpRequestCallback callback);

  // Setter for testing.
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

 private:
  // Processes the response code in |headers|.
  void OnURLLoadComplete(HttpRequestCallback callback,
                         scoped_refptr<net::HttpResponseHeaders> headers);

  // Holds a reference to the URLLoaderFactory associated with the storage
  // partition for |profile_|.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  // SimpleURLLoader to manage http requests.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTP_REQUEST_MANAGER_H_
