// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Integration test for the Password Check component, testing the interaction between sub-components
 * of the password check feature as well as the creation and destruction of the component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordCheckIntegrationTest {
    @Rule
    public final SettingsActivityTestRule<PasswordCheckFragmentView> mTestRule =
            new SettingsActivityTestRule<>(PasswordCheckFragmentView.class);

    @Rule public final JniMocker mJniMocker = new JniMocker();

    @Mock private PasswordCheckBridge.Natives mPasswordCheckBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PasswordCheckBridgeJni.TEST_HOOKS, mPasswordCheckBridge);
    }

    @Test
    @MediumTest
    public void testDestroysComponentIfFirstInSettingsStack() {
        ThreadUtils.runOnUiThreadBlocking(() -> PasswordCheckFactory.getOrCreate());
        Activity activity = setUpUiLaunchedFromDialog();
        activity.finish();
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        assertNull(PasswordCheckFactory.getPasswordCheckInstance());
    }

    @Test
    @MediumTest
    public void testDoesNotDestroyComponentIfNotFirstInSettingsStack() {
        ThreadUtils.runOnUiThreadBlocking(() -> PasswordCheckFactory.getOrCreate());
        Activity activity = setUpUiLaunchedFromSettings();
        activity.finish();
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        assertNotNull(PasswordCheckFactory.getPasswordCheckInstance());
        // Clean up the password check component.
        ThreadUtils.runOnUiThreadBlocking(PasswordCheckFactory::destroy);
    }

    private Activity setUpUiLaunchedFromSettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                PasswordCheckFragmentView.PASSWORD_CHECK_REFERRER,
                PasswordCheckReferrer.PASSWORD_SETTINGS);
        return mTestRule.startSettingsActivity(fragmentArgs);
    }

    private Activity setUpUiLaunchedFromDialog() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                PasswordCheckFragmentView.PASSWORD_CHECK_REFERRER,
                PasswordCheckReferrer.LEAK_DIALOG);
        return mTestRule.startSettingsActivity(fragmentArgs);
    }
}
