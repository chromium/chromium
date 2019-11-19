// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/fake_wilco_dtc_supportd_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeWilcoDtcSupportdClient::FakeWilcoDtcSupportdClient() = default;

FakeWilcoDtcSupportdClient::~FakeWilcoDtcSupportdClient() = default;

void FakeWilcoDtcSupportdClient::Init(dbus::Bus* bus) {}

void FakeWilcoDtcSupportdClient::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  if (wait_for_service_to_be_available_result_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  *wait_for_service_to_be_available_result_
                                  /* service_is_available */));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeWilcoDtcSupportdClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    VoidDBusMethodCallback callback) {
  if (bootstrap_mojo_connection_result_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       *bootstrap_mojo_connection_result_ /* result */));
  } else {
    pending_bootstrap_mojo_connection_callbacks_.push_back(std::move(callback));
  }
}

int FakeWilcoDtcSupportdClient::
    wait_for_service_to_be_available_in_flight_call_count() const {
  return static_cast<int>(
      pending_wait_for_service_to_be_available_callbacks_.size());
}

void FakeWilcoDtcSupportdClient::SetWaitForServiceToBeAvailableResult(
    base::Optional<bool> wait_for_service_to_be_available_result) {
  wait_for_service_to_be_available_result_ =
      wait_for_service_to_be_available_result;
  if (!wait_for_service_to_be_available_result_)
    return;
  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(*wait_for_service_to_be_available_result_);
}

int FakeWilcoDtcSupportdClient::bootstrap_mojo_connection_in_flight_call_count()
    const {
  return static_cast<int>(pending_bootstrap_mojo_connection_callbacks_.size());
}

void FakeWilcoDtcSupportdClient::SetBootstrapMojoConnectionResult(
    base::Optional<bool> bootstrap_mojo_connection_result) {
  bootstrap_mojo_connection_result_ = bootstrap_mojo_connection_result;
  if (!bootstrap_mojo_connection_result_)
    return;
  std::vector<VoidDBusMethodCallback> callbacks;
  callbacks.swap(pending_bootstrap_mojo_connection_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(*bootstrap_mojo_connection_result_);
}

}  // namespace chromeos
