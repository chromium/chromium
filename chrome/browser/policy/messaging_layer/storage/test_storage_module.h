// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_TEST_STORAGE_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_TEST_STORAGE_MODULE_H_

#include <utility>

#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

class TestStorageModuleStrict : public StorageModule {
 public:
  // As opposed to the production |StorageModule|, test module does not need to
  // call factory method - it is created directly by constructor.
  TestStorageModuleStrict();

  MOCK_METHOD(void,
              AddRecord,
              (Priority priority,
               Record record,
               base::OnceCallback<void(Status)> callback),
              (override));

  Record record() const;
  Priority priority() const;

 protected:
  ~TestStorageModuleStrict() override;

 private:
  void AddRecordSuccessfully(Priority priority,
                             Record record,
                             base::OnceCallback<void(Status)> callback);

  base::Optional<Record> record_;
  base::Optional<Priority> priority_;
};

// Most of the time no need to log uninterested calls to |AddRecord|.
typedef ::testing::NiceMock<TestStorageModuleStrict> TestStorageModule;

}  // namespace test
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_TEST_STORAGE_MODULE_H_
