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
import org.chromium.android_webview.js_sandbox.client.JsEvaluationException;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/** Instrumentation test for JsSandboxService. */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewJsSandboxTest {
    @Test
    @MediumTest
    public void testSimpleJsEvaluation() throws Throwable {
        final String code = "'PASS'";
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
        final String code = ".";
        final String contains = "SyntaxError";
        Context context = ContextUtils.getApplicationContext();

        ListenableFuture<AwJsSandbox> awJsSandboxFuture = AwJsSandbox.newConnectedInstance(context);
        AwJsSandbox jsSandbox = awJsSandboxFuture.get(5, TimeUnit.SECONDS);
        AwJsIsolate jsIsolate = jsSandbox.createIsolate();
        ListenableFuture<String> resultFuture = jsIsolate.evaluateJavascript(code);
        boolean isOfTypeJsEvaluationException = false;
        String error = "";
        try {
            String result = resultFuture.get(5, TimeUnit.SECONDS);
        } catch (ExecutionException e) {
            isOfTypeJsEvaluationException =
                    e.getCause().getClass().equals(JsEvaluationException.class);
            error = e.getCause().getMessage();
        }
        jsIsolate.close();
        jsSandbox.close();

        Assert.assertTrue(isOfTypeJsEvaluationException);
        Assert.assertTrue(error.contains(contains));
    }
}
