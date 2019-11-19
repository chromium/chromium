// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.app.Instrumentation;

import org.junit.Assert;

import org.chromium.android_webview.AwContents;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

/**
 * Collection of functions for JavaScript-based interactions with a page.
 */
public class JSUtils {
    private static final long WAIT_TIMEOUT_MS = 2000L;
    private static final int CHECK_INTERVAL = 100;

    private static String createScriptToClickNode(String nodeId) {
        String script = "var evObj = new MouseEvent('click', {bubbles: true});"
                + "document.getElementById('" + nodeId + "').dispatchEvent(evObj);"
                + "console.log('element with id [" + nodeId + "] clicked');";
        return script;
    }

    public static void clickOnLinkUsingJs(final Instrumentation instrumentation,
            final AwContents awContents,
            final OnEvaluateJavaScriptResultHelper onEvaluateJavaScriptResultHelper,
            final String linkId) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    String linkIsNotNull = executeJavaScriptAndWaitForResult(instrumentation,
                            awContents, onEvaluateJavaScriptResultHelper,
                            "document.getElementById('" + linkId + "') != null");
                    return linkIsNotNull.equals("true");
                } catch (Throwable t) {
                    t.printStackTrace();
                    Assert.fail("Failed to check if DOM is loaded: " + t.toString());
                    return false;
                }
            }
        }, WAIT_TIMEOUT_MS, CHECK_INTERVAL);

        // clang-format off
        instrumentation.runOnMainSync(
                () -> awContents.getWebContents().evaluateJavaScriptForTests(
                        createScriptToClickNode(linkId), null));
        // clang-format on
    }

    public static void clickNodeWithUserGesture(WebContents webContents, String nodeId) {
        WebContentsUtils.evaluateJavaScriptWithUserGesture(
                webContents, createScriptToClickNode(nodeId));
    }

    public static String executeJavaScriptAndWaitForResult(Instrumentation instrumentation,
            final AwContents awContents,
            final OnEvaluateJavaScriptResultHelper onEvaluateJavaScriptResultHelper,
            final String code) throws Exception {
        instrumentation.runOnMainSync(
                () -> onEvaluateJavaScriptResultHelper.evaluateJavaScriptForTests(
                        awContents.getWebContents(), code));
        onEvaluateJavaScriptResultHelper.waitUntilHasValue();
        Assert.assertTrue("Failed to retrieve JavaScript evaluation results.",
                onEvaluateJavaScriptResultHelper.hasValue());
        return onEvaluateJavaScriptResultHelper.getJsonResultAndClear();
    }
}
