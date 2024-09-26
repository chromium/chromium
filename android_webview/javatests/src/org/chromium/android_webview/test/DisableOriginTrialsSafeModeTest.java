// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.jni_zero.JNINamespace;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.origin_trial.DisableOriginTrialsSafeModeAction;
import org.chromium.android_webview.test.util.DisableOriginTrialsSafeModeTestUtilsJni;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/** Tests for WebView DisableOriginTrialsSafeMode. */
@JNINamespace("android_webview")
@RunWith(Parameterized.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class DisableOriginTrialsSafeModeTest extends AwParameterizedTest {
    public static final String TAG = "DisableOriginTrialsSafeModeTest";

    @Rule public AwActivityTestRule mRule;

    public DisableOriginTrialsSafeModeTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @After
    public void tearDown() {
        SafeModeController.getInstance().unregisterActionsForTesting();
    }

    @Test
    @SmallTest
    @Feature("AndroidWebview")
    public void testOriginTrialsSafeModeSavesState() {
        // Given
        assertFalse(DisableOriginTrialsSafeModeAction.isDisableOriginTrialsEnabled());

        // When
        new DisableOriginTrialsSafeModeAction().execute();

        // Then
        assertTrue(DisableOriginTrialsSafeModeAction.isDisableOriginTrialsEnabled());
    }

    @Test
    @SmallTest
    @Feature("AndroidWebview")
    public void testSafeModeOnTrialsStatus() throws Throwable {
        // Given
        SafeModeController safeModeController = SafeModeController.getInstance();
        safeModeController.registerActions(
                new SafeModeAction[] {new DisableOriginTrialsSafeModeAction()});
        safeModeController.executeActions(Set.of(SafeModeActionIds.DISABLE_ORIGIN_TRIALS));

        // Then
        assertTrue(
                "Expect a valid origin trial policy",
                DisableOriginTrialsSafeModeTestUtilsJni.get().doesPolicyExist());
        assertTrue(
                "Expect a allow_only_deprecation_trial flag set true",
                DisableOriginTrialsSafeModeTestUtilsJni.get().isFlagSet());
        assertTrue(
                "Expect a non-deprecation trial disabled.",
                DisableOriginTrialsSafeModeTestUtilsJni.get().isNonDeprecationTrialDisabled());
        assertFalse(
                "Expect a deprecation trial enabled.",
                DisableOriginTrialsSafeModeTestUtilsJni.get().isDeprecationTrialDisabled());
    }

    @Test
    @SmallTest
    @Feature("AndroidWebview")
    public void testSafeModeOffOriginTrialPolicy() throws Throwable {
        // Then
        assertTrue(
                "Expect a valid origin trial policy",
                DisableOriginTrialsSafeModeTestUtilsJni.get().doesPolicyExist());
        assertFalse(
                "Expect a allow_only_deprecation_trial flag set false",
                DisableOriginTrialsSafeModeTestUtilsJni.get().isFlagSet());
        assertFalse(
                "Expect a non-deprecation trial enabled.",
                DisableOriginTrialsSafeModeTestUtilsJni.get().isNonDeprecationTrialDisabled());
        assertFalse(
                "Expect a deprecation trial enabled.",
                DisableOriginTrialsSafeModeTestUtilsJni.get().isDeprecationTrialDisabled());
    }
}
