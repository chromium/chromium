// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

constexpr char kManifestStub[] =
    R"({
         "name": "extension",
         "version": "0.1",
         "manifest_version": %d,
         "background": { %s }
       })";

constexpr char kPersistentBackground[] = R"("scripts": ["background.js"])";

constexpr char kServiceWorkerBackground[] =
    R"("service_worker": "background.js")";

// NOTE(devlin): When running tests using the chrome.tests.runTests API, it's
// not possible to validate the failure message of individual sub-tests using
// the ResultCatcher interface. This is because the test suite always fail with
// an error message like `kExpectedFailureMessage` below without any
// information about the failure of the individual sub-tests. If we expand this
// suite significantly, we should investigate having more information available
// on the C++ side, so that we can assert failures with more specificity.
// TODO(devlin): Investigate using WebContentsConsoleObserver to watch for
// specific errors / patterns.
constexpr char kExpectedFailureMessage[] = "Failed 1 of 1 tests";

}  // namespace

using ContextType = ExtensionApiTest::ContextType;

class TestAPITest : public ExtensionApiTest {
 protected:
  const Extension* LoadExtensionScriptWithContext(const char* background_script,
                                                  ContextType context_type,
                                                  int manifest_version);

  std::vector<TestExtensionDir> test_dirs_;
};

const Extension* TestAPITest::LoadExtensionScriptWithContext(
    const char* background_script,
    ContextType context_type,
    int manifest_version = 2) {
  TestExtensionDir test_dir;
  const char* background_value = context_type == ContextType::kServiceWorker
                                     ? kServiceWorkerBackground
                                     : kPersistentBackground;
  const std::string manifest =
      base::StringPrintf(kManifestStub, manifest_version, background_value);
  test_dir.WriteManifest(manifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background_script);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  test_dirs_.push_back(std::move(test_dir));
  return extension;
}

class TestAPITestWithContextType
    : public TestAPITest,
      public testing::WithParamInterface<ContextType> {};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    TestAPITestWithContextType,
    ::testing::Values(ExtensionApiTest::ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    TestAPITestWithContextType,
    ::testing::Values(ExtensionApiTest::ContextType::kServiceWorker));

// TODO(devlin): This test name should be more descriptive.
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, ApiTest) {
  ASSERT_TRUE(RunExtensionTest("apitest", {}, {.context_type = GetParam()}))
      << message_;
}

