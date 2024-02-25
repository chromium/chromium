// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;


import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.autofill.AndroidAutofillSafeModeAction;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/** Tests for WebView AndroidAutofillSafeMode. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AndroidAutofillSafeModeTest extends AwParameterizedTest {
    public static final String TAG = "AndroidAutofillTest";

    @Rule public AwActivityTestRule mRule;

    public AndroidAutofillSafeModeTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

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
        safeModeController.executeActions(Set.of(SafeModeActionIds.DISABLE_ANDROID_AUTOFILL));

        // When
        AwTestContainerView mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
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
