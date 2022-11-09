// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.autofill.ChromeAutocompleteSafeModeAction;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MaxAndroidSdkLevel;

import java.util.Set;

/**
 * Tests for WebView ChromeAutocompleteSafeMode.
 *
 * Chrome autocomplete is only used when WebView is running in Android < O.
 * In >= O Android Autofill system is used. See AndroidAutofillSafeModeTest
 * for tests of the corresponding SafeMode action.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MaxAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
public class ChromeAutocompleteSafeModeTest {
    public static final String TAG = "AndroidAutofillTest";

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    @After
    public void tearDown() {
        SafeModeController.getInstance().unregisterActionsForTesting();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeDisabled() throws Throwable {
        AwTestContainerView mTestContainerView = mRule.createAwTestContainerViewOnMainSync(
                new TestAwContentsClient(), false, new TestDependencyFactory());

        assertTrue("SaveFormData should be enabled when safe mode is disabled",
                mRule.getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                        .getSaveFormData());
        assertNotNull("AutofillClient shouldn't be null when safe mode is disabled",
                mTestContainerView.getAwContents().getAutofillClient());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeEnabledAfterWebViewStartUp() throws Throwable {
        SafeModeController safeModeController = SafeModeController.getInstance();
        safeModeController.registerActions(
                new SafeModeAction[] {new ChromeAutocompleteSafeModeAction()});
        safeModeController.executeActions(Set.of(ChromeAutocompleteSafeModeAction.ID));

        AwTestContainerView mTestContainerView = mRule.createAwTestContainerViewOnMainSync(
                new TestAwContentsClient(), false, new TestDependencyFactory());

        assertFalse("SaveFormData should be disabled when safe mode is enabled",
                mRule.getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                        .getSaveFormData());
        assertNull("AutofillClient should be null when safe mode is enabled",
                mTestContainerView.getAwContents().getAutofillClient());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void
    testSafeModeEnabledAfterSetSaveFormDataEnabledCalled() throws Throwable {
        SafeModeController safeModeController = SafeModeController.getInstance();
        safeModeController.registerActions(
                new SafeModeAction[] {new ChromeAutocompleteSafeModeAction()});
        safeModeController.executeActions(Set.of(ChromeAutocompleteSafeModeAction.ID));

        AwTestContainerView mTestContainerView = mRule.createAwTestContainerViewOnMainSync(
                new TestAwContentsClient(), false, new TestDependencyFactory());
        mRule.getAwSettingsOnUiThread(mTestContainerView.getAwContents()).setSaveFormData(true);

        assertFalse("SaveFormData should be disabled when safe mode is enabled",
                mRule.getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                        .getSaveFormData());
        assertNull("AutofillClient should be null when safe mode is enabled",
                mTestContainerView.getAwContents().getAutofillClient());
    }
}
