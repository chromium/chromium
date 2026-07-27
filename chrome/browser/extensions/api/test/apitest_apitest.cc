// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr char kManifest[] =
    R"({
         "name": "extension",
         "version": "0.1",
         "manifest_version": 3,
         "background": { "service_worker": "background.js" }
       })";

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
constexpr char kExpectedFailureMessageOneOfTwoTests[] = "Failed 1 of 2 tests";

}  // namespace

class TestAPITest : public ExtensionApiTest {
 protected:
  const Extension* LoadExtensionWithScript(const char* background_script);

  std::vector<TestExtensionDir> test_dirs_;
};

const Extension* TestAPITest::LoadExtensionWithScript(
    const char* background_script) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background_script);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  test_dirs_.push_back(std::move(test_dir));
  return extension;
}

// TODO(devlin): This test name should be more descriptive.
IN_PROC_BROWSER_TEST_F(TestAPITest, ApiTest) {
  ASSERT_TRUE(RunExtensionTest("apitest")) << message_;
}

// Verifies that failing an assert in a promise will properly fail and end the
// test.
IN_PROC_BROWSER_TEST_F(TestAPITest, FailedAssertsInPromises) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that using await and assert'ing aspects of the results succeeds.
IN_PROC_BROWSER_TEST_F(TestAPITest, AsyncAwaitAssertions_Succeed) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that using await and having failed assertions properly fails the
// test.
IN_PROC_BROWSER_TEST_F(TestAPITest, AsyncAwaitAssertions_Failed) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

IN_PROC_BROWSER_TEST_F(TestAPITest, AsyncExceptions) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function asyncExceptions() {
             throw new Error('test error');
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Exercises chrome.test.assertNe() in cases where the check should succeed
// (that is, when the passed values are different).
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertNe_Success) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Exercises chrome.test.assertNe() in failure cases (i.e., the passed values
// are equal). We can only test one case at a time since otherwise we'd be
// unable to determine which part of the test failed (since "failure" here is
// a successful assertNe() check).
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertNe_Failure_Primitive) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertNeTestsWithPrimitiveTypes() {
             chrome.test.assertNe(1, 1);
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Exercises chrome.test.assertNe() in failure cases (i.e., the passed values
// are equal). We can only test one case at a time since otherwise we'd be
// unable to determine which part of the test failed (since "failure" here is
// a successful assertNe() check).
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertNe_Failure_Object) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertNeTestsWithObjectTypes() {
             chrome.test.assertNe({x: 42}, {x: 42});
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that assertTrue fails when passed a non-boolean.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertTrue_TypeCheck) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertTrueTypeCheck() {
             chrome.test.assertTrue(1);
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that assertFalse fails when passed a non-boolean.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertFalse_TypeCheck) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertFalseTypeCheck() {
             chrome.test.assertFalse(0);
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}
// Exercises chrome.test.assertNe() in failure cases (i.e., the passed values
// are equal). We can only test one case at a time since otherwise we'd be
// unable to determine which part of the test failed (since "failure" here is
// a successful assertNe() check).
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertNe_Failure_AdditionalErrorMessage) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertNeTestsWithAdditionalErrorMessage() {
             chrome.test.assertNe(2, 2, '2 does equal 2');
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
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
  ASSERT_TRUE(LoadExtensionWithScript(kWorkerJs));
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
  ASSERT_TRUE(LoadExtensionWithScript(kWorkerJs));
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
  ASSERT_TRUE(LoadExtensionWithScript(kWorkerJs));
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
  ASSERT_TRUE(LoadExtensionWithScript(kWorkerJs));
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
  ASSERT_TRUE(LoadExtensionWithScript(kWorkerJs));
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
  ASSERT_TRUE(LoadExtensionWithScript(kWorkerJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Exercises `chrome.test.assertEq()` in cases where the assert should succeed
// (that is, when the passed values are the same).
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertEq_Success) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Exercises `chrome.test.assertEq()` in failure cases (i.e., the passed values
// are not equal). Test one case at a time since "failure" means that the assert
// worked as expected.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertEq_Failure) {
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
    ASSERT_TRUE(LoadExtensionWithScript(script.c_str()));
    EXPECT_FALSE(result_catcher.GetNextResult());
    EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
  }
}

IN_PROC_BROWSER_TEST_F(TestAPITest, AssertEq_UndefinedVsNull) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function assertEqUndefinedVsNull() {
             chrome.test.assertEq(null, undefined);
             chrome.test.assertEq(undefined, null);
             chrome.test.succeed();
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
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
IN_PROC_BROWSER_TEST_F(TestAPITest, RecursiveCheckDeepAssertEq_Success) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(TestAPITest, ListenOnceWithoutPromise) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(TestAPITest, ListenOnceWithPromise) {
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
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Tests that sequential calls to runTests() properly reset internal states. To
// verify this, we run a passing batch, a failing batch, and another passing
// batch. If internal state (like `testsFailed`) was not reset, the final
// passing batch would incorrectly fail.
IN_PROC_BROWSER_TEST_F(TestAPITest, RunTestsSuccessiveAwaits) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(async function testEntryPoint() {
           await chrome.test.runTests([
             function test1() { chrome.test.succeed(); }
           ]);

           try {
             await chrome.test.runTests([
               function test2() { chrome.test.fail('intentional failure'); }
             ]);
           } catch (e) {
             // Prevent the error thrown from `runTests()` `Promise` rejecting
             // from stopping the JS thread.
           }

           await chrome.test.runTests([
             function test3() {
               chrome.test.succeed();
             }
           ]);
         }
         testEntryPoint();)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));

  // Batch 1 passes.
  EXPECT_TRUE(result_catcher.GetNextResult());

  // Batch 2 fails.
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ("Failed 1 of 1 tests", result_catcher.message());

  // Batch 3 passes. If internal state was not reset, batch 3 would incorrectly
  // fail because `testsFailed` from batch 2 would still be 1.
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that `chrome.test.assertThrows` succeeds when the passed function
// throws an error that matches the expectations, using the signature
// `assertThrows(fn, expectedError?, message?)`.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Success_NoArguments) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests that assertThrows succeeds when the function throws and
            * the error matches the expected value (or no expected value is
            * provided).
            */
           function testAssertThrowsSuccessNoArguments() {
             // Assert that an error is thrown (without error matching).
             chrome.test.assertThrows(() => {
               throw new Error('foo');
             });

             // Assert that the thrown error message matches the expected
             // string.
             chrome.test.assertThrows(() => {
               throw new Error('foo');
             }, 'foo');

             // Assert that the thrown error message matches the expected
             // RegExp.
             chrome.test.assertThrows(() => {
               throw new Error('foo');
             }, /fo+/);

             chrome.test.succeed();
           }
         ]);)";

  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that `chrome.test.assertThrows` succeeds when using arrow functions
