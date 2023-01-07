// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/certificate_requests.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace chromeos {
namespace certificate_provider {

namespace {
const int kGetCertificatesTimeoutInMinutes = 5;
}  // namespace

// Holds state for a single certificate request.
struct CertificateRequests::CertificateRequestState {
  CertificateRequestState() {}

  ~CertificateRequestState() {}

  // Extensions that are too slow are eventually dropped from a request.
  base::OneShotTimer timeout;

  // Extensions that this request is still waiting for.
  std::set<std::string> pending_extensions;

  // The callback that must be run with the final list of certificates.
  base::OnceCallback<void(net::ClientCertIdentityList)> callback;
};

CertificateRequests::CertificateRequests() {}

CertificateRequests::~CertificateRequests() {}

int CertificateRequests::AddRequest(
    const std::vector<std::string>& extension_ids,
    base::OnceCallback<void(net::ClientCertIdentityList)> callback,
    base::OnceCallback<void(int)> timeout_callback) {
  auto state = std::make_unique<CertificateRequestState>();
  state->callback = std::move(callback);
  state->pending_extensions.insert(extension_ids.begin(), extension_ids.end());

  const int request_id = next_free_request_id_++;
  state->timeout.Start(FROM_HERE,
                       base::Minutes(kGetCertificatesTimeoutInMinutes),
                       base::BindOnce(std::move(timeout_callback), request_id));

  const auto insert_result =
      requests_.insert(std::make_pair(request_id, std::move(state)));
  DCHECK(insert_result.second) << "request id already in use.";
  return request_id;
}

bool CertificateRequests::SetExtensionReplyReceived(
    const std::string& extension_id,
    int request_id,
    bool* completed) {
  *completed = false;
  const auto it = requests_.find(request_id);
  if (it == requests_.end())
    return false;

  CertificateRequestState& state = *it->second;
  if (state.pending_extensions.erase(extension_id) == 0)
    return false;

  *completed = state.pending_extensions.empty();
  return true;
}

bool CertificateRequests::RemoveRequest(
    int request_id,
    base::OnceCallback<void(net::ClientCertIdentityList)>* callback) {
  const auto it = requests_.find(request_id);
  if (it == requests_.end())
    return false;

  CertificateRequestState& state = *it->second;
  *callback = std::move(state.callback);
  requests_.erase(it);
  DVLOG(2) << "Completed certificate request " << request_id;
  return true;
}

std::vector<int> CertificateRequests::DropExtension(
    const std::string& extension_id) {
  std::vector<int> completed_requests;
  for (const auto& entry : requests_) {
    DVLOG(2) << "Remove extension " << extension_id
             << " from certificate request " << entry.first;

    CertificateRequestState& state = *entry.second.get();
    state.pending_extensions.erase(extension_id);
    if (state.pending_extensions.empty())
      completed_requests.push_back(entry.first);
  }
  return completed_requests;
}

}  // namespace certificate_provider
}  // namespace chromeos
