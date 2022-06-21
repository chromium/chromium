// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_js_sandbox_test.test;

import android.content.Context;

import androidx.test.filters.MediumTest;

import com.google.common.util.concurrent.ListenableFuture;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.js_sandbox.client.AwJsIsolate;
import org.chromium.android_webview.js_sandbox.client.AwJsSandbox;
import org.chromium.android_webview.js_sandbox.client.EvaluationFailedException;
import org.chromium.android_webview.js_sandbox.client.IsolateTerminatedException;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.nio.charset.StandardCharsets;
import java.util.Vector;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/** Instrumentation test for JsSandboxService. */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewJsSandboxTest {
    @Test
    @MediumTest
    public void testSimpleJsEvaluation() throws Throwable {
        final String code = "\"PASS\"";
        final String expected = "PASS";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascript(code);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate1 = jsSandbox.createIsolate();
        AwJsIsolate jsIsolate2 = jsSandbox.createIsolate();
        jsIsolate1.close();
        ListenableFuture<String> resultFuture = jsIsolate2.evaluateJavascript(code);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate1 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavascript(code1);
        String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate2 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture2 = jsIsolate2.evaluateJavascript(code2);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate1 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavascript(code1);
        String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate2 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture2 = jsIsolate2.evaluateJavascript(code2);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate1 = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture1 = jsIsolate1.evaluateJavascript(code1);
        String result1 = resultFuture1.get(5, TimeUnit.SECONDS);
        ListenableFuture<String> resultFuture2 = jsIsolate1.evaluateJavascript(code2);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascript(code);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascript(code);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate = jsSandbox.createIsolate();
        Vector<ListenableFuture<String>> resultFutures = new Vector<ListenableFuture<String>>();
        for (int i = 0; i < num_of_evaluations; i++) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascript(code);
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
    public void testFeatureDetection() throws Throwable {
        ListenableFuture<AwJsSandbox> awJsSandboxFuture =
                AwJsSandbox.newConnectedInstance(ContextUtils.getApplicationContext());
        try (AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS)) {
            Assert.assertTrue(jsSandbox.isIsolateTerminationSupported());
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
        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        try (AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
                AwJsIsolate jsIsolate = jsSandbox.createIsolate()) {
            boolean provideNamedDataReturn = jsIsolate.provideNamedData("id-1", bytes);
            Assert.assertTrue(provideNamedDataReturn);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascript(code);
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
        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        try (AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
                AwJsIsolate jsIsolate = jsSandbox.createIsolate()) {
            boolean provideNamedDataReturn = jsIsolate.provideNamedData("id-1", bytes);
            Assert.assertTrue(provideNamedDataReturn);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascript(code);
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
        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        try (AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
                AwJsIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascript(code);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        try (AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
                AwJsIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascript(code1);
            ListenableFuture<String> resultFuture2 = jsIsolate.evaluateJavascript(code2);
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
        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        try (AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
                AwJsIsolate jsIsolate = jsSandbox.createIsolate()) {
            jsIsolate.provideNamedData("id-1", bytes);
            jsIsolate.provideNamedData("id-2", bytes);
            jsIsolate.provideNamedData("id-3", bytes);
            jsIsolate.provideNamedData("id-4", bytes);
            jsIsolate.provideNamedData("id-5", bytes);
            Thread.sleep(1000);
            ListenableFuture<String> resultFuture1 = jsIsolate.evaluateJavascript(code);
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

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        try (AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
                AwJsIsolate jsIsolate = jsSandbox.createIsolate()) {
            ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascript(code);
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
}
