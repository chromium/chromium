// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_
#define ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_

#include <utility>
#include <vector>

#include "ash/services/secure_channel/pending_connection_request.h"
#include "base/unguessable_token.h"

namespace ash::secure_channel {

class ClientConnectionParameters;
class PendingConnectionRequestDelegate;

// Fake PendingConnectionRequest implementation.
template <typename FailureDetailType>
class FakePendingConnectionRequest
    : public PendingConnectionRequest<FailureDetailType> {
 public:
  FakePendingConnectionRequest(PendingConnectionRequestDelegate* delegate,
                               ConnectionPriority connection_priority)
      : PendingConnectionRequest<FailureDetailType>(delegate,
                                                    connection_priority),
        id_(base::UnguessableToken::Create()) {}

  FakePendingConnectionRequest(const FakePendingConnectionRequest&) = delete;
  FakePendingConnectionRequest& operator=(const FakePendingConnectionRequest&) =
      delete;

  ~FakePendingConnectionRequest() override = default;

  const std::vector<FailureDetailType>& handled_failure_details() const {
    return handled_failure_details_;
  }

  void set_client_data_for_extraction(
      std::unique_ptr<ClientConnectionParameters> client_data_for_extraction) {
    client_data_for_extraction_ = std::move(client_data_for_extraction);
  }

  // PendingConnectionRequest<FailureDetailType>:
  const base::UnguessableToken& GetRequestId() const override { return id_; }

  // Make NotifyRequestFinishedWithoutConnection() public for testing.
  using PendingConnectionRequest<
      FailureDetailType>::NotifyRequestFinishedWithoutConnection;

 private:
  // PendingConnectionRequest<FailureDetailType>:
  void HandleConnectionFailure(FailureDetailType failure_detail) override {
    handled_failure_details_.push_back(failure_detail);
  }

  std::unique_ptr<ClientConnectionParameters>
  ExtractClientConnectionParameters() override {
    DCHECK(client_data_for_extraction_);
    return std::move(client_data_for_extraction_);
  }

  const base::UnguessableToken id_;

  std::vector<FailureDetailType> handled_failure_details_;

  std::unique_ptr<ClientConnectionParameters> client_data_for_extraction_;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_
