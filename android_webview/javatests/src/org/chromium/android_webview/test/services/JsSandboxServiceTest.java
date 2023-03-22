// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.javascriptengine.EvaluationFailedException;
import androidx.javascriptengine.EvaluationResultSizeLimitExceededException;
import androidx.javascriptengine.IsolateStartupParameters;
import androidx.javascriptengine.IsolateTerminatedException;
import androidx.javascriptengine.JavaScriptIsolate;
import androidx.javascriptengine.JavaScriptSandbox;
import androidx.javascriptengine.MemoryLimitExceededException;
import androidx.javascriptengine.SandboxDeadException;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.nio.charset.StandardCharsets;
import java.util.Vector;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * Instrumentation test for JavaScriptSandbox.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
@RequiresApi(Build.VERSION_CODES.O)
public class JsSandboxServiceTest {
    // This value is somewhat arbitrary. It might need bumping if V8 snapshots become significantly
    // larger in future. However, we don't want it too large as that will make the tests slower and
    // require more memory.
    private static final long REASONABLE_HEAP_SIZE = 100 * 1024 * 1024;
    private static final int LARGE_NAMED_DATA_SIZE = 2 * 1024 * 1024;

    @Test
    @MediumTest
    public void testSimpleJsEvaluation() throws Throwable {
        final String code = "\"PASS\"";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            String result = resultFuture.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testClosingOneIsolate() throws Throwable {
        final String code = "'PASS'";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate1 = jsSandbox.createIsolate()) {
            JavaScriptIsolate jsIsolate2 = jsSandbox.createIsolate();
            jsIsolate2.close();

            ListenableFuture<String> resultFuture = jsIsolate1.evaluateJavaScriptAsync(code);
            String result = resultFuture.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected, result);
        }
    }

    @Test
    @MediumTest
    public void testEvaluationInTwoIsolates() throws Throwable {
        final String code1 = "this.x = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.x = 'SUPER_PASS';\n";
        final String expected2 = "SUPER_PASS";

        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate1 = jsSandbox.createIsolate();
             JavaScriptIsolate jsIsolate2 = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavaScriptAsync(code1);
            String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
            ListenableFuture<String> resultFuture2 = jsIsolate2.evaluateJavaScriptAsync(code2);
            String result2 = resultFuture2.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected1, result1);
            Assert.assertEquals(expected2, result2);
        }
    }

    @Test
    @MediumTest
    public void testTwoIsolatesDoNotShareEnvironment() throws Throwable {
        final String code1 = "this.y = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.y = this.y + ' PASS';\n";
        final String expected2 = "undefined PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate1 = jsSandbox.createIsolate();
             JavaScriptIsolate jsIsolate2 = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavaScriptAsync(code1);
            String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
            ListenableFuture<String> resultFuture2 = jsIsolate2.evaluateJavaScriptAsync(code2);
            String result2 = resultFuture2.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected1, result1);
            Assert.assertEquals(expected2, result2);
        }
    }

    @Test
    @MediumTest
    public void testTwoExecutionsShareEnvironment() throws Throwable {
        final String code1 = "this.z = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.z = this.z + ' PASS';\n";
        final String expected2 = "PASS PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate1 = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavaScriptAsync(code1);
            String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
            ListenableFuture<String> resultFuture2 = jsIsolate1.evaluateJavaScriptAsync(code2);
            String result2 = resultFuture2.get(5, TimeUnit.SECONDS);

            Assert.assertEquals(expected1, result1);
            Assert.assertEquals(expected2, result2);
        }
    }

    @Test
    @MediumTest
    public void testJsEvaluationError() throws Throwable {
        final String code = "throw new WebAssembly.LinkError('RandomLinkError');";
        final String contains = "RandomLinkError";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            boolean isOfCorrectType = false;
            String error = "";
            try {
                resultFuture.get(5, TimeUnit.SECONDS);
                Assert.fail("Should have thrown.");
            } catch (ExecutionException e) {
                isOfCorrectType = e.getCause().getClass().equals(EvaluationFailedException.class);
                error = e.getCause().getMessage();
            }

            Assert.assertTrue(isOfCorrectType);
            Assert.assertTrue(error.contains(contains));
        }
    }

    @Test
    @MediumTest
    public void testInfiniteLoop() throws Throwable {
        final String code = "while(true){}";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_ISOLATE_TERMINATION));

            ListenableFuture<String> resultFuture;
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            }

            try {
                resultFuture.get(5, TimeUnit.SECONDS);
                Assert.fail("Should have thrown.");
            } catch (ExecutionException e) {
                if (!(e.getCause() instanceof IsolateTerminatedException)) {
                    throw e;
                }
            }
        }
    }

    @Test
    @MediumTest
    public void testMultipleInfiniteLoops() throws Throwable {
        final String code = "while(true){}";
        final int num_of_evaluations = 10;
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_ISOLATE_TERMINATION));

            Vector<ListenableFuture<String>> resultFutures = new Vector<ListenableFuture<String>>();
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                for (int i = 0; i < num_of_evaluations; i++) {
                    ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                    resultFutures.add(resultFuture);
                }
            }

            for (int i = 0; i < num_of_evaluations; i++) {
                try {
                    resultFutures.elementAt(i).get(5, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof IsolateTerminatedException)) {
                        throw e;
                    }
                }
            }
        }
    }

    @Test
    @MediumTest
    @DisabledTest(
            message =
                    "Enable it back once we have a WebView version to see if the feature is actually supported in that version")
    public void
    testFeatureDetection() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(
                        ContextUtils.getApplicationContext());
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assert.assertFalse(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_ISOLATE_TERMINATION));
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
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));

            boolean provideNamedDataReturn = jsIsolate.provideNamedData("id-1", bytes);
            Assert.assertTrue(provideNamedDataReturn);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavaScriptAsync(code);
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
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_WASM_COMPILATION));

            boolean provideNamedDataReturn = jsIsolate.provideNamedData("id-1", bytes);
            Assert.assertTrue(provideNamedDataReturn);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavaScriptAsync(code);
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
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));

            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
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

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));

            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavaScriptAsync(code1);
            ListenableFuture<String> resultFuture2 = jsIsolate.evaluateJavaScriptAsync(code2);
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
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));

            jsIsolate.provideNamedData("id-1", bytes);
            jsIsolate.provideNamedData("id-2", bytes);
            jsIsolate.provideNamedData("id-3", bytes);
            jsIsolate.provideNamedData("id-4", bytes);
            jsIsolate.provideNamedData("id-5", bytes);
            Thread.sleep(1000);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavaScriptAsync(code);
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

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));

            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
            try {
                resultFuture.get(5, TimeUnit.SECONDS);
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
    public void testEvaluationThrowsWhenSandboxClosed() throws Throwable {
        final String code = "while(true){}";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavaScriptAsync(code);
            jsSandbox.close();
            // Check already running evaluation gets SandboxDeadException
            try {
                resultFuture1.get(5, TimeUnit.SECONDS);
                Assert.fail("Should have thrown.");
            } catch (ExecutionException e) {
                if (!(e.getCause() instanceof SandboxDeadException)) {
                    throw e;
                }
            }
            // Check post-close evaluation gets SandboxDeadException
            ListenableFuture<String> resultFuture2 = jsIsolate.evaluateJavaScriptAsync(code);
            try {
                resultFuture2.get(5, TimeUnit.SECONDS);
                Assert.fail("Should have thrown.");
            } catch (ExecutionException e) {
                if (!(e.getCause() instanceof SandboxDeadException)) {
                    throw e;
                }
            }
            // Check that closing an isolate then causes the IllegalStateException to be
            // thrown instead.
            jsIsolate.close();
            try {
                ListenableFuture<String> postCloseResultFuture =
                        jsIsolate.evaluateJavaScriptAsync(code);
                Assert.fail("Should have thrown.");
            } catch (IllegalStateException e) {
                // Expected
            }
        }
    }

    @Test
    @MediumTest
    public void testMultipleSandboxesCannotCoexist() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        final String contains = "already bound";
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture1 =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox1 = jsSandboxFuture1.get(5, TimeUnit.SECONDS)) {
            ListenableFuture<JavaScriptSandbox> jsSandboxFuture2 =
                    JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
            try {
                try (JavaScriptSandbox jsSandbox2 = jsSandboxFuture2.get(5, TimeUnit.SECONDS)) {
                    Assert.fail("Should have thrown.");
                }
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
        final int num_of_startups = 2;
        Context context = ContextUtils.getApplicationContext();

        for (int i = 0; i < num_of_startups; i++) {
            ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                    JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
            try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS);
                 JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                String result = resultFuture.get(5, TimeUnit.SECONDS);

                Assert.assertEquals(expected, result);
            }
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
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            for (long heapSize : heapSizes) {
                IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
                isolateStartupParameters.setMaxHeapSizeBytes(heapSize);
                try (JavaScriptIsolate jsIsolate =
                             jsSandbox.createIsolate(isolateStartupParameters)) {
                    ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
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
    @LargeTest
    public void testHeapSizeEnforced() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        // We need to beat the v8 optimizer to ensure it really allocates the required memory. Note
        // that we're allocating an array of elements - not bytes. Filling will ensure that the
        // array is not sparsely allocated.
        final String oomingCode = ""
                + "const array = Array(" + maxHeapSize + ").fill(Math.random(), 0);";
        final String stableCode = "'PASS'";
        final String stableExpected = "PASS";
        final String unresolvedCode = "new Promise((resolve, reject) => {/* never resolve */})";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture1 =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture1.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            try (JavaScriptIsolate jsIsolate1 = jsSandbox.createIsolate(isolateStartupParameters);
                 JavaScriptIsolate jsIsolate2 = jsSandbox.createIsolate()) {
                ListenableFuture<String> earlyUnresolvedResultFuture =
                        jsIsolate1.evaluateJavaScriptAsync(unresolvedCode);
                ListenableFuture<String> earlyResultFuture =
                        jsIsolate1.evaluateJavaScriptAsync(stableCode);
                ListenableFuture<String> oomResultFuture =
                        jsIsolate1.evaluateJavaScriptAsync(oomingCode);

                // Wait for jsIsolate2 to fully initialize before using jsIsolate1.
                jsIsolate2.evaluateJavaScriptAsync(stableCode).get(5, TimeUnit.SECONDS);

                // Check that the heap limit is enforced and that it reports this was the evaluation
                // that exceeded the limit.
                try {
                    // Use a generous timeout for OOM, as it may involve multiple rounds of garbage
                    // collection.
                    oomResultFuture.get(60, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof MemoryLimitExceededException)) {
                        throw e;
                    }
                }

                // Check that the previously submitted (but unresolved) promise evaluation reports a
                // crash
                try {
                    earlyUnresolvedResultFuture.get(5, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof IsolateTerminatedException)) {
                        throw e;
                    }
                }

                // Check that the previously submitted evaluation which completed before the memory
                // limit was exceeded, but for which we haven't yet gotten the result, returns its
                // result just fine.
                String result = earlyResultFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals(stableExpected, result);

                // Check that a totally new evaluation reports a crash
                ListenableFuture<String> lateResultFuture =
                        jsIsolate1.evaluateJavaScriptAsync(stableCode);
                try {
                    lateResultFuture.get(5, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof IsolateTerminatedException)) {
                        throw e;
                    }
                }

                // Check that other pre-existing isolates can still be used.
                ListenableFuture<String> otherIsolateResultFuture =
                        jsIsolate2.evaluateJavaScriptAsync(stableCode);
                String otherIsolateResult = otherIsolateResultFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals(stableExpected, otherIsolateResult);
            }
        }
    }

    @Test
    @LargeTest
    public void testIsolateCreationAfterCrash() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        // We need to beat the v8 optimizer to ensure it really allocates the required memory. Note
        // that we're allocating an array of elements - not bytes. Filling will ensure that the
        // array is not sparsely allocated.
        final String oomingCode = ""
                + "const array = Array(" + maxHeapSize + ").fill(Math.random(), 0);";
        final String stableCode = "'PASS'";
        final String stableExpected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture1 =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture1.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            try (JavaScriptIsolate jsIsolate1 = jsSandbox.createIsolate(isolateStartupParameters)) {
                ListenableFuture<String> oomResultFuture =
                        jsIsolate1.evaluateJavaScriptAsync(oomingCode);

                // Check that the heap limit is enforced and that it reports this was the evaluation
                // that exceeded the limit.
                try {
                    // Use a generous timeout for OOM, as it may involve multiple rounds of garbage
                    // collection.
                    oomResultFuture.get(60, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof MemoryLimitExceededException)) {
                        throw e;
                    }
                }

                // Check that other isolates can still be created and used (without closing
                // jsIsolate1).
                try (JavaScriptIsolate jsIsolate2 =
                             jsSandbox.createIsolate(isolateStartupParameters)) {
                    ListenableFuture<String> resultFuture =
                            jsIsolate2.evaluateJavaScriptAsync(stableCode);
                    String result = resultFuture.get(5, TimeUnit.SECONDS);
                    Assert.assertEquals(stableExpected, result);
                }
            }

            // Check that other isolates can still be created and used (after closing jsIsolate1).
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                ListenableFuture<String> resultFuture =
                        jsIsolate.evaluateJavaScriptAsync(stableCode);
                String result = resultFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals(stableExpected, result);
            }
        }

        // Check that the old sandbox with the "crashed" isolate can be torn down and that a new
        // sandbox and isolate can be spun up.
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture2 =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture2.get(5, TimeUnit.SECONDS);
             JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(stableCode);
            String result = resultFuture.get(5, TimeUnit.SECONDS);
            Assert.assertEquals(stableExpected, result);
        }
    }

    @Test
    @MediumTest
    public void testAsyncPromiseCallbacks() throws Throwable {
        // Unlike testPromiseReturn and testPromiseEvaluationThrow, this test is guaranteed to
        // exercise promises in an asynchronous way, rather than in ways which cause a promise to
        // resolve or reject immediately within the v8::Script::Run call.
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                // Set up a promise that we can resolve
                final String goodPromiseCode = ""
                        + "let ext_resolve;"
                        + "new Promise((resolve, reject) => {"
                        + " ext_resolve = resolve;"
                        + "})";
                ListenableFuture<String> goodPromiseFuture =
                        jsIsolate.evaluateJavaScriptAsync(goodPromiseCode);

                // Set up a promise that we can reject
                final String badPromiseCode = ""
                        + "let ext_reject;"
                        + "new Promise((resolve, reject) => {"
                        + " ext_reject = reject;"
                        + "})";
                ListenableFuture<String> badPromiseFuture =
                        jsIsolate.evaluateJavaScriptAsync(badPromiseCode);

                // This acts as a barrier to ensure promise code finishes (to the extent of
                // returning the promises) before we ask to evaluate the trigger code - else the
                // potentially async `ext_resolve = resolve` (or `ext_reject = reject`) code might
                // not have been run or queued yet.
                jsIsolate.evaluateJavaScriptAsync("''").get(5, TimeUnit.SECONDS);

                // Trigger the resolve and rejection from another evaluation to ensure the promises
                // are truly asynchronous.
                final String triggerCode = ""
                        + "ext_resolve('I should succeed!');"
                        + "ext_reject(new Error('I should fail!'));"
                        + "'DONE'";
                ListenableFuture<String> triggerFuture =
                        jsIsolate.evaluateJavaScriptAsync(triggerCode);
                String triggerResult = triggerFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals("DONE", triggerResult);

                // Check resolve
                String goodPromiseResult = goodPromiseFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals("I should succeed!", goodPromiseResult);

                // Check reject
                try {
                    String badPromiseResult = badPromiseFuture.get(5, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof EvaluationFailedException)) {
                        throw e;
                    }
                    Assert.assertTrue(e.getCause().getMessage().contains("I should fail!"));
                }
            }
        }
    }

    @Test
    @LargeTest
    public void testArrayBufferSizeEnforced() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        // V8 cannot sparsely allocate array buffers, so no fill required.
        final String oomingCode = ""
                + "const bigArray = new Float32Array(new ArrayBuffer(" + (maxHeapSize + 1) + "));"
                + "'Unreachable'";
        final String stableCode = ""
                + "const smallArray = new Float32Array(new ArrayBuffer(100));"
                + "'PASS'";
        final String stableExpected = "PASS";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                // Check that unserviceable large allocations fail.
                ListenableFuture<String> resultFuture1 =
                        jsIsolate.evaluateJavaScriptAsync(oomingCode);
                try {
                    resultFuture1.get(5, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof EvaluationFailedException)) {
                        throw e;
                    }
                    EvaluationFailedException cause = (EvaluationFailedException) e.getCause();
                    if (!cause.getMessage().startsWith(
                            "Uncaught RangeError: Array buffer allocation failed")) {
                        throw e;
                    }
                }

                // Check that the same isolate can be used to perform a smaller allocation.
                ListenableFuture<String> resultFuture2 =
                        jsIsolate.evaluateJavaScriptAsync(stableCode);
                String result = resultFuture2.get(5, TimeUnit.SECONDS);
                Assert.assertEquals(stableExpected, result);
            }
        }
    }

    @Test
    @LargeTest
    public void testGarbageCollection() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            final long num_doubles = 1024 * 1024;
            // There may be additional allocation overhead beyond this value.
            final long allocation_size = 8 * num_doubles;
            final long memoryUseFactor = 2;
            final long allocationsToTry = memoryUseFactor * maxHeapSize / allocation_size;
            // This test will exercise both the V8 heap and ArrayBuffer-allocated memory. Each will
            // have allocations totalling approximately memoryUseFactor times the available memory.
            //
            // Note that the configured heap limit is not precisely enforced by V8, so we need to go
            // comfortably over our specified limit (and not just by an extra allocation).
            //
            // We use doubles (rather than bytes) to reduce (heap) Array overheads.
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                final String code = ""
                        + "this.arrayLength = " + num_doubles + ";"
                        + "this.obj = {"
                        + " array: Array(this.arrayLength).fill(Math.random(), 0),"
                        + " arraybuffer: new Float64Array(new ArrayBuffer(8 * this.arrayLength)),"
                        + "};"
                        + "'PASS'";
                final String expected = "PASS";
                for (int i = 0; i < allocationsToTry; i++) {
                    ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                    // Execution time will be unstable when GC kicks in, so go with 60 seconds.
                    String result = resultFuture.get(60, TimeUnit.SECONDS);
                    Assert.assertEquals(expected, result);
                }
            }
        }
    }

    @Test
    @LargeTest
    public void testNamedDataCanBeFreed() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            // There will be named data allocations of approximately memoryUseFactor times the
            // available memory. Note that the memory usage in the Java side is constant with
            // respect to number of allocations as we reuse the same input bytes.
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                final byte[] bytes = new byte[LARGE_NAMED_DATA_SIZE];
                final long memoryUseFactor = 2;
                final long allocationsToTry = memoryUseFactor * maxHeapSize / LARGE_NAMED_DATA_SIZE;
                for (int i = 0; i < allocationsToTry; i++) {
                    boolean provideNamedDataReturn = jsIsolate.provideNamedData("id-" + i, bytes);
                    Assert.assertTrue(provideNamedDataReturn);
                    final String code = ""
                            + "android.consumeNamedDataAsArrayBuffer('id-' + " + i + ")"
                            + " .then((arrayBuffer) => {"
                            + "  const len = arrayBuffer.byteLength;"
                            + "  if (len != " + LARGE_NAMED_DATA_SIZE + ") {"
                            + "   throw new Error('Bad length ' + len);"
                            + "  }"
                            + "  return 'PASS';"
                            + " })";
                    final String expected = "PASS";
                    ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                    // Execution time may be unstable if the GC kicks in, so go with 60 seconds.
                    String result = resultFuture.get(60, TimeUnit.SECONDS);
                    Assert.assertEquals(expected, result);
                }
            }
        }
    }

    @Test
    @LargeTest
    public void testNamedDataCanTriggerGarbageCollection() throws Throwable {
        // Array buffers for named data are created differently to ordinary array buffers (see
        // native service code android_webview::JsSandboxIsolate::tryAllocateArrayBuffer). The
        // special allocation code needs to run the garbage collector if we've run out of external
        // memory budget. We test this by using up the budget with array buffers (such that there
        // would not be enough space to consume the named data), and then forget about (turn into
        // garbage) enough buffer memory for the allocation to succeed. This means that when we run
        // our own allocation code, there is only enough memory to proceed after a garbage
        // collection (but not without one).
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        final byte[] bytes = new byte[LARGE_NAMED_DATA_SIZE];
        final long allocationsToTry = (maxHeapSize / LARGE_NAMED_DATA_SIZE) + 1;
        final String code = ""
                + "const allocation_size = " + LARGE_NAMED_DATA_SIZE + ";"
                + "this.array_buffers = new Array(" + allocationsToTry + ");"
                + "let i;"
                + "for (i = 0; i < this.array_buffers.length; i++) {"
                + " try {"
                + "  this.array_buffers[i] = new ArrayBuffer(allocation_size);"
                + " } catch (e) {"
                + "  if (e instanceof RangeError) {"
                + "   break;"
                + "  }"
                + " }"
                + "}"
                + "if (i == this.array_buffers.length) {"
                + " throw new Error('Expected to run out of memory, but did not');"
                + "} else if (i == 0) {"
                + " throw new Error('Could not achieve at least one allocation');"
                + "}"
                + "this.array_buffers[0] = null;"
                + "android.consumeNamedDataAsArrayBuffer('test')"
                + " .then((arrayBuffer) => {"
                + "  const len = arrayBuffer.byteLength;"
                + "  if (len != " + LARGE_NAMED_DATA_SIZE + ") {"
                + "   throw new Error('Bad length ' + len);"
                + "  }"
                + "  return 'PASS';"
                + " })";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                boolean provideNamedDataReturn = jsIsolate.provideNamedData("test", bytes);
                Assert.assertTrue(provideNamedDataReturn);
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                // Execution time may be unstable if the GC kicks in, so go with 60 seconds.
                String result = resultFuture.get(60, TimeUnit.SECONDS);
                Assert.assertEquals(expected, result);
            }
        }
    }

    @Test
    @LargeTest
    public void testArrayBuffersAllocatedInPages() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            // The service code should assume that small allocations have overhead and will cost at
            // least a 4096 byte page size. (Even if the page size was larger, that shouldn't
            // interfere with the accuracy of the test.)
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                final long allocations = maxHeapSize / 4096 + 1;
                // We need to use `new Uint8Array(new ArrayBuffer(1))` instead of `new
                // Uint8Array(1)` because V8 appears to instead internalize (onto the V8 heap)
                // smaller directly constructed typed arrays.
                final String code = ""
                        + "(function(){"
                        + " const arrayLength = " + allocations + ";"
                        + " const buffers = Array(arrayLength);"
                        + " for (let i = 0; i < arrayLength; i++) {"
                        + "  try {"
                        + "   buffers[i] = new Uint8Array(new ArrayBuffer(1));"
                        + "  } catch (e) {"
                        + "   if (e instanceof RangeError) {"
                        + "    return i;"
                        + "   } else {"
                        + "    throw e;"
                        + "   }"
                        + "  }"
                        + " }"
                        + " return 'FAIL';"
                        + "})()";
                final String notExpected = "FAIL";
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                String result = resultFuture.get(60, TimeUnit.SECONDS);
                Assert.assertNotEquals(notExpected, result);
            }
            // Allocating one larger contiguous array buffer should not incur significant overhead.
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                // At most maxHeapSize is available to array buffers, but maybe less. Go with
                // something less than the full limit, but which is still much more than what was
                // logically requested by the many smaller buffers.
                final long size = maxHeapSize / 8;
                final String code = ""
                        + "const buffer = new Uint8Array(new ArrayBuffer(" + size + "));"
                        + "'PASS'";
                final String expected = "PASS";
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                String result = resultFuture.get(10, TimeUnit.SECONDS);
                Assert.assertEquals(expected, result);
            }
        }
    }

    @Test
    @LargeTest
    public void testOversizedNamedData() throws Throwable {
        final long maxHeapSize = REASONABLE_HEAP_SIZE;
        final long largeSize = (maxHeapSize + 1L);
        Assert.assertTrue(largeSize <= Integer.MAX_VALUE);
        final byte[] largeBytes = new byte[(int) largeSize];
        final String provideString = "Hello World";
        final byte[] smallBytes = provideString.getBytes(StandardCharsets.US_ASCII);
        // Test that attempting to consume an oversized named data into a new array buffer fails
        // with a RangeError, and a subsequent smaller request succeeds.
        final String code = ""
                + "function ab2str(buf) {"
                + " return String.fromCharCode.apply(null, new Uint8Array(buf));"
                + "}"
                + "async function test() {"
                + " try {"
                + "  await android.consumeNamedDataAsArrayBuffer('large');"
                + "  throw new Error('consumption of large named data should not have succeeded');"
                + " } catch (e) {"
                + "  if (!(e instanceof RangeError)) {"
                + "   throw e;"
                + "  }"
                + " }"
                + " const buffer = await android.consumeNamedDataAsArrayBuffer('small');"
                + " return await ab2str(buffer);"
                + "}"
                + "test()";
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE));
            Assume.assumeTrue(
                    jsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_PROMISE_RETURN));
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));
            IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
            isolateStartupParameters.setMaxHeapSizeBytes(maxHeapSize);
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(isolateStartupParameters)) {
                boolean provideNamedDataLargeReturn =
                        jsIsolate.provideNamedData("large", largeBytes);
                Assert.assertTrue(provideNamedDataLargeReturn);
                boolean provideNamedDataSmallReturn =
                        jsIsolate.provideNamedData("small", smallBytes);
                Assert.assertTrue(provideNamedDataSmallReturn);
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                String result = resultFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals(provideString, result);
            }
        }
    }

    @Test
    @LargeTest
    public void testUnconsumedNamedData() throws Throwable {
        // Ensure that creating and discarding loads of separate unconsumed named data do not result
        // in leaks (particularly memory, file descriptors, and threads).
        final byte[] bytes = new byte[LARGE_NAMED_DATA_SIZE];
        final int numIsolates = 100;
        final int numNames = 100;
        Context context = ContextUtils.getApplicationContext();
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER));
            for (int i = 0; i < numIsolates; i++) {
                try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                    for (int j = 0; j < numNames; j++) {
                        boolean provideNamedDataReturn =
                                jsIsolate.provideNamedData("id-" + j, bytes);
                        Assert.assertTrue(provideNamedDataReturn);
                    }
                }
            }
        }
    }

    @Test
    @LargeTest
    public void testLargeScriptJsEvaluation() throws Throwable {
        String longString = "a".repeat(2000000);
        final String code = ""
                + "let " + longString + " = 0;"
                + "\"PASS\"";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_EVALUATE_WITHOUT_TRANSACTION_LIMIT));
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                String result = resultFuture.get(10, TimeUnit.SECONDS);

                Assert.assertEquals(expected, result);
            }
        }
    }

    @Test
    @LargeTest
    public void testLargeScriptByteArrayJsEvaluation() throws Throwable {
        final String longString = "a".repeat(2000000);
        final String codeString = ""
                + "let " + longString + " = 0;"
                + "\"PASS\"";
        final byte[] code = codeString.getBytes(StandardCharsets.UTF_8);
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_EVALUATE_WITHOUT_TRANSACTION_LIMIT));
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                String result = resultFuture.get(10, TimeUnit.SECONDS);

                Assert.assertEquals(expected, result);
            }
        }
    }

    @Test
    @LargeTest
    public void testLargeReturn() throws Throwable {
        final String longString = "a".repeat(2000000);
        final String code = "'a'.repeat(2000000);";
        final String expected = longString;
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_EVALUATE_WITHOUT_TRANSACTION_LIMIT));
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                String result = resultFuture.get(60, TimeUnit.SECONDS);

                Assert.assertEquals(expected, result);
            }
        }
    }

    @Test
    @LargeTest
    public void testLargeError() throws Throwable {
        final String longString = "a".repeat(2000000);
        final String code = "throw \"" + longString + "\");";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_EVALUATE_WITHOUT_TRANSACTION_LIMIT));
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate()) {
                ListenableFuture<String> resultFuture = jsIsolate.evaluateJavaScriptAsync(code);
                try {
                    resultFuture.get(5, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    Assert.assertTrue(
                            e.getCause().getClass().equals(EvaluationFailedException.class));
                    Assert.assertTrue(e.getCause().getMessage().contains(longString));
                }
            }
        }
    }

    @Test
    @MediumTest
    public void testResultSizeEnforced() throws Throwable {
        final int maxSize = 100;
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(context);
        try (JavaScriptSandbox jsSandbox = jsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assume.assumeTrue(jsSandbox.isFeatureSupported(
                    JavaScriptSandbox.JS_FEATURE_EVALUATE_WITHOUT_TRANSACTION_LIMIT));
            IsolateStartupParameters settings = new IsolateStartupParameters();
            settings.setMaxEvaluationReturnSizeBytes(maxSize);
            try (JavaScriptIsolate jsIsolate = jsSandbox.createIsolate(settings)) {
                // Running code that returns greater than `maxSize` number of bytes should throw.
                final String greaterThanMaxSizeCode = ""
                        + "'a'.repeat(" + (maxSize + 1) + ");";
                ListenableFuture<String> greaterThanMaxSizeResultFuture =
                        jsIsolate.evaluateJavaScriptAsync(greaterThanMaxSizeCode);
                try {
                    greaterThanMaxSizeResultFuture.get(5, TimeUnit.SECONDS);
                    Assert.fail("Should have thrown.");
                } catch (ExecutionException e) {
                    if (!(e.getCause() instanceof EvaluationResultSizeLimitExceededException)) {
                        throw e;
                    }
                }

                // Running code that returns `maxSize` number of bytes should not throw.
                final String maxSizeCode = ""
                        + "'a'.repeat(" + maxSize + ");";
                final String maxSizeExpected = "a".repeat(maxSize);
                ListenableFuture<String> maxSizeResultFuture =
                        jsIsolate.evaluateJavaScriptAsync(maxSizeCode);
                String maxSizeResult = maxSizeResultFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals(maxSizeExpected, maxSizeResult);

                // Running code that returns less than `maxSize` number of bytes should not throw.
                final String lessThanMaxSizeCode = ""
                        + "'a'.repeat(" + (maxSize - 1) + ");";
                final String lessThanMaxSizeExpected = "a".repeat(maxSize - 1);
                ListenableFuture<String> lessThanMaxSizeResultFuture =
                        jsIsolate.evaluateJavaScriptAsync(lessThanMaxSizeCode);
                String lessThanMaxSizeResult = lessThanMaxSizeResultFuture.get(5, TimeUnit.SECONDS);
                Assert.assertEquals(lessThanMaxSizeExpected, lessThanMaxSizeResult);
            }
        }
    }
}