// to wrap function calls that require arguments.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Success_WithArrowFunctions) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests that assertThrows succeeds when wrapping function calls that
            * require arguments inside arrow functions.
            */
           function testAssertThrowsSuccessWithArrowFunctions() {
             const obj = {
               func: function(a, b) {
                 if (a + b > 10) throw new Error('too big');
               }
             };
             chrome.test.assertThrows(() => obj.func(5, 6), 'too big');
             chrome.test.assertThrows(() => obj.func(5, 6), /too+/);
             chrome.test.succeed();
           }
         ]);)";

  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that `chrome.test.assertThrows` succeeds when using `bind`
// to wrap function calls that require arguments.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Success_WithBind) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests `assertThrows` succeeds with `bind` and args.
            */
           function testAssertThrowsSuccessWithBind() {
             const obj = {
               func: function(a, b) {
                 if (a + b > 10) throw new Error('too big');
               }
             };
             // Verify `assertThrows` succeeds with exact string match.
             chrome.test.assertThrows(obj.func.bind(obj, 5, 6), 'too big');
             // Verify `assertThrows` succeeds with regex match.
             chrome.test.assertThrows(obj.func.bind(obj, 5, 6), /too+/);
             chrome.test.succeed();
           }
         ]);)";

  // Load the test extension and verify all tests succeed.
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that `chrome.test.assertThrows` fails when the passed function
// does not throw any error.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Failure_NoThrow) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests that assertThrows fails when the function does not throw.
            */
           function testAssertThrowsFailure() {
             // This should fail because the function does not throw.
             chrome.test.assertThrows(() => {});
           }
         ]);)";

  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that `chrome.test.assertThrows` fails when the passed function
// throws an error that does not match the expected error.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Failure_WrongError) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests that assertThrows fails when the thrown error doesn't match
            * the expected error.
            */
           function testAssertThrowsFailure() {
             // This should fail because 'foo' does not match 'bar'.
             chrome.test.assertThrows(() => { throw new Error('foo'); }, 'bar');
           }
         ]);)";

  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that `chrome.test.assertThrows` aborts the test execution
// immediately when the assertion fails because the function did not throw.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Failure_Aborts) {
  ResultCatcher result_catcher;
  // Set up a listener for a message that should not be sent if the test aborts.
  ExtensionTestMessageListener listener("reached_end");
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests that assertThrows aborts test execution immediately upon
            * failure.
            */
           function testAssertThrowsFailureAborts() {
             // This assertion should fail and abort.
             chrome.test.assertThrows(() => {});
             // This should not be reached.
             chrome.test.sendMessage('reached_end');
           }
         ]);)";

  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  // Verify that the message was not sent, confirming immediate abort.
  EXPECT_FALSE(listener.was_satisfied());
}

