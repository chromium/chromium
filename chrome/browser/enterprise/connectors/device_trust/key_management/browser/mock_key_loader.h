// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_KEY_LOADER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_KEY_LOADER_H_

#include "base/functional/callback_helpers.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

class MockKeyLoader : public KeyLoader {
 public:
  MockKeyLoader();
  ~MockKeyLoader() override;

  MOCK_METHOD(void, LoadKey, (LoadKeyCallback), (override));
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_KEY_LOADER_H_
