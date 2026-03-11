// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.provider.Settings;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.PasswordEchoSettingDelegate;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.embedder_support.util.PasswordEchoSettingState;
import org.chromium.components.user_prefs.UserPrefs;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordEchoSettingHandlerTest {
    private static class TestLegacyPasswordEchoSettingDelegate
            implements PasswordEchoSettingDelegate {
        private Runnable mCallback;

        @Override
        public void registerCallback(Runnable callback) {
            mCallback = callback;
        }

        @Override
        public boolean isPhysicalSettingEnabled() {
            return isLegacySettingEnabled();
        }

        @Override
        public boolean isTouchSettingEnabled() {
            return isLegacySettingEnabled();
        }

        private boolean isLegacySettingEnabled() {
            return Settings.System.getInt(
                            ContextUtils.getApplicationContext().getContentResolver(),
                            Settings.System.TEXT_SHOW_PASSWORD,
                            1)
                    == 1;
        }

        public void runCallback() {
            if (mCallback != null) mCallback.run();
        }
    }

    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private Profile mProfile;
    private PasswordEchoSettingHandler mPasswordEchoSettingHandler;
    private UiDevice mDevice;
    private String mInitialShowPasswordValue;
    private TestLegacyPasswordEchoSettingDelegate mTestDelegate;

    @Before
    public void setUp() throws ExecutionException, IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestDelegate = new TestLegacyPasswordEchoSettingDelegate();
                    PasswordEchoSettingState.setInstanceForTests(mTestDelegate);
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                    mPasswordEchoSettingHandler = new PasswordEchoSettingHandler(mProfile);
                });
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mInitialShowPasswordValue =
                mDevice.executeShellCommand("settings get system show_password").trim();
    }

    @After
    public void tearDown() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mPasswordEchoSettingHandler.destroy());
        if (mInitialShowPasswordValue.equals("null")) {
            mDevice.executeShellCommand("settings delete system show_password");
        } else {
            mDevice.executeShellCommand(
                    "settings put system show_password " + mInitialShowPasswordValue);
        }
    }

    private boolean isPasswordEchoPhysicalEnabledInPrefService() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(mProfile)
                                .getBoolean(Pref.WEB_KIT_PASSWORD_ECHO_ENABLED_PHYSICAL));
    }

    private boolean isPasswordEchoTouchEnabledInPrefService() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).getBoolean(Pref.WEB_KIT_PASSWORD_ECHO_ENABLED_TOUCH));
    }

    private void setSystemPasswordEchoState(boolean enabled)
            throws ExecutionException, IOException {
        mDevice.executeShellCommand("settings put system show_password " + (enabled ? "1" : "0"));
        ThreadUtils.runOnUiThreadBlocking(() -> mTestDelegate.runCallback());
    }

    // The physical and touch preference must hold the same value when setting split is disabled.
    @Test
    @SmallTest
    public void testPhysicalAndTouchPreferencesAreEqual() throws ExecutionException, IOException {
        boolean[] systemSettingsStates = {
            false, true, false,
        };

        for (boolean systemEnabled : systemSettingsStates) {
            setSystemPasswordEchoState(systemEnabled);
            Assert.assertEquals(
                    isPasswordEchoPhysicalEnabledInPrefService(),
                    isPasswordEchoTouchEnabledInPrefService());
        }
    }

    private boolean isPasswordEchoEnabledInSystemSettings() {
        return mTestDelegate.isLegacySettingEnabled();
    }

    @Test
    @SmallTest
    public void testInvokingUpdateMethodSyncsInitialState() throws ExecutionException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mPasswordEchoSettingHandler.updatePasswordEchoState());
        Assert.assertEquals(
                isPasswordEchoPhysicalEnabledInPrefService(),
                isPasswordEchoEnabledInSystemSettings());
    }

    @Test
    @SmallTest
    public void testSettingChangeIsObserved() throws ExecutionException, IOException {
        boolean[] systemSettingsStates = {
            false, true, false,
        };

        for (boolean systemEnabled : systemSettingsStates) {
            setSystemPasswordEchoState(systemEnabled);
            Assert.assertEquals(
                    isPasswordEchoPhysicalEnabledInPrefService(),
                    isPasswordEchoEnabledInSystemSettings());
        }
    }

    // If the setting was never set by the user, the system should consider the setting as enabled.
    @Test
    @SmallTest
    public void testSettingConsideredEnabledIfNeverSet() throws ExecutionException, IOException {
        // Clear the setting from the device to test default behavior.
        if (!mInitialShowPasswordValue.equals("null")) {
            mDevice.executeShellCommand("settings delete system show_password");
        }

        ThreadUtils.runOnUiThreadBlocking(() -> mTestDelegate.runCallback());

        Assert.assertTrue(isPasswordEchoEnabledInSystemSettings());
        Assert.assertTrue(isPasswordEchoPhysicalEnabledInPrefService());
        Assert.assertTrue(isPasswordEchoTouchEnabledInPrefService());
    }
}