// Verifies that `chrome.test.assertThrows` outputs the custom error message
// when the assertion fails.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Failure_CustomMessage) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests `assertThrows` includes custom message on failure.
            */
           function testAssertThrowsFailureCustomMessage() {
             // Intercept `chrome.test.fail` to capture failure messages.
             const originalFail = chrome.test.fail;
             let failureMessage = '';
             chrome.test.fail = function(message) {
               failureMessage = message;
             };

             // Verify failure message when the function does not throw.
             chrome.test.assertThrows(
                 () => {}, /* expectedError */ 'expected error',
                 /* message */ 'Custom failure message 1');
             chrome.test.assertEq(
                 'Custom failure message 1\nDid not throw error: () => {}',
                 failureMessage);

             // Verify failure message when the function throws the wrong error.
             chrome.test.assertThrows(
                 () => { throw new Error('actual'); },
                 /* expectedError */ 'expected',
                 /* message */ 'Custom failure message 2');
             chrome.test.assertEq(
                 'Custom failure message 2\n' +
                     'Expected error: "expected", actual: "actual"',
                 failureMessage);

             // Restore the original failure handler and succeed the test.
             chrome.test.fail = originalFail;
             chrome.test.succeed();
           }
         ]);)";

  // Load the test extension and verify all tests succeed.
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that `chrome.test.assertThrows` succeeds when the function throws
// falsy values (like `null` or `undefined`), both with and without
// `expectedError` matching.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Success_FalsyValues) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests that `assertThrows` succeeds when the function throws `null`
            * or `undefined`, and that these can be matched against their
            * `string` representations.
            */
           function testAssertThrowsSuccessFalsyValues() {
             // Assert that `assertThrows` succeeds when throwing `null` without
             // `expectedError`.
             chrome.test.assertThrows(() => {
               throw null;
             });
             // Assert that `assertThrows` succeeds when throwing `null` and
             // matching against `string` 'null'.
             chrome.test.assertThrows(() => {
               throw null;
             }, 'null');

             // Assert that `assertThrows` succeeds when throwing `undefined`
             // without `expectedError`.
             chrome.test.assertThrows(() => {
               throw undefined;
             });
             // Assert that `assertThrows` succeeds when throwing `undefined`
             // and matching against `string` 'undefined'.
             chrome.test.assertThrows(() => {
               throw undefined;
             }, 'undefined');

             chrome.test.succeed();
           }
         ]);)";

  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that `chrome.test.assertThrows` succeeds when the function throws
// non-`Error` objects (like `string`s or plain `Object`s), and that they can be
// matched using `string`s and `RegExp`s.
IN_PROC_BROWSER_TEST_F(TestAPITest, AssertThrows_Success_NonErrorObjects) {
  ResultCatcher result_catcher;
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           /**
            * Tests that `assertThrows` succeeds when the function throws a
            * `string` or a plain `Object`, and that they can be matched using
            * `string`s and `RegExp`s.
            */
           function testAssertThrowsSuccessNonErrorObjects() {
             // Assert that `assertThrows` succeeds when throwing a `string` and
             // matching against `string`.
             chrome.test.assertThrows(() => {
               throw 'custom error';
             }, 'custom error');
             // Assert that `assertThrows` succeeds when throwing a `string` and
             // matching against `RegExp`.
             chrome.test.assertThrows(() => {
               throw 'custom error';
             }, /custom.*/);

             // Assert that `assertThrows` succeeds when throwing a plain
             // `Object` with `message` property and matching against `string`.
             chrome.test.assertThrows(() => {
               throw {message: 'custom object error'};
             }, 'custom object error');
             // Assert that `assertThrows` succeeds when throwing a plain
             // `Object` with `message` property and matching against `RegExp`.
             chrome.test.assertThrows(() => {
               throw {message: 'custom object error'};
             }, /custom.*error/);

             // Assert that `assertThrows` succeeds when throwing a plain
             // `Object` without `message` property, it should be converted to
             // `string` '[object Object]'.
             chrome.test.assertThrows(() => {
               throw {foo: 'bar'};
             }, '[object Object]');

             chrome.test.succeed();
           }
         ]);)";

  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Note: these enums are the same, but are distinct for type safety and so they
// self-document when used to construct test cases.
enum class StandardizedOutcome {
  kPass,
  kFail,
};

