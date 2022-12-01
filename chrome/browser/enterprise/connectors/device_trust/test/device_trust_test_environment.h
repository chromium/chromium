// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_

#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

namespace enterprise_connectors {

class DeviceTrustTestEnvironment {
 public:
  using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

  DeviceTrustTestEnvironment(base::StringPiece thread_name,
                             HttpResponseCode upload_response_code)
      : worker_thread_(std::string(thread_name)),
        upload_response_code_(upload_response_code) {}
  virtual ~DeviceTrustTestEnvironment() = default;

  // Set the result of key upload to test different behaviours of
  // KeyNetworkDelegate
  virtual void SetUploadResult(HttpResponseCode upload_response_code) = 0;

  // Set up an existing device trust key on the device, to test the case where a
  // key already exists on the device
  virtual void SetUpExistingKey() = 0;

 protected:
  // Use a non-ThreadPool worker thread as the code that will run in the
  // background uses a RunLoop, and those are prohibited from running on the
  // ThreadPool.
  base::Thread worker_thread_;

  // Preset response code of key upload, used to test different behaviours of
  // KeyNetworkDelegate
  HttpResponseCode upload_response_code_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_H_