// Verifies that failing an assert in a promise will properly fail and end the
// test.
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, FailedAssertsInPromises) {
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
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that using await and assert'ing aspects of the results succeeds.
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType,
                       AsyncAwaitAssertions_Succeed) {
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
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that using await and having failed assertions properly fails the
// test.
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType,
                       AsyncAwaitAssertions_Failed) {
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
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, AsyncExceptions) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function asyncExceptions() {
             throw new Error('test error');
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Exercises chrome.test.assertNe() in cases where the check should succeed
// (that is, when the passed values are different).
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, AssertNe_Success) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertNeTestsWithPrimitiveTypes() {
             chrome.test.assertNe(1, 2);
             chrome.test.assertNe(2, 1);
             chrome.test.assertNe(true, false);
             chrome.test.assertNe(1.8, 2.4);
             chrome.test.assertNe('tolstoy', 'dostoyevsky');
             chrome.test.succeed();
           },
           function assertNeTestsWithObjects() {
             chrome.test.assertNe([], [1]);
             chrome.test.assertNe({x: 1}, {x: 2});
             chrome.test.assertNe({x: 1}, {y: 1});
             chrome.test.assertNe({}, []);
             chrome.test.assertNe({}, 'Object object');
             chrome.test.assertNe({}, '{}');
             chrome.test.assertNe({}, null);
             chrome.test.assertNe(null, {});
             chrome.test.succeed();
           },
           function assertNeTestsWithErrorMessage() {
             chrome.test.assertNe(3, 2, '3 does not equal 2');
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Exercises chrome.test.assertNe() in failure cases (i.e., the passed values
// are equal). We can only test one case at a time since otherwise we'd be
// unable to determine which part of the test failed (since "failure" here is
// a successful assertNe() check).
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, AssertNe_Failure_Primitive) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertNeTestsWithPrimitiveTypes() {
             chrome.test.assertNe(1, 1);
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Exercises chrome.test.assertNe() in failure cases (i.e., the passed values
// are equal). We can only test one case at a time since otherwise we'd be
// unable to determine which part of the test failed (since "failure" here is
// a successful assertNe() check).
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, AssertNe_Failure_Object) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertNeTestsWithObjectTypes() {
             chrome.test.assertNe({x: 42}, {x: 42});
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Exercises chrome.test.assertNe() in failure cases (i.e., the passed values
// are equal). We can only test one case at a time since otherwise we'd be
// unable to determine which part of the test failed (since "failure" here is
// a successful assertNe() check).
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType,
                       AssertNe_Failure_AdditionalErrorMessage) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertNeTestsWithAdditionalErrorMessage() {
             chrome.test.assertNe(2, 2, '2 does equal 2');
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that chrome.test.assertPromiseRejects() succeeds using
// promises that reject with the expected message.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertPromiseRejects_Successful) {
  ResultCatcher result_catcher;
  constexpr char kWorkerJs[] =
      R"(const TEST_ERROR = 'Expected Error';
         chrome.test.runTests([
           async function successfulAssert_PromiseAlreadyRejected() {
             let p = Promise.reject(TEST_ERROR);
             await chrome.test.assertPromiseRejects(p, TEST_ERROR);
             chrome.test.succeed();
           },
           async function successfulAssert_PromiseRejectedLater() {
             let rejectPromise;
             let p = new Promise(
                 (resolve, reject) => { rejectPromise = reject; });
             let assertPromise =
                 chrome.test.assertPromiseRejects(p, TEST_ERROR);
             rejectPromise(TEST_ERROR);
             assertPromise.then(() => {
               chrome.test.succeed();
             }).catch(e => {
               chrome.test.fail(e);
             });
           },
           async function successfulAssert_RegExpMatching() {
             const regexp = /.*pect.*rror/;
             chrome.test.assertTrue(regexp.test(TEST_ERROR));
             let p = Promise.reject(TEST_ERROR);
             await chrome.test.assertPromiseRejects(p, regexp);
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kWorkerJs,
                                             ContextType::kServiceWorker,
                                             /*manifest_version=*/3));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Tests that chrome.test.assertPromiseRejects() properly fails the test when
// the promise is rejected with an improper message.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertPromiseRejects_WrongErrorMessage) {
  ResultCatcher result_catcher;
  constexpr char kWorkerJs[] =
      R"(chrome.test.runTests([
           async function failedAssert_WrongErrorMessage() {
             let p = Promise.reject('Wrong Error');
             await chrome.test.assertPromiseRejects(p, 'Expected Error');
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kWorkerJs,
                                             ContextType::kServiceWorker,
                                             /*manifest_version=*/3));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Tests that chrome.test.assertPromiseRejects() properly fails the test when
// the promise resolves instead of rejects.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertPromiseRejects_PromiseResolved) {
  ResultCatcher result_catcher;
  constexpr char kWorkerJs[] =
      R"(chrome.test.runTests([
           async function failedAssert_PromiseResolved() {
             let p = Promise.resolve(42);
             await chrome.test.assertPromiseRejects(p, 'Expected Error');
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kWorkerJs,
                                             ContextType::kServiceWorker,
                                             /*manifest_version=*/3));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Tests that finishing the test without waiting for the result of
// chrome.test.assertPromiseRejects() properly fails the test.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertPromiseRejects_PromiseIgnored) {
  ResultCatcher result_catcher;
  constexpr char kWorkerJs[] =
      R"(chrome.test.runTests([
           async function failedAssert_PromiseIgnored() {
             let p = new Promise((resolve, reject) => { });
             chrome.test.assertPromiseRejects(p, 'Expected Error');
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kWorkerJs,
                                             ContextType::kServiceWorker,
                                             /*manifest_version=*/3));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Tests that chrome.test.sendMessage() successfully sends a message to the C++
// side and can receive a response back using a promise.
IN_PROC_BROWSER_TEST_F(TestAPITest, SendMessage_WithPromise) {
  ResultCatcher result_catcher;
  constexpr char kWorkerJs[] =
      R"(chrome.test.runTests([
           async function sendMessageWithPromise() {
             let response = await chrome.test.sendMessage('ping');
             chrome.test.assertEq('pong', response);
             chrome.test.succeed();
           },
         ]);)";
  ExtensionTestMessageListener ping_listener("ping", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtensionScriptWithContext(kWorkerJs,
                                             ContextType::kServiceWorker,
                                             /*manifest_version=*/3));
  EXPECT_TRUE(ping_listener.WaitUntilSatisfied());
  ping_listener.Reply("pong");
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Tests that calling chrome.test.waitForRountTrip() eventually comes back with
// the same message when using promises. Note: this does not verify that the
// message actually passes through the renderer process, it just tests the
// surface level from the Javascript side.
IN_PROC_BROWSER_TEST_F(TestAPITest, WaitForRoundTrip_WithPromise) {
  ResultCatcher result_catcher;
  constexpr char kWorkerJs[] =
      R"(chrome.test.runTests([
           async function waitForRoundTripWithPromise() {
             let response = await chrome.test.waitForRoundTrip('arrivederci');
             chrome.test.assertEq('arrivederci', response);
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kWorkerJs,
                                             ContextType::kServiceWorker,
                                             /*manifest_version=*/3));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

}  // namespace extensions