enum class NonstandardizedOutcome {
  kPass,
  kFail,
};

// Allows testing the W3C browser.test proposal behavior
// (github.com/w3c/webextensions/blob/main/proposals/browser_test_api.md)
// ("standardized") vs existing "non-standardized" chrome.test API behavior.
class TestStandardizedAPITest : public TestAPITest,
                                public testing::WithParamInterface<bool> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TestAPITest::SetUpCommandLine(command_line);
    if (GetParam()) {
      command_line->AppendSwitch(
          switches::kExtensionTestApiStandardizedBehavior);
    }
  }
};

// Tests the differences between assertEq. The standardized version uses
// Object.is() more extensively.
IN_PROC_BROWSER_TEST_P(TestStandardizedAPITest, assertEq) {
  bool standardized_behavior_enabled = GetParam();

  struct {
    std::string title;
    std::string test_case;
    StandardizedOutcome should_pass_standardized;
    NonstandardizedOutcome should_pass_non_standardized;
  } cases[] = {
      {"Primitives", "chrome.test.assertEq(1, 1);", StandardizedOutcome::kPass,
       NonstandardizedOutcome::kPass},
      {"NaN", "chrome.test.assertEq(NaN, NaN);", StandardizedOutcome::kPass,
       NonstandardizedOutcome::kPass},
      {"0 vs -0", "chrome.test.assertNe(0, -0);", StandardizedOutcome::kPass,
       NonstandardizedOutcome::kFail},
      {"Arrays", "chrome.test.assertEq([1], [1]);", StandardizedOutcome::kPass,
       NonstandardizedOutcome::kPass},
      {"Plain Objects", "chrome.test.assertEq({a: 1}, {a: 1});",
       StandardizedOutcome::kPass, NonstandardizedOutcome::kPass},
      {"Primitive Wrappers",
       "chrome.test.assertEq(new Number(1), new Number(1));",
       StandardizedOutcome::kFail, NonstandardizedOutcome::kPass},
      {"Functions",
       "chrome.test.assertEq(function() { return 1; }, function() { return 1; "
       "});",
       StandardizedOutcome::kFail, NonstandardizedOutcome::kPass},
      {"Dates", "chrome.test.assertEq(new Date(100), new Date(100));",
       StandardizedOutcome::kFail, NonstandardizedOutcome::kPass},
      {"Maps", "chrome.test.assertEq(new Map(), new Map());",
       StandardizedOutcome::kFail, NonstandardizedOutcome::kPass},
      {"Sets", "chrome.test.assertEq(new Set(), new Set());",
       StandardizedOutcome::kFail, NonstandardizedOutcome::kPass},
  };

  for (const auto& c : cases) {
    SCOPED_TRACE(base::StringPrintf("Case: %s", c.title.c_str()));
    ResultCatcher result_catcher;
    // When `standardized_behavior_enabled` is true, the test relies on implicit
    // passing (returning undefined or a resolved Promise) and calling the JS
    // API `chrome.test.succeed()` is disallowed and will fail the test.
    // Otherwise, we must explicitly call `chrome.test.succeed()` to signal
    // pass.
    std::string script;
    if (standardized_behavior_enabled) {
      script = base::StringPrintf(
          R"(chrome.test.runTests([
               function test() {
                 %s
               }
             ]);)",
          c.test_case.c_str());
    } else {
      script = base::StringPrintf(
          R"(chrome.test.runTests([
               function test() {
                 %s
                 chrome.test.succeed();
               }
             ]);)",
          c.test_case.c_str());
    }

    ASSERT_TRUE(LoadExtensionWithScript(script.c_str()));

    bool expected_pass =
        standardized_behavior_enabled
            ? c.should_pass_standardized == StandardizedOutcome::kPass
            : c.should_pass_non_standardized == NonstandardizedOutcome::kPass;
    if (expected_pass) {
      EXPECT_TRUE(result_catcher.GetNextResult())
          << "Expected pass for " << c.title;
    } else {
      EXPECT_FALSE(result_catcher.GetNextResult())
          << "Expected fail for " << c.title;
      EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All, TestStandardizedAPITest, testing::Bool());

using TestStandardizedImplicitTestPassing = TestStandardizedAPITest;

