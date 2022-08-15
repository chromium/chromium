// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_js_sandbox_test.test;

import android.content.Context;

import androidx.test.filters.MediumTest;

import com.google.common.util.concurrent.ListenableFuture;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.js_sandbox.client.EvaluationFailedException;
import org.chromium.android_webview.js_sandbox.client.IsolateSettings;
import org.chromium.android_webview.js_sandbox.client.IsolateTerminatedException;
import org.chromium.android_webview.js_sandbox.client.JsIsolate;
import org.chromium.android_webview.js_sandbox.client.JsSandbox;
import org.chromium.android_webview.js_sandbox.client.SandboxDeadException;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;

import java.nio.charset.StandardCharsets;
import java.util.Vector;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/** Instrumentation test for JsSandboxService. */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewJsSandboxTest {
    // This value is somewhat arbitrary. It might need bumping if V8 snapshots become significantly
    // larger in future. However, we don't want it too large as that will make the tests slower and
    // require more memory.
    private static final long REASONABLE_HEAP_SIZE = 100 * 1024 * 1024;

    @Test
    @MediumTest
    public void testSimpleJsEvaluation() throws Throwable {
        final String code = "\"PASS\"";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
        String result = resultFuture.get(5, TimeUnit.SECONDS);
        jsIsolate.close();
        jsSandbox.close();

        Assert.assertEquals(expected, result);
    }

    @Test
    @MediumTest
    public void testClosingOneIsolate() throws Throwable {
        final String code = "'PASS'";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate1 = jsSandbox.createIsolate();
        JsIsolate jsIsolate2 = jsSandbox.createIsolate();
        jsIsolate1.close();
        ListenableFuture<String> resultFuture = jsIsolate2.evaluateJavascriptAsync(code);
        String result = resultFuture.get(5, TimeUnit.SECONDS);
        jsIsolate2.close();
        jsSandbox.close();

        Assert.assertEquals(expected, result);
    }

    @Test
    @MediumTest
    public void testEvaluationInTwoIsolates() throws Throwable {
        final String code1 = "this.x = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.x = 'SUPER_PASS';\n";
        final String expected2 = "SUPER_PASS";

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate1 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavascriptAsync(code1);
        String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate2 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture2 = jsIsolate2.evaluateJavascriptAsync(code2);
        String result2 = resultFuture2.get(5, TimeUnit.SECONDS);
        jsIsolate1.close();
        jsIsolate2.close();
        jsSandbox.close();

        Assert.assertEquals(expected1, result1);
        Assert.assertEquals(expected2, result2);
    }

    @Test
    @MediumTest
    public void testTwoIsolatesDoNotShareEnvironment() throws Throwable {
        final String code1 = "this.y = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.y = this.y + ' PASS';\n";
        final String expected2 = "undefined PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate1 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavascriptAsync(code1);
        String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate2 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture2 = jsIsolate2.evaluateJavascriptAsync(code2);
        String result2 = resultFuture2.get(5, TimeUnit.SECONDS);
        jsIsolate1.close();
        jsIsolate2.close();
        jsSandbox.close();

        Assert.assertEquals(expected1, result1);
        Assert.assertEquals(expected2, result2);
    }

    @Test
    @MediumTest
    public void testTwoExecutionsShareEnvironment() throws Throwable {
        final String code1 = "this.z = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.z = this.z + ' PASS';\n";
        final String expected2 = "PASS PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate1 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavascriptAsync(code1);
        String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
        ListenableFuture<String> resultFuture2 = jsIsolate1.evaluateJavascriptAsync(code2);
        String result2 = resultFuture2.get(5, TimeUnit.SECONDS);
        jsIsolate1.close();
        jsSandbox.close();

        Assert.assertEquals(expected1, result1);
        Assert.assertEquals(expected2, result2);
    }

    @Test
    @MediumTest
    public void testJsEvaluationError() throws Throwable {
        final String code = "throw new WebAssembly.LinkError('RandomLinkError');";
        final String contains = "RandomLinkError";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
        boolean isOfCorrectType = false;
        String error = "";
        try {
            String result = resultFuture.get(5, TimeUnit.SECONDS);
        } catch (ExecutionException e) {
            isOfCorrectType = e.getCause().getClass().equals(EvaluationFailedException.class);
            error = e.getCause().getMessage();
        }
        jsIsolate.close();
        jsSandbox.close();

        Assert.assertTrue(isOfCorrectType);
        Assert.assertTrue(error.contains(contains));
    }

    @Test
    @MediumTest
    public void testInfiniteLoop() throws Throwable {
        final String code = "while(true){}";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.ISOLATE_TERMINATION));

        JsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
        boolean isOfCorrectType = false;
        try {
            jsIsolate.close();
            String result = resultFuture.get(5, TimeUnit.SECONDS);
        } catch (ExecutionException e) {
            isOfCorrectType = e.getCause().getClass().equals(IsolateTerminatedException.class);
        }
        jsSandbox.close();

        Assert.assertTrue(isOfCorrectType);
    }

    @Test
    @MediumTest
    public void testMultipleInfiniteLoops() throws Throwable {
        final String code = "while(true){}";
        final int num_of_evaluations = 10;
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.ISOLATE_TERMINATION));

        JsIsolate jsIsolate = jsSandbox.createIsolate();
        Vector<ListenableFuture<String>> resultFutures = new Vector<ListenableFuture<String>>();
        for (int i = 0; i < num_of_evaluations; i++) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
            resultFutures.add(resultFuture);
        }
        jsIsolate.close();

        for (int i = 0; i < num_of_evaluations; i++) {
            boolean isOfCorrectType = false;
            try {
                String result = resultFutures.elementAt(i).get(5, TimeUnit.SECONDS);
            } catch (ExecutionException e) {
                isOfCorrectType = e.getCause().getClass().equals(IsolateTerminatedException.class);
            }
            Assert.assertTrue(isOfCorrectType);
        }
        jsSandbox.close();
    }

    @Test
    @MediumTest
    @DisabledTest(
            message =
                    "Enable it back once we have a WebView version to see if the feature is actually supported in that version")
    public void
    testFeatureDetection() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JsSandbox> JsSandboxFuture =
                JsSandbox.newConnectedInstanceAsync(ContextUtils.getApplicationContext());
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assert.assertFalse(jsSandbox.isFeatureSupported(JsSandbox.ISOLATE_TERMINATION));
        }
    }

    @Test
    @MediumTest
    public void testSimpleArrayBuffer() throws Throwable {
        final String provideString = "Hello World";
        final byte[] bytes = provideString.getBytes(StandardCharsets.US_ASCII);
        final String code = ""
                + "function ab2str(buf) {"
                + " return String.fromCharCode.apply(null, new Uint8Array(buf));"
                + "}"
                + "android.consumeNamedDataAsArrayBuffer(\"id-1\").then((value) => {"
                + " return ab2str(value);"
                + "});";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
                JsIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROVIDE_CONSUME_ARRAY_BUFFER));

            boolean provideNamedDataReturn = jsIsolate.provideNamedData("id-1", bytes);
            Assert.assertTrue(provideNamedDataReturn);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascriptAsync(code);
            String result = resultFuture1.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(provideString, result);
        }
    }

    @Test
    @MediumTest
    public void testArrayBufferWasmCompilation() throws Throwable {
        final String success = "success";
        // The bytes of a minimal WebAssembly module, courtesy of v8/test/cctest/test-api-wasm.cc
        final byte[] bytes = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
        final String code = ""
                + "android.consumeNamedDataAsArrayBuffer(\"id-1\").then((value) => {"
                + " return WebAssembly.compile(value).then((module) => {"
                + "  return \"success\";"
                + "  });"
                + "});";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
                JsIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROVIDE_CONSUME_ARRAY_BUFFER));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.WASM_COMPILATION));

            boolean provideNamedDataReturn = jsIsolate.provideNamedData("id-1", bytes);
            Assert.assertTrue(provideNamedDataReturn);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascriptAsync(code);
            String result = resultFuture1.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(success, result);
        }
    }

    @Test
    @MediumTest
    public void testPromiseReturn() throws Throwable {
        final String code = "Promise.resolve(\"PASS\")";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
                JsIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROMISE_RETURN));

            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
            String result = resultFuture.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testPromiseReturnLaterResolve() throws Throwable {
        final String code1 = "var promiseResolve, promiseReject;"
                + "new Promise(function(resolve, reject){"
                + "  promiseResolve = resolve;"
                + "  promiseReject = reject;"
                + "});";
        final String code2 = "promiseResolve(\"PASS\");";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
                JsIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROMISE_RETURN));

            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascriptAsync(code1);
            ListenableFuture<String> resultFuture2 = jsIsolate.evaluateJavascriptAsync(code2);
            String result = resultFuture1.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testNestedConsumeNamedDataAsArrayBuffer() throws Throwable {
        final String success = "success";
        // The bytes of a minimal WebAssembly module, courtesy of v8/test/cctest/test-api-wasm.cc
        final byte[] bytes = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
        final String code = ""
                + "android.consumeNamedDataAsArrayBuffer(\"id-1\").then((value) => {"
                + " return android.consumeNamedDataAsArrayBuffer(\"id-2\").then((value) => {"
                + "  return android.consumeNamedDataAsArrayBuffer(\"id-3\").then((value) => {"
                + "   return android.consumeNamedDataAsArrayBuffer(\"id-4\").then((value) => {"
                + "    return android.consumeNamedDataAsArrayBuffer(\"id-5\").then((value) => {"
                + "     return \"success\";"
                + "     }, (error) => {"
                + "     return error.message;"
                + "    });"
                + "   });"
                + "  });"
                + " });"
                + "});";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
                JsIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROVIDE_CONSUME_ARRAY_BUFFER));

            jsIsolate.provideNamedData("id-1", bytes);
            jsIsolate.provideNamedData("id-2", bytes);
            jsIsolate.provideNamedData("id-3", bytes);
            jsIsolate.provideNamedData("id-4", bytes);
            jsIsolate.provideNamedData("id-5", bytes);
            Thread.sleep(1000);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascriptAsync(code);
            String result = resultFuture1.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(success, result);
        }
    }

    @Test
    @MediumTest
    public void testPromiseEvaluationThrow() throws Throwable {
        final String provideString = "Hello World";
        final byte[] bytes = provideString.getBytes(StandardCharsets.US_ASCII);
        final String code = ""
                + "android.consumeNamedDataAsArrayBuffer(\"id-1\").catch((error) => {"
                + " throw new WebAssembly.LinkError('RandomLinkError');"
                + "});";
        final String contains = "RandomLinkError";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
                JsIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.PROVIDE_CONSUME_ARRAY_BUFFER));

            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
            try {
                String result = resultFuture.get(5, TimeUnit.SECONDS);
                Assert.fail("Should have thrown.");
            } catch (ExecutionException e) {
                if (!(e.getCause() instanceof EvaluationFailedException)) {
                    throw e;
                }
                Assert.assertTrue(e.getCause().getMessage().contains(contains));
            }
        }
    }

    @Test
    @MediumTest
    public void testEvaluationThrowsWhenSandboxDead() throws Throwable {
        final String code = "while(true){}";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS);
        JsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
        try {
            jsSandbox.close();
            resultFuture.get(5, TimeUnit.SECONDS);
            Assert.fail("Should have thrown.");
        } catch (ExecutionException e) {
            if (!(e.getCause() instanceof IsolateTerminatedException)) {
                throw e;
            }
        }
    }

    @Test
    @MediumTest
    public void testMultipleSandboxesCannotCoexist() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        final String contains = "already bound";
        ListenableFuture<JsSandbox> JsSandboxFuture1 = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox1 = JsSandboxFuture1.get(5, TimeUnit.SECONDS)) {
            ListenableFuture<JsSandbox> JsSandboxFuture2 =
                    JsSandbox.newConnectedInstanceAsync(context);
            try {
                JsSandbox jsSandbox2 = JsSandboxFuture2.get(5, TimeUnit.SECONDS);
                Assert.fail("Should have thrown.");
            } catch (ExecutionException e) {
                if (!(e.getCause() instanceof RuntimeException)) {
                    throw e;
                }
                Assert.assertTrue(e.getCause().getMessage().contains(contains));
            }
        }
    }

    @Test
    @MediumTest
    public void testSandboxCanBeCreatedAfterClosed() throws Throwable {
        final String code = "\"PASS\"";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JsSandbox> JsSandboxFuture1 = JsSandbox.newConnectedInstanceAsync(context);
        JsSandbox jsSandbox1 = JsSandboxFuture1.get(5, TimeUnit.SECONDS);
        jsSandbox1.close();
        ListenableFuture<JsSandbox> JsSandboxFuture2 = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox2 = JsSandboxFuture2.get(5, TimeUnit.SECONDS);
                JsIsolate jsIsolate = jsSandbox2.createIsolate()) {
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascriptAsync(code);
            String result = resultFuture1.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testHeapSizeAdjustment() throws Throwable {
        final String code = "\"PASS\"";
        final String expected = "PASS";
        final long[] heapSizes = {
                0,
                REASONABLE_HEAP_SIZE,
                REASONABLE_HEAP_SIZE - 1,
                REASONABLE_HEAP_SIZE + 1,
                REASONABLE_HEAP_SIZE + 4095,
                REASONABLE_HEAP_SIZE + 4096,
                REASONABLE_HEAP_SIZE + 65535,
                REASONABLE_HEAP_SIZE + 65536,
                1L << 50,
        };
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.ISOLATE_MAX_HEAP_SIZE));
            for (long heapSize : heapSizes) {
                IsolateSettings isolateStartupParameters = new IsolateSettings();
                isolateStartupParameters.setMaxHeapSizeBytes(heapSize);
                try (JsIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                    ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
                    String result = resultFuture.get(5, TimeUnit.SECONDS);
                    Assert.assertEquals(expected, result);
                } catch (Throwable e) {
                    throw new AssertionError(
                            "Failed to evaluate JavaScript using max heap size setting " + heapSize,
                            e);
                }
            }
        }
    }

    @Test
    @MediumTest
    public void testHeapSizeEnforced() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        // We need to beat the v8 optimizer to ensure it really allocates the required memory.
        // Note that we're allocating an array of elements - not bytes.
        final String code = "this.array = Array(" + maxHeapSize + ").fill(Math.random(), 0);"
                + "var arrayLength = this.array.length;"
                + "var sum = 0;"
                + "for (var i = 0; i < arrayLength; i++) {"
                + " sum+=this.array[i];"
                + "}";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JsSandbox> JsSandboxFuture = JsSandbox.newConnectedInstanceAsync(context);
        try (JsSandbox jsSandbox = JsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(JsSandbox.ISOLATE_MAX_HEAP_SIZE));
            IsolateSettings isolateStartupParameters = new IsolateSettings();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            try (JsIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascriptAsync(code);
                try {
                    resultFuture.get(10, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof SandboxDeadException)) {
                        throw e;
                    }
                }
            }
        }
    }
}
