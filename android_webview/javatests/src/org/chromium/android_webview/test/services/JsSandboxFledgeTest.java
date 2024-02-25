// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.content.Context;

import androidx.javascriptengine.EvaluationFailedException;
import androidx.javascriptengine.JavaScriptIsolate;
import androidx.javascriptengine.JavaScriptSandbox;
import androidx.javascriptengine.common.Utils;
import androidx.test.filters.MediumTest;

import com.google.common.util.concurrent.ListenableFuture;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.shell.R;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;

import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/** Instrumentation tests for JavaScriptSandbox. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class JsSandboxFledgeTest {
    private static final int TIMEOUT_SECONDS = 5;

    @Test
    @MediumTest
    public void testCanRunFunctionWithNoArgs() throws Throwable {
        final String code =
                ""
                        + "function helloWorld() {"
                        + "  return 'hello world';"
                        + "} \n"
                        + "helloWorld();";
        final String expected = "hello world";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            String result = resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testCanRunFunctionWithOneArg() throws Throwable {
        final String code =
                ""
                        + "function helloPerson(personName) {"
                        + "  return 'hello ' + personName;"
                        + "} \n"
                        + "const jena = 'Jena';"
                        + "helloPerson(jena);";
        final String expected = "hello Jena";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            String result = resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testCanRunFunctionWithJSONArguments() throws Throwable {
        final String code =
                ""
                        + "function helloPerson(person) {"
                        + "  return 'hello ' + person.name;"
                        + "}\n"
                        + "const jena = {\"name\" : \"Jena\"};"
                        + "helloPerson(jena);";
        final String expected = "hello Jena";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            String result = resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testCanRunFunctionWithJSONArrayArguments() throws Throwable {
        final String code =
                ""
                        + "function helloPerson(personArray) {"
                        + "  var str = 'hello';"
                        + "  for (var person of personArray) str += ' ' + person.name;"
                        + "  return str;"
                        + "} \n"
                        + "const nameArray = [{\"name\" : \"Harry\"}, {\"name\":\"Potter\"}];"
                        + "helloPerson(nameArray);";
        final String expected = "hello Harry Potter";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            String result = resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testScriptCanUseNamespace() throws Throwable {
        final String code =
                ""
                        + "var nameSpace = {"
                        + "  firstName: 'Harry',"
                        + "  lastName: 'Potter',"
                        + "  fullName: function() {"
                        + "    return this.firstName + ' ' + this.lastName;"
                        + " },"
                        + "};\n"
                        + "nameSpace.fullName();";

        final String expected = "Harry Potter";

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            String result = resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testScriptCanModifyExistingNamespace() throws Throwable {
        final String codeCreateNameSpace =
                ""
                        + "var nameSpace = {"
                        + "  firstName: 'Harry',"
                        + "  lastName: 'Potter',"
                        + "  fullName: function() {"
                        + "    return this.firstName + ' ' + this.lastName;"
                        + "  },"
                        + "};\n"
                        + "nameSpace.fullName();";

        final String codeModifyMember =
                "" + "nameSpace.firstName = 'James';" + "nameSpace.fullName();";

        final String codeAddFunction =
                ""
                        + "nameSpace.greeting = function() {"
                        + "  return 'Hello ' + this.fullName(); "
                        + "};\n"
                        + "nameSpace.greeting();";

        final String expected1 = "Harry Potter";
        final String expected2 = "James Potter";
        final String expected3 = "Hello James Potter";

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 =
                    jsIsolate.evaluateJavaScriptAsync(codeCreateNameSpace);
            String result1 = resultFuture1.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertEquals(expected1, result1);

            ListenableFuture<String> resultFuture2 =
                    jsIsolate.evaluateJavaScriptAsync(codeModifyMember);
            String result2 = resultFuture2.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertEquals(expected2, result2);

            ListenableFuture<String> resultFuture3 =
                    jsIsolate.evaluateJavaScriptAsync(codeAddFunction);
            String result3 = resultFuture3.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertEquals(expected3, result3);
        }
    }

    @Test
    @MediumTest
    public void testScriptCanRedefineNamespace() throws Throwable {
        final String codeCreateNameSpace =
                ""
                        + "var nameSpace = {"
                        + "  greeting: function() {"
                        + "    return 'Hello Harry';"
                        + "  },"
                        + "};\n"
                        + "nameSpace.greeting();";

        final String codeCheckNameSpace =
                "" + "var nameSpace = nameSpace || {};" + "nameSpace.greeting();";

        final String codeOverwriteNamespace =
                ""
                        + "var nameSpace = {"
                        + "  sayHello: 'Hello Potter',"
                        + "};\n"
                        + "nameSpace.greeting();";

        final String expected1 = "Hello Harry";
        final String expected2 = "Hello Harry";
        final String errorType = "TypeError";

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 =
                    jsIsolate.evaluateJavaScriptAsync(codeCreateNameSpace);
            String result1 = resultFuture1.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertEquals(expected1, result1);

            ListenableFuture<String> resultFuture2 =
                    jsIsolate.evaluateJavaScriptAsync(codeCheckNameSpace);
            String result2 = resultFuture2.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertEquals(expected2, result2);

            ListenableFuture<String> resultFuture3 =
                    jsIsolate.evaluateJavaScriptAsync(codeOverwriteNamespace);
            try {
                resultFuture3.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                Assert.fail("Should have thrown TypeError.");
            } catch (ExecutionException e) {
                Assert.assertTrue(e.getCause().getClass().equals(EvaluationFailedException.class));
                Assert.assertTrue(e.getCause().getMessage().contains(errorType));
            }
        }
    }

    @Test
    @MediumTest
    public void testCanNotReferToFunctionArguments() throws Throwable {
        final String code =
                ""
                        + "function helloPerson(person) {"
                        + "  return 'hello ' + personOuter.name;"
                        + "}\n"
                        + "(function() {"
                        + "  const personOuter = {\"name\": \"Jena\"};"
                        + "  return JSON.stringify(helloPerson(personOuter));"
                        + "})();";
        final String errorType = "ReferenceError";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);

        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            try {
                resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                Assert.fail("Should have thrown RefereneError.");
            } catch (ExecutionException e) {
                Assert.assertTrue(e.getCause().getClass().equals(EvaluationFailedException.class));
                Assert.assertTrue(e.getCause().getMessage().contains(errorType));
            }
        }
    }

    @Test
    @MediumTest
    public void testUndefinedFunctionThrowsErrors() throws Throwable {
        final String code = "undefinedFunction();";
        final String errorType = "ReferenceError";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);

        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            try {
                resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                Assert.fail("Should have thrown ReferenceError.");
            } catch (ExecutionException e) {
                Assert.assertTrue(e.getCause().getClass().equals(EvaluationFailedException.class));
                Assert.assertTrue(e.getCause().getMessage().contains(errorType));
            }
        }
    }

    @Test
    @MediumTest
    public void testParallelCallsToIsolatesDoNotInterfere() throws Throwable {
        final ExecutorService executorService = Executors.newFixedThreadPool(10);
        final String code1 =
                ""
                        + "function helloPerson(person) {"
                        + "  return 'hello ' + person.name;"
                        + "}\n"
                        + "const jena = {\"name\": \"Jena\"};"
                        + "helloPerson(jena);";
        final String expected1 = "hello Jena";

        final String code2 =
                ""
                        + "function helloPerson(person) {"
                        + "  return 'hello again ' + person.name;"
                        + "}\n"
                        + "const jena = {\"name\": \"Jena\"};"
                        + "helloPerson(jena);";
        final String expected2 = "hello again Jena";

        CountDownLatch resultsLatch = new CountDownLatch(2);
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate1 = jsSandbox.createIsolate();
                JavaScriptIsolate jsIsolate2 = jsSandbox.createIsolate(); ) {
            ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavaScriptAsync(code1);
            resultFuture1.addListener(resultsLatch::countDown, executorService);

            ListenableFuture<String> resultFuture2 = jsIsolate2.evaluateJavaScriptAsync(code2);
            resultFuture2.addListener(resultsLatch::countDown, executorService);

            resultsLatch.await();

            Assert.assertEquals(expected1, resultFuture1.get());
            Assert.assertEquals(expected2, resultFuture2.get());
        }
        executorService.shutdown();
    }

    @Test
    @MediumTest
    public void testSimpleAdScriptReturnsJSON() throws Throwable {
        /*
         * This test uses the ad-selection code corresponding to
         * AdSelectionScriptEngineTest#testCanCallScript
         */
        final String codeAdTechLogic =
                ""
                        + "function helloAdvert(ad) {"
                        + "  return {\"status\": 0, \"greeting\": \"hello \" + ad.render_uri};"
                        + "}\n";

        final String codeProcessAds =
                ""
                        + "function compute(ads) {"
                        + "  let status = 0;"
                        + "  const results = [];"
                        + "  for (const ad of ads) {"
                        + "    const script_result = helloAdvert(ad);"
                        + "    if (script_result === Object(script_result) &&"
                        + "         \"status\" in script_result) {"
                        + "      status = script_result.status;"
                        + "    } else {"
                        + "      status = -1;"
                        + "    }"
                        + "    if (status != 0) break;"
                        + "    results.push(script_result);"
                        + "  }"
                        + "  return {\"status\": status, \"results\": results};"
                        + "};\n";

        final String codeCallAdScript =
                ""
                        + "const ads = ["
                        + "  {"
                        + "    \"render_uri\": \"https://www.example.com/adverts/123\","
                        + "    \"metadata\": {\"result\":1.1}"
                        + "  }"
                        + "];"
                        + "JSON.stringify(compute(ads));";

        final String expectedResponse = "hello https://www.example.com/adverts/123";
        final int expectedStatus = 0;

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 =
                    jsIsolate.evaluateJavaScriptAsync(codeAdTechLogic);
            Assert.assertTrue(resultFuture1.get(TIMEOUT_SECONDS, TimeUnit.SECONDS).isEmpty());

            ListenableFuture<String> resultFuture2 =
                    jsIsolate.evaluateJavaScriptAsync(codeProcessAds);
            Assert.assertTrue(resultFuture2.get(TIMEOUT_SECONDS, TimeUnit.SECONDS).isEmpty());

            ListenableFuture<String> resultFuture3 =
                    jsIsolate.evaluateJavaScriptAsync(codeCallAdScript);
            JSONObject result =
                    new JSONObject(resultFuture3.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));

            Assert.assertEquals(expectedStatus, result.get("status"));
            Assert.assertEquals(
                    expectedResponse,
                    ((JSONObject) result.getJSONArray("results").get(0)).get("greeting"));
        }
    }

    @Test
    @MediumTest
    public void testInvalidAdScriptReturnsNegativeStatus() throws Throwable {
        /*
         * This test uses the ad-selection code corresponding to
         * AdSelectionScriptEngineTest#testFailsIfScriptIsNotReturningJson
         */
        final String codeAdTechLogic =
                "" + "function helloAdvert(ad) {" + "  return 'hello ' + ad.render_uri;" + "}\n";

        final String codeProcessAds =
                ""
                        + "function compute(ads) {"
                        + "  let status = 0;"
                        + "  const results = [];"
                        + "  for (const ad of ads) {"
                        + "    const script_result = helloAdvert(ad);"
                        + "    if (script_result === Object(script_result) &&"
                        + "         \"status\" in script_result) {"
                        + "      status = script_result.status;"
                        + "    } else {"
                        + "      status = -1;"
                        + "    }"
                        + "    if (status != 0) break;"
                        + "    results.push(script_result);"
                        + "  }"
                        + "  return {\"status\": status, \"results\": results};"
                        + "};\n";

        final String codeCallAdScript =
                ""
                        + "const ads = ["
                        + "  {"
                        + "    \"render_uri\": \"https://www.example.com/adverts/123\","
                        + "    \"metadata\": {\"result\":1.1}"
                        + "  }"
                        + "];"
                        + "JSON.stringify(compute(ads));";

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 =
                    jsIsolate.evaluateJavaScriptAsync(codeAdTechLogic);
            Assert.assertTrue(resultFuture1.get(TIMEOUT_SECONDS, TimeUnit.SECONDS).isEmpty());

            ListenableFuture<String> resultFuture2 =
                    jsIsolate.evaluateJavaScriptAsync(codeProcessAds);
            Assert.assertTrue(resultFuture2.get(TIMEOUT_SECONDS, TimeUnit.SECONDS).isEmpty());

            ListenableFuture<String> resultFuture3 =
                    jsIsolate.evaluateJavaScriptAsync(codeCallAdScript);
            JSONObject result =
                    new JSONObject(resultFuture3.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));

            Assert.assertEquals(-1, result.get("status"));
        }
    }

    @Test
    @MediumTest
    public void testAdBiddingLogicReturnsSelectedOutcome() throws Throwable {
        /*
         * This test uses the ad-selection code corresponding to
         * AdSelectionScriptEngineTest#testSelectOutcomeOpenBiddingMediationLogicJsSuccess
         */
        final String codeBiddingLogic =
                ""
                        + "function selectOutcome(outcomes, selection_signals) {"
                        + "  let max_bid = 0;"
                        + "  let winner_outcome = null;"
                        + "  for (let outcome of outcomes) {"
                        + "      if (outcome.bid > max_bid) {"
                        + "          max_bid = outcome.bid;"
                        + "          winner_outcome = outcome;"
                        + "      }"
                        + "  }"
                        + "  return {\"status\": 0, \"result\": winner_outcome}; "
                        + "} \n";

        final String codeProcessAds =
                ""
                        + "function compute(ads, selection_signals) {"
                        + "  let status = 0;"
                        + "  const results = [];"
                        + "  const script_result = selectOutcome(ads,selection_signals);"
                        + "  if (script_result === Object(script_result) &&"
                        + "      \"status\" in script_result &&"
                        + "      \"result\" in script_result) {"
                        + "    status = script_result.status;"
                        + "    results.push(script_result.result)"
                        + "  } else {"
                        + "    status = -1;"
                        + "  }"
                        + "  return { \"status\": status, \"results\": results};"
                        + "} \n";

        final String codeCallAdScript =
                ""
                        + "const ads = ["
                        + "  {\"id\": \"12345\", \"bid\": 10.0},"
                        + "  {\"id\": \"123456\", \"bid\": 11.0},"
                        + "  {\"id\": \"1234567\", \"bid\": 12.0}"
                        + "];"
                        + "const selection_signals = {};"
                        + "JSON.stringify(compute(ads,selection_signals));";

        final String expected = "1234567";

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 =
                    jsIsolate.evaluateJavaScriptAsync(codeBiddingLogic);
            Assert.assertTrue(resultFuture1.get(TIMEOUT_SECONDS, TimeUnit.SECONDS).isEmpty());

            ListenableFuture<String> resultFuture2 =
                    jsIsolate.evaluateJavaScriptAsync(codeProcessAds);
            Assert.assertTrue(resultFuture2.get(TIMEOUT_SECONDS, TimeUnit.SECONDS).isEmpty());

            ListenableFuture<String> resultFuture3 =
                    jsIsolate.evaluateJavaScriptAsync(codeCallAdScript);
            JSONObject result =
                    new JSONObject(resultFuture3.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));

            Assert.assertEquals(0, result.get("status"));
            Assert.assertEquals(
                    expected, ((JSONObject) result.getJSONArray("results").get(0)).get("id"));
        }
    }

    @Test
    @MediumTest
    public void testCanUseWasmModuleInScript() throws Throwable {
        final Context context = ContextUtils.getApplicationContext();

        // add_two_numbers contains a function that adds two numbers, equivalent to:
        //   function addTwo(input1, input2) { return input1 + input2; }
        byte[] wasmModule;
        try (InputStream inputStream =
                context.getResources().openRawResource(R.raw.add_two_numbers)) {
            wasmModule = new byte[inputStream.available()];
            if (Utils.readNBytes(inputStream, wasmModule, 0, wasmModule.length)
                    != wasmModule.length) {
                throw new IOException("Couldn't read all bytes from the WASM module");
            }
        }

        final String codeRunWasmModule =
                "'use strict';function callWasm(input1, input2, wasmModule) {  const instance = new"
                        + " WebAssembly.Instance(wasmModule);  const { addTwo } = instance.exports; "
                        + " return addTwo(input1,input2);}\n"
                        + "(function() {  const input1 = 3;  const input2 = 4;  return"
                        + " android.consumeNamedDataAsArrayBuffer('module').then((value) => {    return"
                        + " WebAssembly.compile(value).then((wasmModule) => {      return"
                        + " JSON.stringify(callWasm(input1, input2, wasmModule));    })  });})();";
        final String expected = "7";

        final ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(
                            JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_WASM_COMPILATION));

            final boolean provideNamedDataReturn = jsIsolate.provideNamedData("module", wasmModule);
            Assert.assertTrue(provideNamedDataReturn);
            final ListenableFuture<String> resultFuture =
                    jsIsolate.evaluateJavaScriptAsync(codeRunWasmModule);
            final String result = resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }
}