// Verifies that a synchronous test with a passing assertion passes.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       SyncAssertion_Pass) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncPass() {
             chrome.test.assertTrue(true);
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that a synchronous test returning undefined passes.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       SyncUndefinedReturn_Pass) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncUndefinedPass() {}
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that a synchronous test with a failing assertion fails.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       SyncAssertion_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncFail() {
             chrome.test.assertTrue(false);
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that a test returning a resolved Promise passes.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       PromiseResolve_Pass) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           async function promisePass() {
             chrome.test.assertTrue(true);
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that a test returning a Promise that fails an assertion fails.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       PromiseAssertionFail_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           async function promiseFail() {
             chrome.test.assertTrue(false);
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that a test returning a rejected Promise fails.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       PromiseReject_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           async function promiseReject() {
             throw new Error('Rejected');
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that the test suite fails when a synchronous test case throws
// an uncaught error (first of two tests).
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       SyncThrowFirst_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncError() {
             throw new Error('fail');
           },
           function syncPass() {}
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessageOneOfTwoTests, result_catcher.message());
}

// Verifies that the test suite fails when a synchronous test case throws
// an uncaught error (second of two tests).
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       SyncThrowSecond_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncPass() {},
           function syncError() {
             throw new Error('fail');
           },
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessageOneOfTwoTests, result_catcher.message());
}

// Verifies that a synchronous test fails if it returns a non-undefined value.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       SyncReturnsValue_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncReturnsValue() {
             return 'some string';
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that a test returning a Promise that resolves asynchronously passes.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       PromiseAsyncResolve_Pass) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           async function promisePass() {
             await Promise.resolve();
             chrome.test.assertTrue(true);
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that a test returning a Promise that rejects asynchronously fails.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       PromiseAsyncReject_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           async function promiseReject() {
             await Promise.resolve();
             throw new Error('Rejected Async');
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that calling chrome.test.succeed() explicitly fails the test.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       ExplicitSucceed_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncPass() {
             chrome.test.succeed();
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that calling chrome.test.fail() explicitly fails the test.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing, ExplicitFail_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncFail() {
             chrome.test.fail('some message');
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// Verifies that a synchronous test fails if it returns undefined but has
// pending callbacks.
IN_PROC_BROWSER_TEST_P(TestStandardizedImplicitTestPassing,
                       SyncPendingCallbacks_Fail) {
  ResultCatcher result_catcher;
  constexpr char kBackgroundJs[] =
      R"(
         chrome.test.runTests([
           function syncPendingCallbacks() {
             // We use chrome.test.sendMessage here as an example of an API
             // that takes a callback (wrapped in callbackPass to increment
             // pendingCallbacks), but we are not testing sendMessage behavior.
             chrome.test.sendMessage('ping',
                 chrome.test.callbackPass(() => {}));
           }
         ]);)";
  ASSERT_TRUE(LoadExtensionWithScript(kBackgroundJs));
  EXPECT_FALSE(result_catcher.GetNextResult());
  EXPECT_EQ(kExpectedFailureMessage, result_catcher.message());
}

// We only test with the standardized behavior on because non-standardized
// testing behavior is already thoroughly covered by other test cases.
INSTANTIATE_TEST_SUITE_P(All,
                         TestStandardizedImplicitTestPassing,
                         testing::Values(true));

class TestHarnessEventsBrowserTest : public TestAPITest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TestAPITest::SetUpCommandLine(command_line);
    // Enabled the `chrome.test` API on web pages.
    command_line->AppendSwitch(switches::kExtensionTestApiOnWebPages);
  }

  // This script registers listeners for `onTestStarted` and `onTestFinished`
  // and asserts that they fire in the correct order and with the expected
  // arguments. It runs a single test named `test_name` that succeeds.
  std::string GetTestScript(const std::string& test_name) {
    return base::StringPrintf(
        R"(
           // Setup the listeners before running the test.
           // We use a counter instead of a boolean to ensure that
           // `chrome.test.onTestStarted` is only fired once. This prevents
           // regressions where the browser sends the event back to the source
           // process (which should be excluded via
           // `extensions::Event::exclude_process_id`).
           let onTestStartedFiredCount = 0;
           chrome.test.onTestStarted.addListener((info) => {
             if (info.testName === '%s') {
               onTestStartedFiredCount++;
             }
           });

           const finishedListener = (info) => {
             // Skip unknown test finished events.
             if (info.testName !== '%s') {
               return;
             }
             if (info.result === false) {
               // The test already failed, don't call chrome.test.fail() again
               // to avoid infinite recursion.
               return;
             }
             if (onTestStartedFiredCount === 1 &&
                 info.remainingTests === 0 &&
                 info.assertionDescription === '%s PASS') {
               // Send message indicating we successfully received the finished
               // event for this test.
               chrome.test.sendMessage('finished:' + info.testName);
             } else {
               chrome.test.fail('Unexpected info: ' + JSON.stringify(info) +
                                ', onTestStartedFiredCount: ' +
                                onTestStartedFiredCount);
             }
           };
           chrome.test.onTestFinished.addListener(finishedListener);

           // Run the test. The test passing means that the `onTestStarted`
           // event fired. `onTestFinished` is confirmed when it runs and sends
           // a message back to the test C++.
           chrome.test.runTests([
             function %s() {
               // Assert that `chrome.test.onTestStarted` fired exactly once.
               // If it fired more than once, it likely means the exclusion
               // logic in browser-side `extensions::EventRouter` failed.
               chrome.test.assertEq(1, onTestStartedFiredCount);
               chrome.test.succeed();
             }
           ]);)",
        test_name.c_str(), test_name.c_str(), test_name.c_str(),
        test_name.c_str());
  }
};

