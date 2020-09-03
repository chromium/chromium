// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

// An |EncryptionModule| that does no encryption.
class TestEncryptionModuleStrict : public EncryptionModule {
 public:
  TestEncryptionModuleStrict();

  MOCK_METHOD(void,
              EncryptRecord,
              (base::StringPiece record,
               base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb),
              (const override));

  void UpdateAsymmetricKey(
      base::StringPiece new_key,
      base::OnceCallback<void(Status)> response_cb) override;

 protected:
  ~TestEncryptionModuleStrict() override;
};

// Most of the time no need to log uninterested calls to |EncryptRecord|.
typedef ::testing::NiceMock<TestEncryptionModuleStrict> TestEncryptionModule;

}  // namespace test
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_
