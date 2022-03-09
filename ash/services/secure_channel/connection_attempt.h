// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_H_
#define ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/services/secure_channel/client_connection_parameters.h"
#include "ash/services/secure_channel/connection_attempt_delegate.h"
#include "ash/services/secure_channel/connection_attempt_details.h"
#include "ash/services/secure_channel/connection_details.h"
#include "ash/services/secure_channel/pending_connection_request.h"
#include "ash/services/secure_channel/pending_connection_request_delegate.h"
#include "base/time/clock.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace ash::secure_channel {

class AuthenticatedChannel;

// ConnectionAttempt represents an ongoing attempt to connect to a given device
// over a given medium. Each ConnectionAttempt is comprised of one or
// more PendingConnectionRequests and notifies its delegate when the attempt has
// succeeded or failed.
template <typename FailureDetailType>
class ConnectionAttempt : public PendingConnectionRequestDelegate {
 public:
  // Extracts all of the ClientConnectionParameters owned by |attempt|'s
  // PendingConnectionRequests. This function deletes |attempt| as part of this
  // process to ensure that it is no longer used after extraction is complete.
  static std::vector<std::unique_ptr<ClientConnectionParameters>>
  ExtractClientConnectionParameters(
      std::unique_ptr<ConnectionAttempt<FailureDetailType>> attempt) {
    return attempt->ExtractClientConnectionParameters();
  }

  ConnectionAttempt(const ConnectionAttempt&) = delete;
  ConnectionAttempt& operator=(const ConnectionAttempt&) = delete;

  virtual ~ConnectionAttempt() = default;

  const ConnectionAttemptDetails& connection_attempt_details() const {
    return connection_attempt_details_;
  }

  // Associates |request| with this attempt. If the attempt succeeds, |request|
  // will be notified of success; on failure, |request| will be notified of a
  // connection failure. Returns whether adding the request was successful.
  bool AddPendingConnectionRequest(
      std::unique_ptr<PendingConnectionRequest<FailureDetailType>> request) {
    if (!request) {
      PA_LOG(ERROR) << "ConnectionAttempt::AddPendingConnectionRequest(): "
                    << "Received invalid request.";
      return false;
    }

    if (has_notified_delegate_of_success_) {
      PA_LOG(ERROR) << "ConnectionAttempt::AddPendingConnectionRequest(): "
                    << "Tried to add an additional request,but the attempt had "
                    << "already finished.";
      return false;
    }

    ProcessAddingNewConnectionRequest(std::move(request));
    return true;
  }

 protected:
  ConnectionAttempt(ConnectionAttemptDelegate* delegate,
                    base::Clock* clock,
                    const ConnectionAttemptDetails& connection_attempt_details)
      : delegate_(delegate),
        clock_(clock),
        connection_attempt_details_(connection_attempt_details),
        start_attempt_timestamp_(clock_->Now()) {
    DCHECK(delegate);
  }

  // Derived types should use this function to associate the request with this
  // attempt.
  virtual void ProcessAddingNewConnectionRequest(
      std::unique_ptr<PendingConnectionRequest<FailureDetailType>> request) = 0;

  // Extracts the ClientConnectionParameters from all child
  // PendingConnectionRequests.
  virtual std::vector<std::unique_ptr<ClientConnectionParameters>>
  ExtractClientConnectionParameters() = 0;

  // Derived types can override this function to process the amount of time that
  // this connection attempt has taken to produce a successful connection.
  virtual void ProcessSuccessfulConnectionDuration(
      const base::TimeDelta& duration) {}

  void OnConnectionAttemptSucceeded(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    if (has_notified_delegate_of_success_) {
      PA_LOG(ERROR) << "ConnectionAttempt::OnConnectionAttemptSucceeded(): "
                    << "Tried to alert delegate of a successful connection, "
                    << "but the attempt had already finished.";
      return;
    }

    has_notified_delegate_of_success_ = true;
    ProcessSuccessfulConnectionDuration(clock_->Now() -
                                        start_attempt_timestamp_);
    delegate_->OnConnectionAttemptSucceeded(
        connection_attempt_details_.GetAssociatedConnectionDetails(),
        std::move(authenticated_channel));
  }

  void OnConnectionAttemptFinishedWithoutConnection() {
    if (has_notified_delegate_of_success_) {
      PA_LOG(ERROR) << "ConnectionAttempt::"
                    << "OnConnectionAttemptFinishedWithoutConnection(): "
                    << "Tried to alert delegate of a failed connection, "
                    << "but the attempt had already finished.";
      return;
    }

    delegate_->OnConnectionAttemptFinishedWithoutConnection(
        connection_attempt_details_);
  }

 private:
  ConnectionAttemptDelegate* delegate_;
  base::Clock* clock_;
  const ConnectionAttemptDetails connection_attempt_details_;
  const base::Time start_attempt_timestamp_;

  bool has_notified_delegate_of_success_ = false;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_H_