// Tests that `chrome.test.onTestStarted` and `chrome.test.onTestFinished` fire
// in the same script context where `chrome.test.runTests` is called.
IN_PROC_BROWSER_TEST_F(TestHarnessEventsBrowserTest, SameContext) {
  ResultCatcher result_catcher;

  // We will use ExtensionTestMessageListener to verify `onTestFinished` fires.
  // `onTestStarted` is confirmed to have run when the test case succeeds.
  // The JS will send "finished:<testName>".
  ExtensionTestMessageListener bg_listener("finished:backgroundTest");
  ExtensionTestMessageListener cs_listener("finished:contentScriptTest");
  ExtensionTestMessageListener page_listener("finished:pageTest");
  ExtensionTestMessageListener web_listener("finished:webPageTest");

  // Define the extension and web page tests.
  constexpr char kTestManifest[] =
      R"({
           "name": "test extension",
           "version": "1.0",
           "manifest_version": 3,
           "background": { "service_worker": "background.js" },
           "content_scripts": [{
             "matches": ["http://*/*"],
             "js": ["content_script.js"]
           }]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kTestManifest);
  std::string background_js = GetTestScript("backgroundTest");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background_js);
  std::string content_script_js = GetTestScript("contentScriptTest");
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), content_script_js);
  constexpr char kPageHtml[] = R"(<script src="page.js"></script>)";
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  std::string page_js = GetTestScript("pageTest");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), page_js);

  // Start embedded test server for the web page test.
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load the extension which runs the background script test.
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  EXPECT_TRUE(bg_listener.WaitUntilSatisfied());

  // Navigate to a web page. This will trigger the content script test.
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  EXPECT_TRUE(cs_listener.WaitUntilSatisfied());

  // Navigate to an extension page to run the test.
  GURL ext_url = extension->GetResourceURL("page.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), ext_url));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  EXPECT_TRUE(page_listener.WaitUntilSatisfied());

  // Navigate to a non-extension web page to run the web page context test.
  GURL web_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), web_url));
  std::string web_page_js = GetTestScript("webPageTest");
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), web_page_js));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  EXPECT_TRUE(web_listener.WaitUntilSatisfied());
}

// Tests that `chrome.test.onTestStarted` and `chrome.test.onTestFinished`
// events are broadcast to other listening script contexts outside of the script
// context that called `chrome.test.runTests`. In this case the script context
// that calls `chrome.test.runTests` is an extension background script and the
// broadcasted-to listening script context is a non-extension web page.
IN_PROC_BROWSER_TEST_F(TestHarnessEventsBrowserTest, CrossContextEvents) {
  ResultCatcher result_catcher;

  // Set up listeners for messages from the web page.
  // The web page will send `webpage_started` when it receives the
  // `chrome.test.onTestStarted` event, and `webpage_finished_success` when it
  // receives the `chrome.test.onTestFinished` event for the expected test.
  ExtensionTestMessageListener web_ready_listener("webpage_ready");
  ExtensionTestMessageListener web_listener_test_started_success(
      "webpage_test_started:testSuccess");
  ExtensionTestMessageListener web_listener_test_started_failure(
      "webpage_test_started:testFailure");
  ExtensionTestMessageListener web_listener_test_finished_success(
      "webpage_finished_success:testSuccess");
  ExtensionTestMessageListener web_listener_test_finished_failure(
      "webpage_finished_failure:testFailure");

  // Start the embedded test server to serve the web page.
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Navigate to a non-extension web page.
  GURL web_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), web_url));

  // Register test started and finished listeners on the web page. We verify
  // that these listeners are triggered by the test run in the extension
  // background script context.
  constexpr char kWebPageSetupJs[] = R"(
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`webpage_test_started:${info.testName}`);
    });
    chrome.test.onTestFinished.addListener((info) => {
      if (info.result === true) {
        chrome.test.sendMessage(`webpage_finished_success:${info.testName}`);
      } else {
        chrome.test.sendMessage(`webpage_finished_failure:${info.testName}`);
      }
    });

    chrome.test.sendMessage('webpage_ready');
  )";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), kWebPageSetupJs));
  EXPECT_TRUE(web_ready_listener.WaitUntilSatisfied());

  // Define the extension that will run the test.
  constexpr char kExtensionManifest[] =
      R"({
           "name": "test extension",
           "version": "1.0",
           "manifest_version": 3,
           "background": { "service_worker": "background.js" }
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kExtensionManifest);
  // The background script will run a successful and then a failing test to test
  // both event pathways.
  constexpr char kBackgroundJs[] = R"(
    chrome.test.runTests([
      function testSuccess() {
        chrome.test.succeed();
      },
      function testFailure() {
        chrome.test.fail();
      }
    ]);
  )";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  // Load the extension, which automatically starts running the tests in the
  // service worker context.
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Wait for the extension test to finish and verify. One test fails so we
  // expect false here.
  EXPECT_FALSE(result_catcher.GetNextResult()) << result_catcher.message();

  // Verify that the web page listeners were fired. We should see that the web
  // page loaded, it's listeners saw two started tests, then two finished tests
  // (one successful one failure).
  EXPECT_TRUE(web_ready_listener.WaitUntilSatisfied());
  EXPECT_TRUE(web_listener_test_started_success.WaitUntilSatisfied());
  EXPECT_TRUE(web_listener_test_started_failure.WaitUntilSatisfied());
  EXPECT_TRUE(web_listener_test_finished_success.WaitUntilSatisfied());
  EXPECT_TRUE(web_listener_test_finished_failure.WaitUntilSatisfied());
}

