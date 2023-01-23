// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

namespace enterprise_connectors {

class KeyPersistenceDelegate;

class DeviceTrustTestEnvironment {
 public:
  using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

  DeviceTrustTestEnvironment(base::StringPiece thread_name,
                             HttpResponseCode upload_response_code);

  virtual ~DeviceTrustTestEnvironment();

  // Set up an existing device trust key on the device, to test the case where a
  // key already exists on the device.
  virtual void SetUpExistingKey() = 0;

  // Set up an existing device trust key on the device, to test the case where a
  // key already exists on the device.
  virtual std::vector<uint8_t> GetWrappedKey() = 0;

  // Set the result of key upload to test different behaviours of
  // KeyNetworkDelegate.
  void SetUploadResult(HttpResponseCode upload_response_code);

  // Check if device trust key exists on the device.
  bool KeyExists();

 protected:
  // Use a non-ThreadPool worker thread as the code that will run in the
  // background uses a RunLoop, and those are prohibited from running on the
  // ThreadPool.
  base::Thread worker_thread_;

  // Preset response code of key upload, used to test different behaviours of
  // KeyNetworkDelegate.
  HttpResponseCode upload_response_code_;

  // Instance of platform-dependent KeyPersistenceDelegate to interact with
  // Device Trust keys.
  std::unique_ptr<KeyPersistenceDelegate> key_persistence_delegate_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_
