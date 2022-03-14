// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_BACKEND_DELEGATE_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_BACKEND_DELEGATE_H_

#include "ash/services/multidevice_setup/host_backend_delegate.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace multidevice_setup {

// Test HostBackendDelegate implementation.
class FakeHostBackendDelegate : public HostBackendDelegate {
 public:
  FakeHostBackendDelegate();

  FakeHostBackendDelegate(const FakeHostBackendDelegate&) = delete;
  FakeHostBackendDelegate& operator=(const FakeHostBackendDelegate&) = delete;

  ~FakeHostBackendDelegate() override;

  // Changes the backend host to |host_device_on_backend| and notifies
  // observers.
  void NotifyHostChangedOnBackend(
      const absl::optional<multidevice::RemoteDeviceRef>&
          host_device_on_backend);

  void NotifyBackendRequestFailed();

  size_t num_attempt_to_set_calls() { return num_attempt_to_set_calls_; }

  // HostBackendDelegate:
  void AttemptToSetMultiDeviceHostOnBackend(
      const absl::optional<multidevice::RemoteDeviceRef>& host_device) override;
  bool HasPendingHostRequest() override;
  absl::optional<multidevice::RemoteDeviceRef> GetPendingHostRequest()
      const override;
  absl::optional<multidevice::RemoteDeviceRef> GetMultiDeviceHostFromBackend()
      const override;

 private:
  size_t num_attempt_to_set_calls_ = 0u;
  absl::optional<absl::optional<multidevice::RemoteDeviceRef>>
      pending_host_request_;
  absl::optional<multidevice::RemoteDeviceRef> host_device_on_backend_;
};

// Test HostBackendDelegate::Observer implementation.
class FakeHostBackendDelegateObserver : public HostBackendDelegate::Observer {
 public:
  FakeHostBackendDelegateObserver();

  FakeHostBackendDelegateObserver(const FakeHostBackendDelegateObserver&) =
      delete;
  FakeHostBackendDelegateObserver& operator=(
      const FakeHostBackendDelegateObserver&) = delete;

  ~FakeHostBackendDelegateObserver() override;

  size_t num_changes_on_backend() const { return num_changes_on_backend_; }
  size_t num_failed_backend_requests() const {
    return num_failed_backend_requests_;
  }
  size_t num_pending_host_request_changes() const {
    return num_pending_host_request_changes_;
  }

 private:
  // HostBackendDelegate::Observer:
  void OnHostChangedOnBackend() override;
  void OnBackendRequestFailed() override;
  void OnPendingHostRequestChange() override;

  size_t num_changes_on_backend_ = 0u;
  size_t num_failed_backend_requests_ = 0u;
  size_t num_pending_host_request_changes_ = 0u;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_BACKEND_DELEGATE_H_