// Browser test harness verifying test start and finish event queueing
// behavior across process boundaries.
using ExtensionTestNotificationQueueBrowserTest = TestHarnessEventsBrowserTest;

// Verifies that test start and finish events emitted (before any event
// listeners are attached) are buffered in a browser process queue. When an
// observer subsequently registers in another renderer process, these queued
// events are dispatched in FIFO order.
IN_PROC_BROWSER_TEST_F(ExtensionTestNotificationQueueBrowserTest,
                       LateRegistration) {
  ResultCatcher result_catcher;

  // Start the embedded test server and navigate to a non-extension web page
  // before loading the extension or attaching any test start and finish
  // listeners.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL web_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), web_url));

  // Load and run the extension test suite without any test start and finish
  // listeners attached on the web page. The test start and finish events
  // should be queued inside the browser process event queue.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("queued_test_extension"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  // Set up waiters that will listen for the `onTestStarted` and
  // `onTestFinished` events to fire.
  ExtensionTestMessageListener test_started_event("Test started:myQueuedTest");
  ExtensionTestMessageListener test_finished_event(
      "Test finished:myQueuedTest");

  // Register one test start and one test finish listener on the web page after
  // the extension test has finished running.
  constexpr char kLateListenerJs[] = R"(
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`Test started:${info.testName}`);
    });
    chrome.test.onTestFinished.addListener((info) => {
      if (info.result === true) {
        chrome.test.sendMessage(`Test finished:${info.testName}`);
      }
    });
  )";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), kLateListenerJs));

  // Verify that when the browser process sees the listener additions, both
  // listeners receive the queued events.
  EXPECT_TRUE(test_started_event.WaitUntilSatisfied());
  EXPECT_TRUE(test_finished_event.WaitUntilSatisfied());
}

// Verifies that when multiple event listeners for test start events are
// attached right after one another in the same synchronous block on
// an observing web page after extension tests have completed, both listeners
// receive all queued events from the browser process queue. This is because
// both enter the renderer process local event listener map before the listener
// registration is sent to the browser process.
IN_PROC_BROWSER_TEST_F(ExtensionTestNotificationQueueBrowserTest,
                       ConsecutiveSynchronousListeners) {
  ResultCatcher result_catcher;

  // Start the embedded test server and navigate to the web page.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL web_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), web_url));

  // Load and execute the extension before any event listeners are attached.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("queued_test_extension"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  // Set up waiters that will listen for the two `onTestStarted` events to
  // fire.
  ExtensionTestMessageListener test_started_listener_1(
      "listener1:myQueuedTest");
  ExtensionTestMessageListener test_started_listener_2(
      "listener2:myQueuedTest");

  // Register two test start listeners synchronously on the web page right
  // after one another.
  constexpr char kSyncListenersJs[] = R"(
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`listener1:${info.testName}`);
    });
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`listener2:${info.testName}`);
    });
  )";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), kSyncListenersJs));

  // Verify that both listener callbacks successfully receive the queued event
  // when the browser process dispatches the event queue.
  EXPECT_TRUE(test_started_listener_1.WaitUntilSatisfied());
  EXPECT_TRUE(test_started_listener_2.WaitUntilSatisfied());
}

