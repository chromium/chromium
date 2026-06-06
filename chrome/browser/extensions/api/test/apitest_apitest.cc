// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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
  std::string SetUseStandardizedApiBehaviorForTesting(
      bool standardized_behavior_enabled) const {
    return base::StringPrintf(
        "chrome.test.setUseStandardizedApiBehaviorForTesting(%s);",
        standardized_behavior_enabled ? "true" : "false");
  }
};

// Tests the differences between assertEq. The standardized version uses
// Object.is() more extensively.
IN_PROC_BROWSER_TEST_P(TestStandardizedAPITest, assertEq) {
  bool standardized_behavior_enabled = GetParam();
  std::string set_api_behavior =
      SetUseStandardizedApiBehaviorForTesting(standardized_behavior_enabled);

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
    std::string script = base::StringPrintf(
        R"(%s
           chrome.test.runTests([
             function test() {
               %s
               chrome.test.succeed();
             }
           ]);)",
        set_api_behavior.c_str(), c.test_case.c_str());

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
           let onTestStartedFired = false;
           chrome.test.onTestStarted.addListener((info) => {
               chrome.test.assertEq('%s', info.testName);
               onTestStartedFired = true;
           });

           const finishedListener = (info) => {
             if (info.result === false) {
               // The test already failed, don't call chrome.test.fail() again
               // to avoid infinite recursion.
               return;
             }
             if (onTestStartedFired &&
                 info.remainingTests === 0 &&
                 info.assertionDescription === 'Test succeeded') {
               // Send message indicating we successfully received the finished
               // event for this test.
               chrome.test.sendMessage('finished:' + info.testName);
             } else {
               chrome.test.fail('Unexpected info: ' + JSON.stringify(info));
             }
           };
           chrome.test.onTestFinished.addListener(finishedListener);

           // Run the test. The test passing means that the `onTestStarted`
           // event fired. `onTestFinished` is confirmed when it runs and sends
           // a message back to the test C++.
           chrome.test.runTests([
             function %s() {
               chrome.test.assertTrue(onTestStartedFired);
               chrome.test.succeed();
             }
           ]);)",
        test_name.c_str(), test_name.c_str());
  }
};

// Tests that `chrome.test.onTestStarted` and `chrome.test.onTestFinished` fired
// when `chrome.test.runTests` is called in various contexts (background
// scripts, extension pages, content scripts, and web pages).
IN_PROC_BROWSER_TEST_F(TestHarnessEventsBrowserTest, AllContexts) {
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

}  // namespace extensions
