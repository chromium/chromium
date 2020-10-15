// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

constexpr char kManifestStub[] =
    R"({
         "name": "extension",
         "version": "0.1",
         "manifest_version": 2,
         "background": { "scripts": ["background.js"] }
       })";

}  // namespace

class TestAPITest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ExtensionApiTest::ContextType> {
 public:
  TestAPITest() {
    // Service Workers are currently only available on certain channels, so set
    // the channel for those tests.
    if (GetParam() == ContextType::kServiceWorker)
      current_channel_ = std::make_unique<ScopedWorkerBasedExtensionsChannel>();
  }
  ~TestAPITest() override = default;

  // Loads and returns an extension with the given |background_script|.
  const Extension* LoadExtensionWithBackgroundScript(
      const char* background_script);

 private:
  std::vector<std::unique_ptr<TestExtensionDir>> test_dirs_;
  std::unique_ptr<ScopedWorkerBasedExtensionsChannel> current_channel_;
};

const Extension* TestAPITest::LoadExtensionWithBackgroundScript(
    const char* background_script) {
  auto test_dir = std::make_unique<TestExtensionDir>();
  test_dir->WriteManifest(kManifestStub);
  test_dir->WriteFile(FILE_PATH_LITERAL("background.js"), background_script);
  const Extension* extension = LoadExtension(test_dir->UnpackedPath());
  test_dirs_.push_back(std::move(test_dir));
  return extension;
}

// TODO(devlin): This test name should be more descriptive.
IN_PROC_BROWSER_TEST_P(TestAPITest, ApiTest) {
  ASSERT_TRUE(RunExtensionTest("apitest")) << message_;
}

// Verifies that failing an assert in a promise will properly fail and end the
// test.
IN_PROC_BROWSER_TEST_P(TestAPITest, FailedAssertsInPromises) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function failedAssertsInPromises() {
             let p = new Promise((resolve, reject) => {
               chrome.test.assertEq(1, 2);
               resolve();
             });
             p.then(() => { chrome.test.succeed(); });
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithBackgroundScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ("Failed 1 of 1 tests", result_catcher.message());
}

// Verifies that using await and assert'ing aspects of the results succeeds.
IN_PROC_BROWSER_TEST_P(TestAPITest, AsyncAwaitAssertions_Succeed) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function asyncAssertions() {
             let tabs = await new Promise((resolve) => {
               chrome.tabs.query({}, resolve);
             });
             chrome.test.assertTrue(tabs.length > 0);
             chrome.test.succeed();
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithBackgroundScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that using await and having failed assertions properly fails the
// test.
IN_PROC_BROWSER_TEST_P(TestAPITest, AsyncAwaitAssertions_Failed) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function asyncAssertions() {
             let tabs = await new Promise((resolve) => {
               chrome.tabs.query({}, resolve);
             });
             chrome.test.assertEq(0, tabs.length);
             chrome.test.succeed();
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithBackgroundScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ("Failed 1 of 1 tests", result_catcher.message());
}

// Verifies that we can assert values on chrome.runtime.lastError after using
// await with an API call.
IN_PROC_BROWSER_TEST_P(TestAPITest, AsyncAwaitAssertions_LastError) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function asyncAssertions() {
             let result = await new Promise((resolve) => {
               const nonexistentId = 99999;
               chrome.tabs.update(
                   nonexistentId, {url: 'https://example.com'}, resolve);
             });
             chrome.test.assertLastError('No tab with id: 99999.');
             chrome.test.succeed();
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithBackgroundScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

INSTANTIATE_TEST_SUITE_P(
    EventPage,
    TestAPITest,
    ::testing::Values(ExtensionApiTest::ContextType::kEventPage));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    TestAPITest,
    ::testing::Values(ExtensionApiTest::ContextType::kServiceWorker));

}  // namespace extensions
