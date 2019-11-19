// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_REQUESTS_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_REQUESTS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_info.h"
#include "net/ssl/client_cert_identity.h"

namespace chromeos {
namespace certificate_provider {

class CertificateRequests {
 public:
  CertificateRequests();
  ~CertificateRequests();

  // Returns the id of the new request. |callback| will be stored with this
  // request and be returned by RemoveRequest(). |timeout_callback| will be
  // called with the request id if this request times out before
  // SetCertificates() was called for all extensions in |extension_ids|.
  int AddRequest(const std::vector<std::string>& extension_ids,
                 base::OnceCallback<void(net::ClientCertIdentityList)> callback,
                 base::OnceCallback<void(int)> timeout_callback);

  // Returns whether this reply was expected, i.e. the request with |request_id|
  // was waiting for a reply from this extension. If it was expected, the
  // certificates are stored and |completed| is set to whether this request has
  // no more pending replies. Otherwise |completed| will be set to false.
  bool SetCertificates(const std::string& extension_id,
                       int request_id,
                       const CertificateInfoList& certificate_infos,
                       bool* completed);

  // If this request is pending, stores the collected certificates in
  // |certificates|, sets |callback|, drops the request and returns true.
  // Otherwise returns false.
  bool RemoveRequest(
      int request_id,
      std::map<std::string, CertificateInfoList>* certificates,
      base::OnceCallback<void(net::ClientCertIdentityList)>* callback);

  // Removes this extension from all pending requests and returns the ids of
  // all completed requests.
  std::vector<int> DropExtension(const std::string& extension_id);

 private:
  struct CertificateRequestState;

  std::map<int, std::unique_ptr<CertificateRequestState>> requests_;
  int next_free_request_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CertificateRequests);
};

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_REQUESTS_H_