// Verifies that when a second test start event listener is attached
// asynchronously after the first listener on an observing web page after
// extension tests have completed, only the first listener receives the queued
// event. When the first listener registers with the browser process, the
// pending queue is completely drained and cleared; therefore, the second
// asynchronously registered listener receives zero queued events.
IN_PROC_BROWSER_TEST_F(ExtensionTestNotificationQueueBrowserTest,
                       ConsecutiveAsynchronousListeners) {
  ResultCatcher result_catcher;

  // Start the embedded test server and navigate to the web page.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL web_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), web_url));

  // Load and execute the extension before any event listeners are attached.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("queued_test_extension"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  // Attach the first listener and wait until the browser process dispatches the
  // pending queue to the first listener.
  ExtensionTestMessageListener test_started_listener_1(
      "listener1:myQueuedTest");
  constexpr char kFirstListenerJs[] = R"(
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`listener1:${info.testName}`);
    });
  )";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), kFirstListenerJs));
  ASSERT_TRUE(test_started_listener_1.WaitUntilSatisfied());

  // Now that the first listener has received the event, attach the second
  // listener and wait to confirm that the second listener does not receive an
  // event. We do this by waiting for an async message to be sent after the
  // second listener registers, and then explicitly flushing all pending
  // `EventRouter` IPC messages.
  ExtensionTestMessageListener test_started_listener_2(
      "listener2:myQueuedTest");
  ExtensionTestMessageListener async_check_done("async_check_done");
  constexpr char kSecondListenerJs[] = R"(
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`listener2:${info.testName}`);
    });
    chrome.test.sendMessage('async_check_done');
  )";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), kSecondListenerJs));
  ASSERT_TRUE(async_check_done.WaitUntilSatisfied());
  EventRouter::Get(profile())->FlushForTesting();
  EXPECT_FALSE(test_started_listener_2.was_satisfied());
}

// Verifies that when a test event listener for `chrome.test.onTestStarted` is
// registered inside the extension's own process before the test starts, the
// test start event is still enqueued because no listeners exist outside that
// source process. When an observing web page in another renderer process
// subsequently attaches a listener, it successfully receives the queued event.
IN_PROC_BROWSER_TEST_F(ExtensionTestNotificationQueueBrowserTest,
                       PreExistingSameProcessListenerQueuing) {
  ResultCatcher result_catcher;

  // Start the embedded test server.
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Set up message listeners for the extension page and same-process listener.
  ExtensionTestMessageListener page_ready("page_ready");
  ExtensionTestMessageListener same_process_listener(
      "same_process_listener:sameProcessTest");

  // Load and run an extension that contains an extension page (`page.html`)
  // and a background service worker. The extension page registers an
  // `onTestStarted` listener inside the extension process without invoking
  // `chrome.test.runTests()`.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"),
                     "<script src=\"page.js\"></script>");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), R"(
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`same_process_listener:${info.testName}`);
    });
    chrome.test.sendMessage('page_ready');
  )");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.test.sendMessage('bg_ready', () => {
      chrome.test.runTests([function sameProcessTest() {
        chrome.test.succeed();
      }]);
    });
  )");

  ExtensionTestMessageListener bg_ready("bg_ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(bg_ready.WaitUntilSatisfied());

  // Navigate to `page.html` in the active web contents so that the extension
  // page attaches an `onTestStarted` listener in the extension process.
  GURL ext_page_url = extension->GetResourceURL("page.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), ext_page_url));
  EXPECT_TRUE(page_ready.WaitUntilSatisfied());

  // Now trigger the extension tests in `background.js`.
  bg_ready.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  // Flush pending IPC events in `EventRouter` and verify that the same-process
  // listener in `page.html` did not receive the test start event via IPC.
  EventRouter::Get(profile())->FlushForTesting();
  EXPECT_FALSE(same_process_listener.was_satisfied());

  // Set up a listener for any messages sent by the observing web page's
  // `onTestStarted` listener.
  ExtensionTestMessageListener test_started_event(
      "Test started:sameProcessTest");

  // Navigate to a non-extension web page in a different renderer process.
  GURL web_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), web_url));

  // Register a test start listener on the observing web page after the
  // extension test has finished running.
  constexpr char kLateListenerJs[] = R"(
    chrome.test.onTestStarted.addListener((info) => {
      chrome.test.sendMessage(`Test started:${info.testName}`);
    });
  )";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), kLateListenerJs));

  // Verify that the observing web page receives the queued event, while the
  // same-process listener remains unsatisfied.
  EXPECT_TRUE(test_started_event.WaitUntilSatisfied());
  EXPECT_FALSE(same_process_listener.was_satisfied());
}

}  // namespace extensions
