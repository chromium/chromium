// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"

#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The extension's code can be found in
// chrome/test/data/extensions/api_test/login_screen_apis/
const char kExtensionId[] = "oclffehlkdgibkainkilopaalpdobkan";
const char kExtensionUpdateManifestPath[] =
    "/extensions/api_test/login_screen_apis/update_manifest.xml";

// The test extension will query for a test to run using this message.
const char kWaitingForTestName[] = "Waiting for test name";

}  // namespace

namespace chromeos {

LoginScreenApitestBase::LoginScreenApitestBase(version_info::Channel channel)
    : SigninProfileExtensionsPolicyTestBase(channel),
      extension_id_(kExtensionId),
      extension_update_manifest_path_(kExtensionUpdateManifestPath),
      listener_message_(kWaitingForTestName) {}

LoginScreenApitestBase::~LoginScreenApitestBase() {
  ClearTestListeners();
}

void LoginScreenApitestBase::SetUpTestListeners() {
  catcher_ = std::make_unique<extensions::ResultCatcher>();
  listener_ = std::make_unique<ExtensionTestMessageListener>(
      listener_message_, ReplyBehavior::kWillReply);
}

void LoginScreenApitestBase::ClearTestListeners() {
  catcher_.reset();
  listener_.reset();
}

void LoginScreenApitestBase::RunTest(const std::string& test_name) {
  RunTest(test_name, /*assert_test_succeed=*/true);
}

void LoginScreenApitestBase::RunTest(const std::string& test_name,
                                     bool assert_test_succeed) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(test_name);

  if (assert_test_succeed)
    ASSERT_TRUE(catcher_->GetNextResult());
}

void LoginScreenApitestBase::SetUpLoginScreenExtensionAndRunTest(
    const std::string& test_name) {
  SetUpLoginScreenExtensionAndRunTest(test_name, /*assert_test_succeed=*/true);
}

void LoginScreenApitestBase::SetUpLoginScreenExtensionAndRunTest(
    const std::string& test_name,
    bool assert_test_succeed) {
  SetUpTestListeners();
  AddExtensionForForceInstallation(extension_id_,
                                   extension_update_manifest_path_);
  RunTest(test_name, assert_test_succeed);
}

}  // namespace chromeos
