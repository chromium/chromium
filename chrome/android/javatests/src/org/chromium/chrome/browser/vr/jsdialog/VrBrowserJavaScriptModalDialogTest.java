// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.jsdialog;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.vr.UserFriendlyElementName;
import org.chromium.chrome.browser.vr.VrBrowserTestFramework;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.RenderTestUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.io.IOException;

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
    private VrBrowserTestFramework mVrBrowserTestFramework;

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.getActivity();
        mVrBrowserTestFramework = new VrBrowserTestFramework(mActivityTestRule);
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("2d_permission_page"),
                PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        // Wait for any residual animations from entering VR to finish so that they don't get caught
        // later.
        NativeUiUtils.waitForUiQuiescence();
    }

    /**
     * Verifies modal alert-dialog appearance and that it looks as it is expected.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testAlertModalDialog() throws InterruptedException, IOException {
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.BROWSING_DIALOG, true /* visible */, () -> {
                        JavaScriptUtils.executeJavaScript(
                                mActivity.getCurrentWebContents(), "alert('Hello Android!')");
                    });
        });
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "js_modal_view_vr_alert_visible_browser_ui", mRenderTestRule);
        // No assertNoJavaScriptErrors since the alert is still visible, preventing further
        // JavaScript execution.
    }

    /**
     * Verifies modal confirm-dialog appearance and that it looks as it is expected. Additionally,
     * verifies that its "Cancel" button works as expected.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testConfirmModalDialog() throws InterruptedException, IOException {
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.BROWSING_DIALOG, true /* visible */, () -> {
                        JavaScriptUtils.executeJavaScript(
                                mActivity.getCurrentWebContents(), "var c = confirm('Deny?')");
                    });
        });
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "js_modal_view_vr_confirm_visible_browser_ui", mRenderTestRule);
        NativeUiUtils.clickFallbackUiNegativeButton();
        // Ensure the cancel button was clicked.
        Assert.assertTrue("JavaScript Confirm's cancel button was not clicked",
                mVrBrowserTestFramework.runJavaScriptOrFail("c", POLL_TIMEOUT_SHORT_MS)
                        .equals("false"));
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "js_modal_view_vr_confirm_canceled_browser_ui", mRenderTestRule);
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Verifies modal prompt-dialog appearance and that it looks as it is expected. Additionally,
     * verifies that its "Ok" button works as expected.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testPromptModalDialog() throws InterruptedException, IOException {
        String expectedString = "Hopefully not";
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.BROWSING_DIALOG, true /* visible */, () -> {
                        JavaScriptUtils.executeJavaScript(mActivity.getCurrentWebContents(),
                                "var p = prompt('Is the tree closed?', '" + expectedString + "')");
                    });
        });
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "js_modal_view_vr_prompt_visible_browser_ui", mRenderTestRule);
        NativeUiUtils.clickFallbackUiPositiveButton();
        // This JavaScript will only run once the prompt has been dismissed, and the return value
        // will only be what we expect if the positive button was actually clicked (as opposed to
        // canceled).
        Assert.assertTrue("JavaScript Prompt's OK button was not clicked",
                mVrBrowserTestFramework
                        .runJavaScriptOrFail("p == '" + expectedString + "'", POLL_TIMEOUT_SHORT_MS)
                        .equals("true"));
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "js_modal_view_vr_prompt_submitted_browser_ui", mRenderTestRule);
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }
}
