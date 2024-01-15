// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace api {

class DocumentScanScanFunctionTest : public ExtensionApiUnittest {
 public:
  DocumentScanScanFunctionTest()
      : function_(base::MakeRefCounted<DocumentScanScanFunction>()) {}
  ~DocumentScanScanFunctionTest() override {}

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    function_->set_user_gesture(true);
  }

 protected:
  std::string RunFunctionAndReturnError(const std::string& args) {
    function_->set_extension(extension());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function_.get(), args, browser()->profile(),
        api_test_utils::FunctionMode::kNone);
    return error;
  }

  scoped_refptr<DocumentScanScanFunction> function_;
};

TEST_F(DocumentScanScanFunctionTest, UserGestureRequiredError) {
  function_->set_user_gesture(false);
  EXPECT_EQ("User gesture required to perform scan",
            RunFunctionAndReturnError("[{}]"));
}

}  // namespace api

}  // namespace extensions
