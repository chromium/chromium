// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"

namespace enterprise_connectors {

class KeyPersistenceDelegate;

class DeviceTrustTestEnvironment {
 public:
  using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

  DeviceTrustTestEnvironment(std::string_view thread_name,
                             HttpResponseCode upload_response_code);

  virtual ~DeviceTrustTestEnvironment();

  // Set up an existing device trust key on the device, to test the case where a
  // key already exists on the device.
  virtual void SetUpExistingKey() = 0;

  // Clear a key on persistence delegate to test the cases where there's
  // no key on the device.
  virtual void ClearExistingKey() = 0;

  // Set up an existing device trust key on the device, to test the case where a
  // key already exists on the device.
  virtual std::vector<uint8_t> GetWrappedKey() = 0;

  // Set the result of key upload to test different behaviours of
  // KeyNetworkDelegate.
  void SetUploadResult(HttpResponseCode upload_response_code);

  // Set up an expected DM token, which we will check if it's identical to
  // the DM token we use when uploading to DM server
  void SetExpectedDMToken(std::string expected_dm_token);

  void SetExpectedClientID(std::string expected_client_id);

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

  // Expected dm token to be used when upload to dm server
  std::string expected_dm_token_;

  // Expected client ID to be included in dm server URL when upload to dm server
  std::string expected_client_id_;

  // In-memory scoped KeyPersistenceDelegateFactory to create
  // `key_persistence_delegate_`.
  std::optional<test::ScopedInMemoryKeyPersistenceDelegateFactory>
      scoped_in_memory_key_persistence_delegate_factory_;

  // Instance of platform-dependent KeyPersistenceDelegate to interact with
  // Device Trust keys, this should keep the key in an in-memory storage
  // instead of using the actual key storage.
  std::unique_ptr<KeyPersistenceDelegate> key_persistence_delegate_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_
