// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.js.browser.AwJsContext;
import org.chromium.android_webview.js.browser.AwJsSandbox;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;

/** Instrumentation test for JsSandboxService. */
@RunWith(AwJUnit4ClassRunner.class)
public class JsSandboxServiceTest {
    private class TestExecutionCallback implements AwJsContext.ExecutionCallback {
        public CallbackHelper helper = new CallbackHelper();
        public String result;
        public String error;

        @Override
        public void reportResult(String result) {
            this.result = result;
            helper.notifyCalled();
        }

        @Override
        public void reportError(String error) {
            this.error = error;
            helper.notifyCalled();
        }
    }

    @Test
    @MediumTest
    public void testSimpleJsEvaluation() throws Throwable {
        final String code = "'PASS'";
        final String expected = "PASS";
        TestExecutionCallback callback = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance((AwJsSandbox jsSandbox) -> {
            AwJsContext jsContext = jsSandbox.createContext();
            jsContext.evaluateJavascript(code, callback);
        });

        callback.helper.waitForCallback("Timed out waiting for reportResult() to be called", 0);
        Assert.assertEquals(expected, callback.result);
    }

    @Test
    @MediumTest
    public void testClosingOneContext() throws Throwable {
        final String code = "'PASS'";
        final String expected = "PASS";
        TestExecutionCallback callback = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance((AwJsSandbox jsSandbox) -> {
            AwJsContext jsContext1 = jsSandbox.createContext();
            AwJsContext jsContext2 = jsSandbox.createContext();
            jsContext1.close();
            jsContext2.evaluateJavascript(code, callback);
            jsContext2.close();
        });

        callback.helper.waitForCallback("Timed out waiting for reportResult() to be called", 0);
        Assert.assertEquals(expected, callback.result);
    }

    @Test
    @MediumTest
    public void testEvaluationInTwoContexts() throws Throwable {
        final String code1 = "this.x = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.x = 'SUPER_PASS';\n";
        final String expected2 = "SUPER_PASS";
        TestExecutionCallback callback1 = new TestExecutionCallback();
        TestExecutionCallback callback2 = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance((AwJsSandbox jsSandbox) -> {
            AwJsContext jsContext1 = jsSandbox.createContext();
            jsContext1.evaluateJavascript(code1, callback1);
            AwJsContext jsContext2 = jsSandbox.createContext();
            jsContext2.evaluateJavascript(code2, callback2);
        });
        callback1.helper.waitForCallback(
                "Timed out waiting for reportResult() to be called for first case", 0);
        callback2.helper.waitForCallback(
                "Timed out waiting for reportResult() to be called for second case", 0);

        Assert.assertEquals(expected1, callback1.result);
        Assert.assertEquals(expected2, callback2.result);
    }

    @Test
    @MediumTest
    public void testTwoContextsDoNotShareEnvironment() throws Throwable {
        final String code1 = "this.y = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.y = this.y + ' PASS';\n";
        final String expected2 = "undefined PASS";
        TestExecutionCallback callback1 = new TestExecutionCallback();
        TestExecutionCallback callback2 = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance((AwJsSandbox jsSandbox) -> {
            AwJsContext jsContext1 = jsSandbox.createContext();
            jsContext1.evaluateJavascript(code1, callback1);
            AwJsContext jsContext2 = jsSandbox.createContext();
            jsContext2.evaluateJavascript(code2, callback2);
        });
        callback1.helper.waitForCallback(
                "Timed out waiting for reportResult() to be called for first case", 0);
        callback2.helper.waitForCallback(
                "Timed out waiting for reportResult() to be called for second case", 0);

        Assert.assertEquals(expected1, callback1.result);
        Assert.assertEquals(expected2, callback2.result);
    }

    @Test
    @MediumTest
    public void testTwoExecutionsShareEnvironment() throws Throwable {
        final String code1 = "this.z = 'PASS';\n";
        final String expected1 = "PASS";
        final String code2 = "this.z = this.z + ' PASS';\n";
        final String expected2 = "PASS PASS";
        TestExecutionCallback callback1 = new TestExecutionCallback();
        TestExecutionCallback callback2 = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance((AwJsSandbox jsSandbox) -> {
            AwJsContext jsContext1 = jsSandbox.createContext();
            jsContext1.evaluateJavascript(code1, callback1);
            jsContext1.evaluateJavascript(code2, callback2);
        });
        callback1.helper.waitForCallback(
                "Timed out waiting for reportResult() to be called for first case", 0);
        callback2.helper.waitForCallback(
                "Timed out waiting for reportResult() to be called for second case", 0);

        Assert.assertEquals(expected1, callback1.result);
        Assert.assertEquals(expected2, callback2.result);
    }

    @Test
    @MediumTest
    public void testJsEvaluationError() throws Throwable {
        final String code = ".";
        final String contains = "SyntaxError";
        TestExecutionCallback callback = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance(jsSandbox -> {
            AwJsContext jsContext = jsSandbox.createContext();
            jsContext.evaluateJavascript(code, callback);
        });

        callback.helper.waitForCallback("Timed out waiting for reportError() to be called", 0);
        Assert.assertTrue(callback.error.contains(contains));
    }
}
