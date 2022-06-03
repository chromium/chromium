// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

namespace ash {
namespace device_activity {

namespace {

// Amount of time to wait before retriggering repeating timer.
// Currently we define it as 5 hours to align our protocol with Omahas
// device active reporting.
constexpr base::TimeDelta kTimeToRepeat = base::Hours(5);

}  // namespace

DeviceActivityClient::DeviceActivityClient(NetworkStateHandler* handler)
    : report_timer_(ConstructReportTimer()), network_state_handler_(handler) {
  DCHECK(network_state_handler_);

  report_timer_->Start(FROM_HERE, kTimeToRepeat, this,
                       &DeviceActivityClient::TransitionOutOfIdle);

  network_state_handler_->AddObserver(this, FROM_HERE);
  DefaultNetworkChanged(network_state_handler_->DefaultNetwork());
}

DeviceActivityClient::~DeviceActivityClient() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

std::unique_ptr<base::RepeatingTimer>
DeviceActivityClient::ConstructReportTimer() {
  return std::make_unique<base::RepeatingTimer>();
}

base::RepeatingTimer* DeviceActivityClient::GetReportTimer() {
  return report_timer_.get();
}

// Method gets called when the state of the default (primary)
// network OR properties of the default network changes.
void DeviceActivityClient::DefaultNetworkChanged(const NetworkState* network) {
  bool was_connected = network_connected_;
  network_connected_ = network && network->IsOnline();

  if (network_connected_ == was_connected)
    return;
  if (network_connected_)
    OnNetworkOnline();
}

DeviceActivityClient::State DeviceActivityClient::GetState() const {
  return state_;
}

void DeviceActivityClient::OnNetworkOnline() {
  TransitionOutOfIdle();
}

// TODO(hirthanan): Add callback to report actives only after
// synchronizing the system clock.
void DeviceActivityClient::TransitionOutOfIdle() {
  if (!network_connected_ || state_ != State::kIdle)
    return;

  // The network is connected and the client |state_| is kIdle.
  last_time_network_came_online_ = base::Time::Now();

  // Begin phase one of checking membership.
  TransitionToCheckMembershipOprf();
}

void DeviceActivityClient::TransitionToHealthCheck() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kHealthCheck;

  // TODO(hirthanan): Add Health Check network request with callback to
  // OnHealthCheckDone with response.
}

void DeviceActivityClient::OnHealthCheckDone() {
  TransitionToIdle();
}

void DeviceActivityClient::TransitionToCheckMembershipOprf() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kCheckingMembershipOprf;

  // TODO(hirthanan): Add OPRF network request with callback to
  // OnCheckMembershipOprfDone with response.
}

void DeviceActivityClient::OnCheckMembershipOprfDone() {
  TransitionToCheckMembershipQuery();
}

void DeviceActivityClient::TransitionToCheckMembershipQuery() {
  DCHECK_EQ(state_, State::kCheckingMembershipOprf);
  state_ = State::kCheckingMembershipQuery;

  // TODO(hirthanan): Add Query network request with callback to
  // OnCheckMembershipQueryDone with response.
}

void DeviceActivityClient::OnCheckMembershipQueryDone(bool needs_check_in) {
  if (needs_check_in) {
    TransitionToCheckIn();
  } else {
    TransitionToIdle();
  }
}

void DeviceActivityClient::TransitionToCheckIn() {
  DCHECK_EQ(state_, State::kCheckingMembershipQuery);
  state_ = State::kCheckingIn;

  // TODO(hirthanan): Add import network request with callback to
  // OnCheckInDone with response.
}

void DeviceActivityClient::OnCheckInDone() {
  TransitionToIdle();
}

void DeviceActivityClient::TransitionToIdle() {
  state_ = State::kIdle;
}

}  // namespace device_activity
}  // namespace ash
