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

    // Test sending data to the service and retrieving it back.
    // TODO(crbug.com/1297672): Currently this test only checks if evaluateJavascript() can convert
    // the input string to uppercase. This needs to be modified to test the actual
    // behaviour of the evaluateJavascript() method once implemented.
    @Test
    @MediumTest
    public void testJsEvaluation() throws Throwable {
        final String smallCase = "helloworld";
        final String expected = "HELLOWORLD";
        TestExecutionCallback callback = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance(jsSandbox -> {
            AwJsContext jsContext = jsSandbox.createContext();
            jsContext.evaluateJavascript(smallCase, callback);
        });

        callback.helper.waitForCallback("Timed out waiting for reportResult() to be called", 0);
        Assert.assertEquals(expected, callback.result);
    }

    @Test
    @MediumTest
    public void testClosingOneContextBeforeExecutingOther() throws Throwable {
        final String smallCase = "helloworld";
        final String expected = "HELLOWORLD";
        TestExecutionCallback callback = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance(jsSandbox -> {
            AwJsContext jsContext1 = jsSandbox.createContext();
            AwJsContext jsContext2 = jsSandbox.createContext();
            jsContext1.close();
            jsContext2.evaluateJavascript(smallCase, callback);
            jsContext2.close();
        });

        callback.helper.waitForCallback("Timed out waiting for reportResult() to be called", 0);
        Assert.assertEquals(expected, callback.result);
    }

    @Test
    @MediumTest
    public void testJsEvaluationError() throws Throwable {
        final String smallCase = "ERROR";
        final String expected = "There has been an error.";
        TestExecutionCallback callback = new TestExecutionCallback();

        AwJsSandbox.newConnectedInstance(jsSandbox -> {
            AwJsContext jsContext = jsSandbox.createContext();
            jsContext.evaluateJavascript(smallCase, callback);
        });

        callback.helper.waitForCallback("Timed out waiting for reportResult() to be called", 0);
        Assert.assertEquals(expected, callback.error);
    }
}
