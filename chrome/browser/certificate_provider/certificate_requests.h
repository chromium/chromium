// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_REQUESTS_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_REQUESTS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "net/ssl/client_cert_identity.h"

namespace chromeos {
namespace certificate_provider {

class CertificateRequests {
 public:
  CertificateRequests();
  CertificateRequests(const CertificateRequests&) = delete;
  CertificateRequests& operator=(const CertificateRequests&) = delete;
  ~CertificateRequests();

  // Returns the id of the new request. |callback| will be stored with this
  // request and be returned by RemoveRequest(). |timeout_callback| will be
  // called with the request id if this request times out before
  // SetExtensionReplyReceived() was called for all extensions in
  // |extension_ids|.
  int AddRequest(const std::vector<std::string>& extension_ids,
                 base::OnceCallback<void(net::ClientCertIdentityList)> callback,
                 base::OnceCallback<void(int)> timeout_callback);

  // Returns whether this reply was expected, i.e. the request with |request_id|
  // was waiting for a reply from this extension. If it was expected,
  // |completed| is set to whether this request has no more pending replies.
  // Otherwise |completed| will be set to false.
  bool SetExtensionReplyReceived(const std::string& extension_id,
                                 int request_id,
                                 bool* completed);

  // If this request is pending, sets |callback|, drops the request, and returns
  // true. Otherwise returns false.
  bool RemoveRequest(
      int request_id,
      base::OnceCallback<void(net::ClientCertIdentityList)>* callback);

  // Removes this extension from all pending requests and returns the ids of
  // all completed requests.
  std::vector<int> DropExtension(const std::string& extension_id);

 private:
  struct CertificateRequestState;

  std::map<int, std::unique_ptr<CertificateRequestState>> requests_;
  int next_free_request_id_ = 0;
};

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_REQUESTS_H_
