// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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

using ContextType = extensions::browser_test_util::ContextType;

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

#if !BUILDFLAG(IS_ANDROID)
// Android only supports service worker.
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         TestAPITestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
#endif
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         TestAPITestWithContextType,
                         ::testing::Values(ContextType::kServiceWorker));

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
             let allowed = await new Promise((resolve) => {
               chrome.extension.isAllowedIncognitoAccess(resolve);
             });
             chrome.test.assertFalse(allowed);
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
             let allowed = await new Promise((resolve) => {
               chrome.extension.isAllowedIncognitoAccess(resolve);
             });
             chrome.test.assertTrue(allowed);
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
             // Wrapper types.
             chrome.test.assertNe(new Boolean(true), new Boolean(false));
             chrome.test.assertNe(new Number(1), new Number(2));
             chrome.test.assertNe(new String('a'), new String('b'));
             chrome.test.assertNe(new Date(100), new Date(200));
             chrome.test.assertNe(new ArrayBuffer(8), new ArrayBuffer(16));
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

// Exercises `chrome.test.assertEq()` in cases where the assert should succeed
// (that is, when the passed values are the same).
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, AssertEq_Success) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertEqTestsWithPrimitiveTypes() {
             chrome.test.assertEq(42, 42);
             chrome.test.assertEq(false, false);
             chrome.test.assertEq(3.14, 3.14);
             chrome.test.assertEq('chromium', 'chromium');
             chrome.test.assertEq(null, null);
             chrome.test.assertEq(NaN, NaN);
             chrome.test.succeed();
           },
           function assertEqTestsWithObjects() {
             // Object Tests
             chrome.test.assertEq([], []);
             chrome.test.assertEq({}, {});
             chrome.test.assertEq({x: 42}, {x: 42});
             chrome.test.assertEq({x: 1}, {x: 1});

             // Object keys in different order
             chrome.test.assertEq({a: 1, b: 2}, {b: 2, a: 1});

             // Array Tests
             chrome.test.assertEq([1, "a", true], [1, "a", true]);

             // Sparse Array (Array with empty slots)
             const sparse1 = [1, , 3];
             const sparse2 = [1, , 3];
             chrome.test.assertEq(sparse1, sparse2);

             // Map Tests
             // Standard Map with primitive keys/values
             const map1 = new Map([['a', 1], ['b', 2]]);
             const map2 = new Map([['a', 1], ['b', 2]]);
             chrome.test.assertEq(map1, map2);

             // Map keys in different order
             const map3 = new Map([['a', 1], ['b', 2]]);
             const map4 = new Map([['b', 2], ['a', 1]]);
             chrome.test.assertEq(map3, map4);

             // Set Tests
             const set1 = new Set([1, 2, 3]);
             const set2 = new Set([1, 2, 3]);
             chrome.test.assertEq(set1, set2);

             // Set values in different order
             const set3 = new Set([1, 3, 2]);
             const set4 = new Set([1, 2, 3]);
             chrome.test.assertEq(set3, set4);

             // Function Tests
             const func1 = function() { return 1; };
             const func2 = function() { return 1; };
             chrome.test.assertEq(func1, func2);

             // Wrapper types.
             chrome.test.assertEq(new Boolean(true), new Boolean(true));
             chrome.test.assertEq(new Number(1), new Number(1));
             chrome.test.assertEq(Number("a"), Number("b"));
             chrome.test.assertEq(new Number(NaN), new Number(NaN));
             chrome.test.assertEq(new String("a"), new String("a"));
             chrome.test.assertEq(new Date(100), new Date(100));
             chrome.test.assertEq(new Date(NaN), new Date(NaN));

             // ArrayBuffer tests.
             let ab1 = new ArrayBuffer(8);
             let ab2 = new ArrayBuffer(8);
             chrome.test.assertEq(ab1, ab2);
             // Nested ArrayBuffer.
             chrome.test.assertEq({buf: ab1}, {buf: ab2});

             chrome.test.succeed();
           },
           function assertEqTestsWithErrorMessage() {
             chrome.test.assertEq(2, 2, '2 equals 2');
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Exercises `chrome.test.assertEq()` in failure cases (i.e., the passed values
// are not equal). Test one case at a time since "failure" means that the assert
// worked as expected.
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, AssertEq_Failure) {
  struct {
    std::string title;
    std::string code;
  } test_cases[] = {
      {"Primitive", "chrome.test.assertEq(4, 2);"},
      {"Object", "chrome.test.assertEq({x: 2}, {x: 42});"},
      {"Sparse Array", "chrome.test.assertEq([1, , 3], [1, , 4]);"},
      {"Map w/ Object value",
       "chrome.test.assertEq(new Map([['key', { deep: true }]]), "
       "new Map([['key', { deep: false }]]));"},
      {"Set", "chrome.test.assertEq(new Set([1, 2, 3]), new Set([1, 2, 4]));"},
      {"Different Functions",
       "chrome.test.assertEq(function() { return 1; }, function() { return 2; "
       "});"},
      {"ArrayBuffer",
       "chrome.test.assertEq(new ArrayBuffer(8), new ArrayBuffer(16));"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    ResultCatcher result_catcher;
    std::string script = base::StringPrintf(
        R"(chrome.test.runTests([
            function assertEqTest() {
              %s
            },
          ]);)",
        test_case.code);
    ASSERT_TRUE(LoadExtensionScriptWithContext(script.c_str(), GetParam()));
    EXPECT_FALSE(result_catcher.GetNextResult());
    EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
  }
}

IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, AssertEq_UndefinedVsNull) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertEqUndefinedVsNull() {
             chrome.test.assertEq(null, undefined);
             chrome.test.assertEq(undefined, null);
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  // TODO(crbug.com/466303357): JS `null` and `undefined` should not be
  // considered equal. This seems to be because
  // `APISignature::ConvertArgumentsIgnoringSchema()` converts non-JSON
  // serializable arguments to empty `base::Value>()`s which convert to JS
  // `null`;
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Exercises `chrome.test.assertEq()` with complex structures, ensuring that JS
// primitives, `NaN`, `null`, and `function`s are handled correctly within
// nested objects and arrays.
IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType,
                       RecursiveCheckDeepAssertEq_Success) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function recursiveCheckDeepAssertEqTests() {
             const obj1 = {
               a: 1,
               b: 'string',
               c: null,
               d: NaN,
               e: {
                 nested: true,
                 array: [1, 2, { deep: 'value' }]
               },
               f: function() { return 'test'; }
             };
             const obj2 = {
               a: 1,
               b: 'string',
               c: null,
               d: NaN,
               e: {
                 nested: true,
                 array: [1, 2, { deep: 'value' }]
               },
               f: function() { return 'test'; }
             };
             chrome.test.assertEq(obj1, obj2);

             // Test with Maps having complex keys/values
             const map1 = new Map();
             map1.set({key: 'complex'}, {value: 'complex'});
             const map2 = new Map();
             map2.set({key: 'complex'}, {value: 'complex'});
             chrome.test.assertEq(map1, map2);

             // Test with Sets having complex values
             const set1 = new Set([{a: 1}, {b: 2}]);
             const set2 = new Set([{a: 1}, {b: 2}]);
             chrome.test.assertEq(set1, set2);

             // Map with Object as value
             const mapObj1 = new Map([['key', { deep: true }]]);
             const mapObj2 = new Map([['key', { deep: true }]]);
             chrome.test.assertEq(mapObj1, mapObj2);

             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, ListenOnceWithoutPromise) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(let createdTab;
         let invokedCalls = 0;
         chrome.test.runTests([
           async function performListenOnceWithoutPromise() {
             // Set up a `listenOnce` listener. The test should not complete
             // until this listener is resolved (exactly once), and it should
             // run the callback within.
             let result = chrome.test.listenOnce(chrome.tabs.onCreated,
                                                 function(tab) {
               createdTab = tab;
               // The test should end after this.
             });
             // When passed a callback, listenOnce() should not return anything.
             chrome.test.assertEq(undefined, result);
             // There should be an onCreated listener from the call above.
             chrome.test.assertTrue(chrome.tabs.onCreated.hasListeners());
             // Trigger the event, which will trigger the listenOnce handler,
             // which should end the test.
             chrome.tabs.create({});
           },
           async function verifyState() {
             // Now, we verify the expected end state.
             // The callback passed to listenOnce should have been invoked,
             // which we verify by checking the value of `createdTab`:
             chrome.test.assertTrue(!!createdTab);
             // And verify it smells like a tab.
             chrome.test.assertTrue(createdTab.id >= 0);
             // The listener for tabs.onCreated also should have been removed.
             chrome.test.assertFalse(chrome.tabs.onCreated.hasListeners());

             chrome.test.succeed();
           },
           async function multiListeners() {
             // Test that listenOnce() adds a callback to the pending callback
             // count, so the test will only pass once each listener is
             // validated.
             chrome.test.listenOnce(chrome.tabs.onCreated, function() {
               ++invokedCalls;
             });
             chrome.test.listenOnce(chrome.tabs.onMoved, function() {
               ++invokedCalls;
             });
             // Trigger the first listener. The test shouldn't finish, since
             // there's still a pending callback with the second listener.
             let tab = await chrome.tabs.create({});
             chrome.test.assertTrue(!!tab);
             chrome.test.assertEq(1, invokedCalls);
             // Move the tab to the previous index; this will trigger the second
             // listener, ending the test.
             chrome.tabs.move(tab.id, {index: 0});
           },
           async function verifyMultiListenerState() {
             chrome.test.assertEq(2, invokedCalls);
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_P(TestAPITestWithContextType, ListenOnceWithPromise) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function listenOnceWithPromise() {
             // Set up a `listenOnce` listener. The test should not complete
             // until this listener is resolved (exactly once), and it should
             // run the callback within.
             let eventPromise = chrome.test.listenOnce(chrome.tabs.onCreated);
             chrome.test.assertTrue(!!eventPromise);

             // There should be an onCreated listener from the call above.
             chrome.test.assertTrue(chrome.tabs.onCreated.hasListeners());

             // Trigger the event, which will trigger the listenOnce handler,
             // which should end the test.
             chrome.tabs.create({});

             let createdTab = await eventPromise;

             // The promise should resolve with the created tab.
             chrome.test.assertTrue(!!createdTab);
             // Verify it smells like a tab.
             chrome.test.assertTrue(createdTab.id >= 0,
                                    JSON.stringify(createdTab));

             // The listener for tabs.onCreated also should have been removed.
             chrome.test.assertFalse(chrome.tabs.onCreated.hasListeners());

             chrome.test.succeed();
           },

           async function listenOnceWithPromiseAndMultipleEventArgs() {
             // This test is similar to the above, but with an event that
             // fires with multiple arguments. In this case, the returned
             // promise is resolved with an array containing the arguments.
             let eventPromise = chrome.test.listenOnce(chrome.tabs.onMoved);

             let tabs = await chrome.tabs.query({});
             // There should be an extra tab from the test above.
             chrome.test.assertTrue(tabs.length > 1);
             let tab = tabs.find((tab) => { return tab.index == 0; });
             chrome.test.assertTrue(!!tab);
             // Move the tab to the second index, triggering the event.
             chrome.tabs.move(tab.id, {index: 1});

             let args = await eventPromise;
             // chrome.tabs.onMoved has two arguments...
             chrome.test.assertTrue(Array.isArray(args));
             chrome.test.assertEq(2, args.length);
             // The first argument is the tab ID.
             chrome.test.assertEq(tab.id, args[0]);

             // The second argument is `moveInfo`; verify it smells like it.
             chrome.test.assertTrue(!!args[1]);
             chrome.test.assertEq(args[1].fromIndex, 0);
             chrome.test.assertEq(args[1].toIndex, 1);

             chrome.test.succeed();
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionScriptWithContext(kBackgroundJs, GetParam()));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

}  // namespace extensions
