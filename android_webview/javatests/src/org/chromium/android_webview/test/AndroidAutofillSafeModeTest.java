// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.autofill.AndroidAutofillSafeModeAction;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.Set;

/**
 * Tests for WebView AndroidAutofillSafeMode.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
@RequiresApi(Build.VERSION_CODES.O)
public class AndroidAutofillSafeModeTest {
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
    public void testAutofillProviderNotInitialised() throws Throwable {
        // Given
        SafeModeController safeModeController = SafeModeController.getInstance();
        safeModeController.registerActions(
                new SafeModeAction[] {new AndroidAutofillSafeModeAction()});
        safeModeController.executeActions(Set.of(AndroidAutofillSafeModeAction.ID));

        // When
        AwTestContainerView mTestContainerView = mRule.createAwTestContainerViewOnMainSync(
                new TestAwContentsClient(), false, new TestDependencyFactory());

        // Then
        assertNull(mTestContainerView.getAwContents().getAutofillProviderForTesting());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeActionSavesState() throws Throwable {
        // Given
        assertFalse(AndroidAutofillSafeModeAction.isAndroidAutofillDisabled());

        // When
        new AndroidAutofillSafeModeAction().execute();

        // Then
        assertTrue(AndroidAutofillSafeModeAction.isAndroidAutofillDisabled());
    }
}
