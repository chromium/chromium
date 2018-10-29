// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.jsdialog;

import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.jsdialog.JavascriptTabModalDialog;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/**
 * Test JavaScript modal dialogs in VR.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
public class VrBrowserJavaScriptModalDialogTest {
    @Rule
    public ChromeTabbedActivityVrTestRule mActivityTestRule = new ChromeTabbedActivityVrTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("components/test/data/js_dialogs/render_tests");

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        mActivity = mActivityTestRule.getActivity();
    }

    /**
     * Verifies modal alert-dialog appearance and that it looks as it is expected.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testAlertModalDialog() throws ExecutionException, IOException {
        testModalDialogImpl("js_modal_view_vr_alert", "alert('Hello Android!')");
    }

    /**
     * Verifies modal confirm-dialog appearance and that it looks as it is expected.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testConfirmModalDialog() throws ExecutionException, IOException {
        testModalDialogImpl("js_modal_view_vr_confirm", "confirm('Deny?')");
    }

    /**
     * Verifies modal prompt-dialog appearance and that it looks as it is expected.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testPromptModalDialog() throws ExecutionException, IOException {
        testModalDialogImpl(
                "js_modal_view_vr_prompt", "prompt('Is the tree closed?', 'Hopefully not')");
    }

    private void testModalDialogImpl(String name, String js)
            throws ExecutionException, IOException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        executeJavaScriptAndWaitForDialog(js);

        JavascriptTabModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        Assert.assertEquals(NativeUiUtils.getVrViewContainer().getChildCount(), 1);
        mRenderTestRule.render(NativeUiUtils.getVrViewContainer().getChildAt(0), name);
    }

    /**
     * Asynchronously executes the given code for spawning a dialog and waits
     * for the dialog to be visible.
     */
    private void executeJavaScriptAndWaitForDialog(String script) {
        JavaScriptUtils.executeJavaScript(mActivity.getCurrentWebContents(), script);
        NativeUiUtils.waitForModalDialogStatus(true /* shouldBeShown */, mActivity);
    }

    /**
     * Returns the current JavaScript modal dialog showing or null if no such dialog is currently
     * showing.
     */
    private JavascriptTabModalDialog getCurrentDialog() throws ExecutionException {
        return (JavascriptTabModalDialog) ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getModalDialogManager().getCurrentDialogForTest().getController());
    }
}
